#include <cstring>
#include <ctime>
#include <cstdlib>

#include "DataCol.hpp"

int main(int argc, char **argv) {
  int dcPortId;
  if (argc != 3 || sscanf(argv[1], "%d", &dcPortId) != 1) {
    fprintf(stderr, "usage: %s <dpdk-port-id> <host-ip>\n", argv[0]);
    return 1;
  }

  char *hostIp = argv[2];
  char *injectKeyBufPath = argv[3];
  char *resultPath = argv[4];

  DataCol dc(dcPortId, hostIp);
  puts("init");

  char line[100];
  while (fgets(line, sizeof(line), stdin)) {
    uint32_t num = 16;
    sscanf(line, "%u", &num);

    timespec tStart, tEnd;

    clock_gettime(CLOCK_REALTIME, &tStart);
    for (uint16_t i = 0; i < num; i++)
      dc.sendReq(htons(i));

    packet_data_t pktData;
    dc.recv(&pktData);
    clock_gettime(CLOCK_REALTIME, &tEnd);
    double tu = static_cast<double>(tEnd.tv_sec - tStart.tv_sec) +
                static_cast<double>(tEnd.tv_nsec - tStart.tv_nsec) * 1e-9;
    printf("done %f (s)\n", tu);
    DPDKToolchainCpp::freePacket(&pktData);
  }

  return 0;
}
