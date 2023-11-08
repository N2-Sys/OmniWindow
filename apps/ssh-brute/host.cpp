#include "RDMA.h"
#include "HashTable.hpp"
#include "DataCol.hpp"
#include "NetworkStream.hpp"

#include <cstdlib>
#include <algorithm>

#include <immintrin.h>

static double tsDiff(timespec end, timespec start) {
  return (double)(end.tv_sec - start.tv_sec) + 1e-9 * (double)(end.tv_nsec - start.tv_nsec);
}

#if !TUMBLING && !SLIDING
#warning "unspecified window type, using tumbling"
#define TUMBLING
#endif

class Server {
  struct Key {
    uint32_t dst_ip;
    uint32_t pkt_size;
  } __attribute__((packed));
  // using Key = uint32_t;
  using Val = uint32_t;

  enum DCReqOp : uint8_t {
    START     = 0b00001001,
    COL       = 0b00001000,
    QRY_KEY   = 0b01001010,
    RST_START = 0b10001100,
    RST       = 0b00001100,
    PUSH      = 0b00011100
  };

  enum DCReqOpH : uint8_t {
    RST_DISTINCT = 0b00000001,
  };

  enum DCRespOp {
    DONE = 0,
    KEY = 1,
    KEYVAL = 2
  };

  struct DCReqData {
    uint8_t oph;
    uint8_t op;
    Key key;
    uint64_t vaddr;
  } __attribute__((packed));

  struct DCRespData {
    uint32_t ipadding;
    uint8_t op;
    uint8_t padding;
    uint16_t val;
    Key key;
  } __attribute__((packed));

  static constexpr uint32_t KEY_LEN = sizeof(Key);

  static constexpr uint32_t RDMA_MAX_RECV = 8192;
  static constexpr uint32_t MSG_BUF_SIZE = 4 << 23;

  static constexpr uint32_t N = 1 << 19;
#ifdef SLIDING
  static constexpr uint32_t NEPOCH_BUF = 8;
  static constexpr uint32_t MEM_LEN = MSG_BUF_SIZE + sizeof(Val) * N * (NEPOCH_BUF + 1);
#else
  static constexpr uint32_t MEM_LEN = MSG_BUF_SIZE + sizeof(Val) * N * 2;
#endif

  static inline uint64_t htonll(uint64_t x) {
    static_assert(__BYTE_ORDER == __LITTLE_ENDIAN);
    return static_cast<uint64_t>(htonl(x & 0xffffffffU)) << 32 | htonl(x >> 32);
  }

  alignas(4096)
  char mem[MEM_LEN];

  alignas(64)
  Key keys[N], abnormalKeys[N];
#ifdef SLIDING
  alignas(64)
  Key delKeys[N];

  alignas(64)
  uint16_t valid[N / 16], pin[N / 16];
#endif

  char *const msgBuf = mem;
#ifdef SLIDING
  Val (*const epochData)[N] = reinterpret_cast<Val (*)[N]>(mem + MSG_BUF_SIZE);
  Val *const mergedData = reinterpret_cast<Val *>(mem + MSG_BUF_SIZE + sizeof(Val) * N * NEPOCH_BUF);
#else
  Val *const epochData = reinterpret_cast<Val *>(mem + MSG_BUF_SIZE);
  Val *const mergedData = epochData + N;
#endif

#ifdef SLIDING
  uint32_t curEpochPos;
#endif
  uint32_t curEpoch, nEpoch;
  uint32_t threshold;

#ifndef SLIDING
  uint32_t nKeys;
#endif

  uint32_t nFlush;
  uint32_t msgHead, msgTail;

  DataCol<DCReqData> dc;
  HashTable h;
  RDMA rdma;

  std::vector<DCReqData> overflowKeys;

  void waitNewMsg() {
    ibv_wc wc;
    rdma.pollCQ(&wc);
    msgTail = (ntohl(wc.imm_data) + sizeof(DCRespData)) % MSG_BUF_SIZE;
    rdma.postRecv(1, nullptr, 0);
    nFlush++;
  }

  DCRespData nextMsg() {
    if (msgHead == msgTail)
      waitNewMsg();
    uint32_t p = msgHead;
    msgHead = (msgHead + sizeof(DCRespData)) % MSG_BUF_SIZE;
    return *reinterpret_cast<DCRespData *>(msgBuf + p);
  }

