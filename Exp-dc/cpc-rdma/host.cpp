#include <cstring>
#include <ctime>
#include <cstdlib>

#include <thread>

#include <arpa/inet.h>

#include "RDMA.h"
#include "DataCol.hpp"
#include "FlowKey.hpp"
#include "NetworkStream.hpp"

#include <unistd.h>

struct DCReqData {
  uint64_t vaddr;
  uint32_t rem;
  FlowKey key;
} __attribute__((packed));

struct DCRespData {
  FlowKey key;
  uint16_t data;
} __attribute__((packed));

constexpr int MAX_NKEY = 1 << 20;

alignas(4096)
uint32_t data[MAX_NKEY];

inline uint64_t htonll(uint64_t x) {
  static_assert(__BYTE_ORDER == __LITTLE_ENDIAN);
  return (uint64_t)htonl(x & 0xffffffffU) << 32 | htonl(x >> 32);
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

  RDMA rdma(rdmaDev, hostIp, data, sizeof(data));
  printf("dst_qp=0x%x, rkey=0x%x, vaddr_base=%p\n", rdma.getQPN(), rdma.getRKey(), data);
  puts("init");

  struct InitMsg {
    uint32_t qpn;
    uint32_t rkey;
    uint64_t msgBuf;
  } __attribute__((packed));
  InitMsg msg{htonl(rdma.getQPN()), htonl(rdma.getRKey()), htonll(reinterpret_cast<uint64_t>(data))};
  ns.writeAll(&msg, sizeof(msg));
  ns.readChar();

  DataCol<DCReqData> dc(dcPortId, hostIp);

  char line[100];
  while (fgets(line, sizeof(line), stdin)) {
    timespec tStart, tEnd;

    rdma.postRecv(1, nullptr, 0);

    auto keyBuf = readKeys(injectKeyBufPath);
    size_t nKey = keyBuf.size();
    if (nKey > MAX_NKEY)
      throw std::out_of_range("too many keys");

    std::vector<DCReqData> dcReqs(nKey);
    for (size_t i = 0; i < nKey; i++)
      dcReqs[i] = {
        htonll(reinterpret_cast<uint64_t>(&data[i])),
        htonl(nKey - i - 1), keyBuf[i]
      };

    clock_gettime(CLOCK_REALTIME, &tStart);

    if (nKey) {
      for (size_t i = 0; i < nKey; i++)
        dc.sendReq(dcReqs[i]);
      dc.flush();

      ibv_wc wc;
      rdma.pollCQ(&wc);
      // printf("%u\n", ntohl(wc.imm_data));
    }

    clock_gettime(CLOCK_REALTIME, &tEnd);

    double tu = static_cast<double>(tEnd.tv_sec - tStart.tv_sec) +
                static_cast<double>(tEnd.tv_nsec - tStart.tv_nsec) * 1e-9;

    if (FILE *fp = fopen(resultPath, "w")) {
      for (size_t i = 0; i < nKey; i++) {
        fprintf(fp, "%08x:%u -> %08x:%u (%hhu) : %u\n",
          keyBuf[i].srcIp, ntohs(keyBuf[i].srcPort), keyBuf[i].dstIp, ntohs(keyBuf[i].dstPort),
          keyBuf[i].protocol, data[i]);
      }
      fclose(fp);
    } else {
      perror("fopen");
    }

    printf("done %fs\n", tu);
  }

  return 0;
}
