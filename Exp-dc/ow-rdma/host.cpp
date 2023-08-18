#include <cstring>
#include <ctime>
#include <cstdlib>

#include <thread>

#include "DataCol.hpp"
#include "FlowKey.hpp"
#include "RDMA.h"
#include "NetworkStream.hpp"

enum DCReqOp : uint8_t {
  COL            = 0b00000100,
  QRY_KEY        = 0b00000010,
  QRY_KEY_NOTIFY = 0b00001010
};

struct DCReqData {
  uint8_t op;
  FlowKey key;
} __attribute__((packed));

struct DCRespData {
  FlowKey key;
  uint8_t padding;
  uint16_t data;
} __attribute__((packed));

constexpr uint32_t NKEYS_DP = 32 << 10;
constexpr uint32_t MAX_NKEYS = 1 << 17;

alignas(4096)
DCRespData resp[MAX_NKEYS];

inline uint64_t htonll(uint64_t x) {
  static_assert(__BYTE_ORDER == __LITTLE_ENDIAN);
  return static_cast<uint64_t>(htonl(x & 0xffffffffU)) << 32 | htonl(x >> 32);
}

int main(int argc, char **argv) {
  int dcPortId;
  if (argc != 7 || sscanf(argv[1], "%d", &dcPortId) != 1) {
    fprintf(stderr, "usage: %s <dpdk-port-id> <host-ip> <rdma-dev> <switch-ctrl-ip> <input-keys> <output>\n", argv[0]);
    return 1;
  }

  char *hostIp = argv[2];
  char *rdmaDev = argv[3];
  char *switchCtrlIp = argv[4];
  char *injectKeyBufPath = argv[5];
  char *resultPath = argv[6];

  NetworkStream ns;
  ns.connect(switchCtrlIp, 33668);

  RDMA rdma(rdmaDev, hostIp, resp, sizeof(resp));

  printf("dst_qp=0x%x, rkey=0x%x, vaddr_base=%p\n", rdma.getQPN(), rdma.getRKey(), resp);
  puts("init");

  struct InitMsg {
    uint32_t qpn;
    uint32_t rkey;
    uint64_t msgBuf;
  } __attribute__((packed));
  InitMsg msg{htonl(rdma.getQPN()), htonl(rdma.getRKey()), htonll(reinterpret_cast<uint64_t>(resp))};
  ns.writeAll(&msg, sizeof(msg));
  ns.readChar();

  DataCol<DCReqData> dc(dcPortId, hostIp);

  char line[100];
  while (fgets(line, sizeof(line), stdin)) {
    uint32_t num = 16;
    sscanf(line, "%u", &num);

    timespec tStart, tEnd;

    rdma.postRecv(1, nullptr, 0);
    rdma.postRecv(1, nullptr, 0);

    auto keyBuf = readKeys(injectKeyBufPath);
    uint32_t nInjectKeys = keyBuf.size() <= NKEYS_DP ? 0 : keyBuf.size() - NKEYS_DP;

    std::vector<DCReqData> pkts(nInjectKeys);
    for (uint32_t i = NKEYS_DP; i < keyBuf.size(); i++) {
      pkts[i - NKEYS_DP] = {
        i + 1 == keyBuf.size() ? DCReqOp::QRY_KEY_NOTIFY : DCReqOp::QRY_KEY,
        keyBuf[i]
      };
    }

    clock_gettime(CLOCK_REALTIME, &tStart);

    for (uint32_t i = 0; i < num; i++)
      dc.sendReq({DCReqOp::COL, {}});
    for (uint32_t i = 0; i < nInjectKeys; i++)
      dc.sendReq(pkts[i], false);
    dc.flush();
    // puts("sent");

    ibv_wc wc;
    rdma.pollCQ(&wc);
    rdma.pollCQ(&wc);
    uint32_t nKeys = ntohl(wc.imm_data) / sizeof(DCRespData);
    // printf("%u\n", ntohl(wc.imm_data));

    clock_gettime(CLOCK_REALTIME, &tEnd);
    double tu = static_cast<double>(tEnd.tv_sec - tStart.tv_sec) +
                static_cast<double>(tEnd.tv_nsec - tStart.tv_nsec) * 1e-9;

    if (FILE *fp = fopen(resultPath, "w")) {
      for (int i = nKeys - 1; i >= 0; i--) {
        fprintf(fp, "%08x:%u -> %08x:%u (%hhu) : %hu\n",
          resp[i].key.srcIp, ntohs(resp[i].key.srcPort), resp[i].key.dstIp, ntohs(resp[i].key.dstPort),
          resp[i].key.protocol, ntohs(resp[i].data));
      }
      fclose(fp);
    } else {
      perror("fopen");
    }

    printf("done (%u keys) %fs\n", nKeys, tu);
  }

  return 0;
}
