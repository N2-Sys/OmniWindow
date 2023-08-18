#include <cstring>
#include <ctime>
#include <cstdlib>

#include <thread>

#include "DataCol.hpp"
#include "FlowKey.hpp"
#include "CountMin.hpp"

constexpr int N = 65536;

enum DCReqOp {
  START = 0b00000011,
  COL   = 0b00000001
};

struct DCRespData {
  uint16_t idx;
  uint16_t data[CM_NUM];
} __attribute__((packed));

CountMin<FlowKey, N, CM_NUM, MAX_CM_NUM> cm;

int main(int argc, char **argv) {
  int dcPortId;
  if (argc != 5 || sscanf(argv[1], "%d", &dcPortId) != 1) {
    fprintf(stderr, "usage: %s <dpdk-port-id> <host-ip> <input-keys> <output>\n", argv[0]);
    return 1;
  }

  char *hostIp = argv[2];
  char *injectKeyBufPath = argv[3];
  char *resultPath = argv[4];

  DataCol<uint8_t> dc(dcPortId, hostIp);
  puts("init");

  char line[100];
  while (fgets(line, sizeof(line), stdin)) {
    uint32_t num = 3;
    sscanf(line, "%u", &num);

    timespec tStart, tEndRecv, tEnd;

    auto keyBuf = readKeys(injectKeyBufPath);
    size_t nKey = keyBuf.size();
    std::vector<uint16_t> data(nKey);

    clock_gettime(CLOCK_REALTIME, &tStart);

    dc.sendReq(DCReqOp::START);
    for (int i = 1; i < num; i++)
      dc.sendReq(DCReqOp::COL);
    for (int i = 0; i < N; i++) {
      packet_data_t pktData;
      dc.recv(&pktData);

      auto *resp = reinterpret_cast<DCRespData *>(reinterpret_cast<DCRespPkt *>(pktData.data)->data);
      // printf("recv: %hu\n", ntohs(resp->idx));
      if (ntohs(resp->idx) != i)
        throw std::logic_error("idx mismatch " + std::to_string(i) + " " + std::to_string(ntohs(resp->idx)));

      memcpy(cm.data[i], resp->data, sizeof(resp->data));

      DPDKToolchainCpp::freePacket(&pktData);
    }

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
