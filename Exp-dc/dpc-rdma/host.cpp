#include <cstring>
#include <ctime>
#include <cstdlib>

#include <thread>

#include "RDMA.h"
#include "DataCol.hpp"
#include "FlowKey.hpp"
#include "CountMin.hpp"
#include "NetworkStream.hpp"

constexpr int N = 65536;

enum DCReqOp {
  START = 0b00000011,
  COL   = 0b00000001
};

CountMin<FlowKey, N, CM_NUM, MAX_CM_NUM> cm;

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

  RDMA rdma(rdmaDev, hostIp, cm.data, sizeof(cm.data));

  printf("dst_qp=0x%x, rkey=0x%x, vaddr_base=%p\n", rdma.getQPN(), rdma.getRKey(), cm.data);
  puts("init");

  struct InitMsg {
    uint32_t qpn;
    uint32_t rkey;
    uint64_t msgBuf;
  } __attribute__((packed));
  InitMsg msg{htonl(rdma.getQPN()), htonl(rdma.getRKey()), htonll(reinterpret_cast<uint64_t>(cm.data))};
  ns.writeAll(&msg, sizeof(msg));
  ns.readChar();

  DataCol<uint8_t> dc(dcPortId, hostIp);

  char line[100];
  while (fgets(line, sizeof(line), stdin)) {
    uint32_t num = 16;
    sscanf(line, "%u", &num);

    timespec tStart, tEndRecv, tEnd;

    rdma.postRecv(1, nullptr, 0);

    auto keyBuf = readKeys(injectKeyBufPath);
    size_t nKey = keyBuf.size();
    std::vector<uint16_t> data(nKey);

    clock_gettime(CLOCK_REALTIME, &tStart);

    dc.sendReq(DCReqOp::START);
    for (int i = 1; i < num; i++)
      dc.sendReq(DCReqOp::COL);

    ibv_wc wc;
    rdma.pollCQ(&wc);
    // printf("%u\n", ntohl(wc.imm_data));

    clock_gettime(CLOCK_REALTIME, &tEndRecv);

    for (size_t i = 0; i < nKey; i++)
      data[i] = cm.query(keyBuf[i]);

    clock_gettime(CLOCK_REALTIME, &tEnd);

    if (FILE *fp = fopen(resultPath, "w")) {
      for (size_t i = 0; i < nKey; i++) {
        fprintf(fp, "%08x:%u -> %08x:%u (%hhu) : %hu\n",
          keyBuf[i].srcIp, ntohs(keyBuf[i].srcPort), keyBuf[i].dstIp, ntohs(keyBuf[i].dstPort),
          keyBuf[i].protocol, data[i]);
      }
      fclose(fp);
    } else {
      perror("fopen");
    }

    double tRcv = static_cast<double>(tEndRecv.tv_sec - tStart.tv_sec) +
                static_cast<double>(tEndRecv.tv_nsec - tStart.tv_nsec) * 1e-9;
    double tQry = static_cast<double>(tEnd.tv_sec - tEndRecv.tv_sec) +
                static_cast<double>(tEnd.tv_nsec - tEndRecv.tv_nsec) * 1e-9;
    printf("done %f (s), and %f (s) for query\n", tRcv, tQry);
  }

  return 0;
}
