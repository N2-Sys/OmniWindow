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

struct CapPacket {
  iphdr ip;
  union {
    tcphdr tcp;
    udphdr udp;
  };
} __attribute__((packed));

constexpr int MAX_DATA = 65536;

struct SendPacket {
  ethhdr eth;
  u_char data[65536];
} sendPkt;

int main(int argc, char **argv) {
  long length_us;
  int win_num;
  if (argc != 5 || sscanf(argv[3], "%ld", &length_us) != 1 || sscanf(argv[4], "%d", &win_num) != 1) {
    fprintf(stderr, "usage: %s <trace-pcap> <inject-device> <length-us> <win-num>\n", argv[0]);
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

  pcap_t *dev = pcap_create(argv[2], errbuf);
  if (dev) {
    if (pcap_activate(dev) < 0) {
      fprintf(stderr, "pcap_activate: %s\n", pcap_geterr(dev));
      pcap_close(dev);
      dev = nullptr;
    }
  } else {
    fprintf(stderr, "pcap_create: %s\n", errbuf);
  }

  bool started = false;
  timeval ts_start;

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
    }

    if (pkthdr.caplen > MAX_DATA)
      throw std::out_of_range("capture too large");

    memcpy(sendPkt.data, buf, pkthdr.caplen);
    if (dev && pcap_inject(dev, &sendPkt, sizeof(ethhdr) + pkthdr.caplen) < 0) {
      fprintf(stderr, "pcap_inject: %s\n", pcap_geterr(dev));
      break;
    }
  }

  fprintf(stderr, "%d CapPackets\n", nPkt);

  if (dev)
    pcap_close(dev);

  pcap_close(trace);

  return 0;
}
