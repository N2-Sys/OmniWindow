#include <cstring>
#include <ctime>
#include <cstdlib>

#include <thread>

#include "DataCol.hpp"
#include "FlowKey.hpp"

enum DCReqOp {
  COL     = 0b00000100,
  QRY_KEY = 0b00000010
};

struct DCReqData {
  uint8_t op;
  FlowKey key;
  uint32_t rem;
} __attribute__((packed));

struct DCRespData {
  uint32_t rem;
  uint16_t data;
  FlowKey key;
} __attribute__((packed));

constexpr uint32_t NKEYS_DP = 32 << 10;
constexpr uint32_t MAX_NKEYS = 1 << 17;

alignas(4096)
DCRespData resp[MAX_NKEYS];

int main(int argc, char **argv) {
  int dcPortId;
  if (argc != 5 || sscanf(argv[1], "%d", &dcPortId) != 1) {
    fprintf(stderr, "usage: %s <dpdk-port-id> <host-ip> <input-keys> <output>\n", argv[0]);
    return 1;
  }

  char *hostIp = argv[2];
  char *injectKeyBufPath = argv[3];
  char *resultPath = argv[4];

  DataCol<DCReqData> dc(dcPortId, hostIp);
  puts("init");

  char line[100];
  while (fgets(line, sizeof(line), stdin)) {
    uint32_t num = 3;
    sscanf(line, "%u", &num);

    timespec tStart, tEnd;

    auto keyBuf = readKeys(injectKeyBufPath);
    uint32_t nInjectKeys = keyBuf.size() <= NKEYS_DP ? 0 : keyBuf.size() - NKEYS_DP;

    std::vector<DCReqData> pkts(nInjectKeys);
    for (uint32_t i = NKEYS_DP; i < keyBuf.size(); i++)
      pkts[i - NKEYS_DP] = {DCReqOp::QRY_KEY, keyBuf[i], htonl(i)};

    clock_gettime(CLOCK_REALTIME, &tStart);

    constexpr uint32_t M = 512;
    uint32_t remInjectKeys = nInjectKeys;
    for (uint32_t i = 0; i < 2 * M && remInjectKeys; i++) {
      dc.sendReq(pkts[--remInjectKeys], false);
    }
    dc.flush();
    uint32_t nextInject = htonl(NKEYS_DP + remInjectKeys + M);
    for (uint32_t i = 0; i < num; i++)
      dc.sendReq({DCReqOp::COL, {}});
    // puts("sent");

    uint32_t nKeys = 0;
    uint32_t barrier = 2;

    while (1) {
      packet_data_t pktData;
      dc.recv(&pktData);
      uint32_t p = nKeys++;
      resp[p] = *reinterpret_cast<DCRespData *>(reinterpret_cast<DCRespPkt *>(pktData.data)->data);
      DPDKToolchainCpp::freePacket(&pktData);
      if (p % 10000 == 0)
        fprintf(stderr, "recv: %u\n", p);
      // fprintf(stderr, "%u\n", ntohl(resp[nKeys - 1].rem));
      if (resp[p].rem == nextInject) {
        for (uint32_t i = 0; i < M && remInjectKeys; i++) {
          dc.sendReq(pkts[--remInjectKeys], false);
        }
        dc.flush();
        nextInject = htonl(NKEYS_DP + remInjectKeys + M);
      }
      if (resp[p].rem == 0 || ntohl(resp[p].rem) == NKEYS_DP) {
        if (--barrier == 0)
          break;
      }
    }

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

    printf("done (%d keys) %fs\n", nKeys, tu);
  }

  return 0;
}