  uint32_t insKey(Key key) {
#ifdef SLIDING
    uint32_t pos = h.addKey(&key);
    valid[pos / 16] |= 1U << (pos & 15);
    keys[pos] = key;
#else
    uint32_t pos = h.addKey(&key);
    assert(pos <= nKeys);
    if (pos == nKeys) {
      keys[pos] = key;
      nKeys++;
    }
#endif
    return pos;
  }

  void recvKeys() {
    while (1) {
      auto msg = nextMsg();
      if (msg.op == DCRespOp::DONE)
        break;
      assert(msg.op == DCRespOp::KEY);

      uint32_t pos = insKey(msg.key);

      overflowKeys.push_back({
        0,
        DCReqOp::QRY_KEY,
        msg.key,
#ifdef SLIDING
        htonll(reinterpret_cast<uint64_t>(&epochData[curEpochPos][pos]))
#else
        htonll(reinterpret_cast<uint64_t>(&epochData[pos]))
#endif
      });
    }
  }

  void recvKeyVals() {
    while (1) {
      auto msg = nextMsg();
      if (msg.op == DCRespOp::DONE)
        break;
      assert(msg.op == DCRespOp::KEYVAL);

      uint32_t pos = insKey(msg.key);

#ifdef SLIDING
      epochData[curEpochPos][pos] = msg.val;
#else
      epochData[pos] = msg.val;
#endif
    }
  }

  void recvDone() {
    auto msg = nextMsg();
    assert(msg.op == DCRespOp::DONE);
  }

public:
  Server(uint32_t nEpoch_, uint32_t threshold_, int dcPortId,
         const char *dcSrcIp, const char *rdmaDevName, NetworkStream &ns) :
      mem{},
#ifdef SLIDING
      valid{}, pin{},
#endif
      curEpoch(0), nEpoch(nEpoch_), threshold(threshold_),
#ifndef SLIDING
      nKeys(0),
#endif
      msgHead(0), msgTail(0),
      dc(dcPortId, dcSrcIp), h(N, KEY_LEN), rdma(rdmaDevName, dcSrcIp, mem, MEM_LEN) {
    for (uint32_t i = 0; i < RDMA_MAX_RECV; i++)
      rdma.postRecv(1, nullptr, 0);
    fprintf(stderr, "Init\n");
    fprintf(stderr, "dst_qp=0x%x, rkey=0x%x, msg_vaddr_base=%p\n", rdma.getQPN(), rdma.getRKey(), msgBuf);

    struct InitMsg {
      uint32_t qpn;
      uint32_t rkey;
      uint64_t msgBuf;
    } __attribute__((packed));
    InitMsg msg{htonl(rdma.getQPN()), htonl(rdma.getRKey()), htonll(reinterpret_cast<uint64_t>(msgBuf))};
    ns.writeAll(&msg, sizeof(msg));
    ns.readChar();
  }

