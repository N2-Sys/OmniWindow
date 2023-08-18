#ifndef TRACE_H
#define TRACE_H

#include <cstdint>

#include <vector>

struct FlowKey {
  // 8 (4*2) bytes
  uint32_t src_ip;  // source IP address
  uint32_t dst_ip;
  // 4 (2*2) bytes
  uint16_t src_port;
  uint16_t dst_port;
  // 1 bytes
  uint8_t proto;
} __attribute__((packed));

struct PktInfo {
  FlowKey key;

  // 8 bytes
  int32_t size;  // inner IP datagram length (header + data)

  // 9 bytes
  uint32_t tcp_ack;  // user for autosketch syn flood
  uint32_t tcp_seq;  // user for marple query
  uint8_t tcp_flag;

  // 8 bytes
  double pkt_ts;  // timestamp of the packet
  uint8_t ip_hdr_size;
  uint8_t tcp_hdr_size;
} __attribute__((packed));

using Trace = std::vector<PktInfo>;

Trace readTrace(const char *path);
void writeTrace(const char *path, const Trace &trace);

#endif
