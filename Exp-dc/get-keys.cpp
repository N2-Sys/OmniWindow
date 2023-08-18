#include <cstdio>
#include <cassert>

#include <vector>
#include <unordered_set>

#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <pcap/pcap.h>

#include "FlowKey.hpp"
#include "CountMin.hpp"
#include <crcutil/crc32c_sse4.h>

static crcutil::Crc32cSSE4 crc32(true);

template<>
class std::hash<FlowKey> {
public:
  size_t operator ()(const FlowKey &k) const {
    return crc32.CrcDefault(&k, sizeof(FlowKey), 0);
  }
};

struct CapPacket {
  iphdr ip;
  union {
    tcphdr tcp;
    udphdr udp;
  };
} __attribute__((packed));

constexpr int MAX_DATA = 65536;

CountMin<FlowKey, 65536, CM_NUM, MAX_CM_NUM> cm;

struct SendPacket {
  ethhdr eth;
  u_char data[65536];
} sendPkt;

int main(int argc, char **argv) {
  long length_us;
  int win_num;
  if (argc != 5 || sscanf(argv[2], "%ld", &length_us) != 1 || sscanf(argv[3], "%d", &win_num) != 1) {
    fprintf(stderr, "usage: %s <trace-pcap> <length-us> <win-num> <result-path>\n", argv[0]);
    return 1;
  }

  char errbuf[PCAP_ERRBUF_SIZE];
  pcap_t *trace = pcap_open_offline(argv[1], errbuf);
  if (!trace) {
    fprintf(stderr, "pcap_open_offline: %s\n", errbuf);
    return 1;
  }

  int traceDL = pcap_datalink(trace);
  printf("datalink: %s (%s)\n", pcap_datalink_val_to_name(traceDL), pcap_datalink_val_to_description(traceDL));
  if (traceDL != DLT_RAW)
    throw std::runtime_error("invalid trace data link");

  bool started = false;
  timeval ts_start;

  std::unordered_set<FlowKey> h;
  std::vector<FlowKey> keyBuf;

  int nPkt = 0;

  sendPkt.eth.h_proto = htons(ETHERTYPE_IP);

  while (1) {
    pcap_pkthdr pkthdr;
    const u_char *buf = pcap_next(trace, &pkthdr);

    if (!buf) {
      fprintf(stderr, "pcap_next: %s\n", pcap_geterr(trace));
      break;
    }

    if (!started) {
      started = true;
      ts_start = pkthdr.ts;
    }

    if ((pkthdr.ts.tv_sec - ts_start.tv_sec) * 1000000 + (pkthdr.ts.tv_usec - ts_start.tv_usec) >= length_us) {
      fprintf(stderr, "done %ldus\n", length_us);
      if (--win_num) {
        getchar();
        ts_start = pkthdr.ts;
        continue;
      } else {
        break;
      }
    }

    nPkt++;

    auto pkt = reinterpret_cast<const CapPacket *>(buf);

    if (pkthdr.caplen > sizeof(iphdr)) {
      FlowKey key {
        .dstIp = pkt->ip.daddr,
        .srcIp = pkt->ip.saddr,
        .protocol = pkt->ip.protocol
      };
      if (pkt->ip.protocol == IPPROTO_UDP) {
        assert(pkthdr.caplen >= sizeof(iphdr) + sizeof(udphdr));
        key.dstPort = pkt->udp.dest;
        key.srcPort = pkt->udp.source;
      } else if (pkt->ip.protocol == IPPROTO_TCP) {
        assert(pkthdr.caplen >= sizeof(iphdr) + sizeof(tcphdr));
        key.dstPort = pkt->tcp.dest;
        key.srcPort = pkt->tcp.source;
      }

      h.insert(key);
      if (!cm.query(key)) {
        keyBuf.push_back(key);
      }
      cm.inc(key);
    }

    if (pkthdr.caplen > MAX_DATA)
      throw std::out_of_range("capture too large");

    memcpy(sendPkt.data, buf, pkthdr.caplen);
  }

  fprintf(stderr, "%d CapPackets, %zu of %zu keys in total\n", nPkt, keyBuf.size(), h.size());

  if (FILE *fp = fopen(argv[4], "wb")) {
    uint64_t nKey = keyBuf.size();
    fwrite(&nKey, sizeof(uint64_t), 1, fp);
    fwrite(keyBuf.data(), sizeof(FlowKey), keyBuf.size(), fp);
    fclose(fp);
  } else {
    perror("fopen");
  }

  pcap_close(trace);

  return 0;
}