  void switchWin(uint16_t nReq = 1) {

#ifdef PROC_OVERLAP
    timespec tStart, tEndIns, tEndMerge, tEndReset;
#else
    timespec tStart, tEndRecv, tEndIns, tEndMerge, tEndCmp, tEndReset;
#ifdef SLIDING
    timespec tEndSub, tEndFindDel, tEndDel;
#endif
#endif

    uint8_t oph = 0;
    if (curEpoch == nEpoch - 1)
      oph |= DCReqOpH::RST_DISTINCT;

    overflowKeys.clear();
    dc.sendReq({oph, DCReqOp::PUSH, {}});
    recvKeys();
    fprintf(stderr, "%zu overflow keys\n", overflowKeys.size());

    uint32_t msgHeadOld = msgHead;
    nFlush = 0;

    clock_gettime(CLOCK_REALTIME, &tStart);

    dc.sendReq({oph, DCReqOp::START, {}});
    for (uint16_t i = 1; i < nReq; i++)
      dc.sendReq({oph, DCReqOp::COL, {}});
    for (const auto &req: overflowKeys)
      dc.sendReq(req, false);
    dc.sendReq({oph, DCReqOp::RST_START, {}});
    for (uint16_t i = 1; i < nReq; i++)
      dc.sendReq({oph, DCReqOp::RST, {}});

#ifndef PROC_OVERLAP
    waitNewMsg();
    clock_gettime(CLOCK_REALTIME, &tEndRecv);
#endif

    recvKeyVals();
    clock_gettime(CLOCK_REALTIME, &tEndIns);

    uint32_t nAbnormal = 0;

#ifdef SLIDING
    uint32_t nDel = 0;

#ifndef PROC_OVERLAP

    for (uint32_t i = 0; i < N; i += 512 / 32) {
      __m512i x, y;
      x = _mm512_load_epi32(mergedData + i);
      y = _mm512_load_epi32(epochData[curEpochPos] + i);
      x = _mm512_add_epi32(x, y);
      _mm512_store_epi32(mergedData + i, x);
    }
    clock_gettime(CLOCK_REALTIME, &tEndMerge);

    __m512i vThres = _mm512_set1_epi32(threshold);
    for (uint32_t i = 0; i < N; i += 512 / 32) {
      __m512i x, vKeys0, vKeys1;
      __mmask16 mAbnormal;
      vKeys0 = _mm512_load_epi64(keys + i);
      vKeys1 = _mm512_load_epi64(keys + i + 512 / 64);
      x = _mm512_load_epi32(mergedData + i);
      mAbnormal = _mm512_cmpge_epu32_mask(x, vThres);

      _mm512_mask_compressstoreu_epi64(abnormalKeys + nAbnormal, mAbnormal & 0xFF, vKeys0);
      nAbnormal += _popcnt32(mAbnormal & 0xFF);
      _mm512_mask_compressstoreu_epi64(abnormalKeys + nAbnormal, mAbnormal >> 8, vKeys1);
      nAbnormal += _popcnt32(mAbnormal >> 8);
    }
    clock_gettime(CLOCK_REALTIME, &tEndCmp);

    uint32_t delEpochPos = (curEpochPos + (NEPOCH_BUF - (nEpoch - 1))) % NEPOCH_BUF;
    for (uint32_t i = 0; i < N; i += 512 / 32) {
      __m512i x, y;
      x = _mm512_load_epi32(mergedData + i);
      y = _mm512_load_epi32(epochData[delEpochPos] + i);
      x = _mm512_sub_epi32(x, y);
      _mm512_store_epi32(mergedData + i, x);
    }
    clock_gettime(CLOCK_REALTIME, &tEndSub);

    __m512i vZero = _mm512_setzero_epi32();
    for (uint32_t i = 0; i < N; i += 512 / 32) {
      __m512i x, vKeys0, vKeys1;
      __mmask16 mDel;
      vKeys0 = _mm512_load_epi64(keys + i);
      vKeys1 = _mm512_load_epi64(keys + i + 512 / 64);
      x = _mm512_load_epi32(mergedData + i);
      mDel = _mm512_mask_cmpeq_epi32_mask(valid[i / 16] & ~pin[i / 16], x, vZero);
      _mm512_mask_compressstoreu_epi64(delKeys + nDel, mDel & 0xFF, vKeys0);
      nDel += _popcnt32(mDel & 0xFF);
      _mm512_mask_compressstoreu_epi64(delKeys + nDel, mDel >> 8, vKeys1);
      nDel += _popcnt32(mDel >> 8);
      valid[i / 16] ^= mDel;
    }
    clock_gettime(CLOCK_REALTIME, &tEndFindDel);

#else
#error "unimplemented"
#endif

    for (uint32_t i = 0; i < nDel; i++)
      h.delKey(&delKeys[i]);
    clock_gettime(CLOCK_REALTIME, &tEndDel);

#else

#ifdef PROC_OVERLAP
    if (curEpoch != 0) {
#endif
      for (uint32_t i = 0; i < nKeys; i += 512 / 32) {
        __m512i x, y;
        x = _mm512_load_epi32(mergedData + i);
        y = _mm512_load_epi32(epochData + i);
        x = _mm512_add_epi32(x, y);
        _mm512_store_epi32(mergedData + i, x);
      }
      clock_gettime(CLOCK_REALTIME, &tEndMerge);
#ifdef PROC_OVERLAP
    } else {
      __m512i vThres = _mm512_set1_epi32(threshold);
      for (uint32_t i = 0; i < nKeys; i += 512 / 32) {
        __m512i x, vKeys0, vKeys1;
        __mmask16 mAbnormal;
        vKeys0 = _mm512_load_epi64(keys + i);
        vKeys1 = _mm512_load_epi64(keys + i + 512 / 64);
        x = _mm512_load_epi32(mergedData + i);
        mAbnormal = _mm512_cmpge_epu32_mask(x, vThres);

        _mm512_mask_compressstoreu_epi64(abnormalKeys + nAbnormal, mAbnormal & 0xFF, vKeys0);
        nAbnormal += _popcnt32(mAbnormal & 0xFF);
        _mm512_mask_compressstoreu_epi64(abnormalKeys + nAbnormal, mAbnormal >> 8, vKeys1);
        nAbnormal += _popcnt32(mAbnormal >> 8);
      }
      clock_gettime(CLOCK_REALTIME, &tEndMerge);
    }
#else
    if (curEpoch == nEpoch - 1) {
      __m512i vThres = _mm512_set1_epi32(threshold);
      for (uint32_t i = 0; i < nKeys; i += 512 / 32) {
        __m512i x, vKeys0, vKeys1;
        __mmask16 mAbnormal;
        vKeys0 = _mm512_load_epi64(keys + i);
        vKeys1 = _mm512_load_epi64(keys + i + 512 / 64);
        x = _mm512_load_epi32(mergedData + i);
        mAbnormal = _mm512_cmpge_epu32_mask(x, vThres);

        _mm512_mask_compressstoreu_epi64(abnormalKeys + nAbnormal, mAbnormal & 0xFF, vKeys0);
        nAbnormal += _popcnt32(mAbnormal & 0xFF);
        _mm512_mask_compressstoreu_epi64(abnormalKeys + nAbnormal, mAbnormal >> 8, vKeys1);
        nAbnormal += _popcnt32(mAbnormal >> 8);
      }
      clock_gettime(CLOCK_REALTIME, &tEndCmp);
    }
#endif

#endif

    recvDone();
    clock_gettime(CLOCK_REALTIME, &tEndReset);

    fprintf(stderr, "%zu messages received (%u Flushes)\n", (msgHead - msgHeadOld) / sizeof(DCRespData), nFlush);
#ifdef SLIDING
    uint32_t nKeys = 0;
    for (uint32_t i = 0; i < N / 16; i++)
      nKeys += _popcnt32(valid[i]);
#endif
    fprintf(stderr, "%u keys in total\n", nKeys);

#ifdef PROC_OVERLAP
    fprintf(stderr, "recv & ins: %fs, merge (& compare): %fs\n",
        tsDiff(tEndIns, tStart), tsDiff(tEndMerge, tEndIns));
#else
    fprintf(stderr, "recv: %fs, ins: %fs, merge: %fs\n",
        tsDiff(tEndRecv, tStart), tsDiff(tEndIns, tEndRecv), tsDiff(tEndMerge, tEndIns));
#endif

#ifndef SLIDING
    if (curEpoch == nEpoch - 1) {
#endif
      fprintf(stderr, "%u abnormals detected\n", nAbnormal);
#ifndef PROC_OVERLAP
      fprintf(stderr, "compare: %fs\n", tsDiff(tEndCmp, tEndMerge));
#endif
#ifndef SLIDING
    }
#endif

#ifdef SLIDING
    fprintf(stderr, "%u keys deleted (original %u)\n", nDel, nDel + nKeys);
#ifndef PROC_OVERLAP
    fprintf(stderr, "sub: %fs, find-del: %fs, del: %fs\n", tsDiff(tEndSub, tEndCmp), tsDiff(tEndFindDel, tEndSub), tsDiff(tEndDel, tEndFindDel));
#endif
#endif

#ifdef SLIDING
    memset(epochData[(curEpochPos + (NEPOCH_BUF - (nEpoch - 1))) % NEPOCH_BUF], 0, sizeof(Val) * N);
#else
    memset(epochData, 0, sizeof(Val) * nKeys);
#endif

    curEpoch = (curEpoch + 1) % nEpoch;
#ifdef SLIDING
    curEpochPos = (curEpochPos + 1) % NEPOCH_BUF;
#endif
  }

