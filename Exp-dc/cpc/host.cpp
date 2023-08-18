#include <cstring>
#include <ctime>
#include <cstdlib>

#include <thread>

#include "DataCol.hpp"
#include "FlowKey.hpp"

struct DCRespData {
  FlowKey key;
  uint16_t data;
} __attribute__((packed));

int main(int argc, char **argv) {
  int dcPortId;
  if (argc != 5 || sscanf(argv[1], "%d", &dcPortId) != 1) {
    fprintf(stderr, "usage: %s <dpdk-port-id> <host-ip> <input-keys> <output>\n", argv[0]);
    return 1;
  }

  char *hostIp = argv[2];
  char *injectKeyBufPath = argv[3];
  char *resultPath = argv[4];

  DataCol<FlowKey> dc(dcPortId, hostIp);
  puts("init");

  char line[100];
  while (fgets(line, sizeof(line), stdin)) {
    timespec tStart, tEnd;

    auto keyBuf = readKeys(injectKeyBufPath);
    size_t nKey = keyBuf.size();
    std::vector<uint16_t> data(nKey);

    clock_gettime(CLOCK_REALTIME, &tStart);

    constexpr int M = 4096;
    for (size_t i = 0; i < 2 * M; i++)
      dc.sendReq(keyBuf[i]);
    dc.flush();

    for (size_t b = 0; b < nKey; b += M) {
      for (size_t i = b; i < b + M && i < nKey; i++) {
        packet_data_t pktData;
        dc.recv(&pktData);

        auto *resp = reinterpret_cast<DCRespData *>(reinterpret_cast<DCRespPkt *>(pktData.data)->data);
        // printf("recv: %hu\n", ntohs(resp->idx));
        if (memcmp(&resp->key, &keyBuf[i], sizeof(FlowKey)) != 0)
          throw std::logic_error("key mismatch " + std::to_string(i));

        data[i] = ntohs(resp->data);

        DPDKToolchainCpp::freePacket(&pktData);
      }
      for (size_t i = b + 2 * M; i < b + 3 * M && i < nKey; i++)
        dc.sendReq(keyBuf[i], false);
      dc.flush();
    }

    clock_gettime(CLOCK_REALTIME, &tEnd);
    double tu = static_cast<double>(tEnd.tv_sec - tStart.tv_sec) +
                static_cast<double>(tEnd.tv_nsec - tStart.tv_nsec) * 1e-9;

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

    printf("done %fs\n", tu);
  }

  return 0;
}