  void dump(FILE *fp = stderr) {
#ifdef SLIDING
    for (uint32_t i = 0; i < N; i++) if (valid[i / 16] >> (i & 15) & 1) {
#else
    for (uint32_t i = 0; i < nKeys; i++) {
#endif
      unsigned char *ip = reinterpret_cast<unsigned char *>(&keys[i]);
      fprintf(fp, "%hhu.%hhu.%hhu.%hhu: %u\n", ip[0], ip[1], ip[2], ip[3], mergedData[i]);
    }
  }

  void insKeys(const char *input_path, NetworkStream &ns) {
    FILE *fp_in = fopen(input_path, "r");
    if (!fp_in) {
      perror("fopen");
      return;
    }

    struct KeyData {
      uint32_t key;
      uint64_t pos;
    } __attribute__((packed));

    Key key;
    Val val;
    int rc;
    std::vector<KeyData> keys;
#ifdef SLIDING
    uint32_t nKeys = 0;
#endif
//     while ((rc = fscanf(fp_in, "%x%u", &key, &val)) == 2) {
//       key = htonl(key);
//       uint32_t pos = insKey(key);
// #ifdef SLIDING
//       pin[pos / 16] |= 1U << (pos & 15);
//       keys.push_back({key, htonll(reinterpret_cast<uint64_t>(&epochData[0][pos]))});
//       // fprintf(fp_out, "0x%x %p\n", ntohl(key), &epochData[0][pos]);
//       nKeys++;
// #else
//       keys.push_back({key, htonll(reinterpret_cast<uint64_t>(&epochData[pos]))});
//       // fprintf(fp_out, "0x%x %p\n", ntohl(key), &epochData[pos]);
// #endif
//     }
//    fprintf(stderr, "%u keys inserted\n", nKeys);

    uint32_t size = htonl(static_cast<uint32_t>(keys.size()));
    ns.writeAll(&size, sizeof(size));
    // ns.writeAll(keys.data(), sizeof(KeyData) * keys.size());
    ns.readChar();

    fclose(fp_in);
  }

//   void dumpHotKeys(const char *path, uint32_t m) {
//     FILE *fp = fopen(path, "w");
//     if (!fp) {
//       perror("fopen");
//       return;
//     }

// #ifdef SLIDING
//     std::vector<uint32_t> idx;
//     for (uint32_t i = 0; i < N; i++)
//       if (valid[i / 16] >> (i & 15) & 1)
//         idx.push_back(i);
// #else
//     std::vector<uint32_t> idx(nKeys);
//     for (uint32_t i = 0; i < nKeys; i++)
//       idx[i] = i;
// #endif
//     std::sort(idx.begin(), idx.end(), [this](uint32_t i, uint32_t j) -> bool {
//       return mergedData[i] > mergedData[j];
//     });
//     for (uint32_t i = 0; i < m && i < idx.size(); i++)
//       fprintf(fp, "0x%x %u\n", ntohl(keys[idx[i]]), mergedData[idx[i]]);
//     fclose(fp);
//   }
};

int main(int argc, char **argv) {
  int dcPortId;
  if (argc != 6 || sscanf(argv[1], "%d", &dcPortId) != 1) {
    fprintf(stderr, "usage: %s <dpdk-port-id> <host-ip> <rdma-dev> <switch-ctrl-ip> <input-keys>\n", argv[0]);
    return 1;
  }

  uint32_t nSubWins = 5;
  uint32_t threshold = 100;
  char *hostIp = argv[2];
  char *rdmaDev = argv[3];
  char *switchCtrlIp = argv[4];
  char *inputKeysFn = argv[5];

  NetworkStream ns;

  ns.connect(switchCtrlIp, 33668);

  Server *s = new Server(nSubWins, threshold, dcPortId, hostIp, rdmaDev, ns);
  s->insKeys(inputKeysFn, ns);

  char line[100];
  while (fgets(line, sizeof(line), stdin)) {
    int nReq = 16;
    sscanf(line, "%d", &nReq);
    s->switchWin(nReq);
    // s->dump();
  }

  delete s;

  return EXIT_SUCCESS;
}
