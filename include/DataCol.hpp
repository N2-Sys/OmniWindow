#ifndef DATA_COL_H
#define DATA_COL_H

#include <inttypes.h>

#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>

#include "dpdk_toolchain.h"
#include "dpdk_toolchain/protocol_headers.h"

constexpr uint16_t DC_PORT = 33669;

struct DCRespPkt {
  ethernet_header_t ethernet;
  ipv4_header_t ipv4;
  udp_header_t udp;
  char data[0];
} __attribute__((packed));

template <typename DCReqData = uint16_t>
class DataCol {
  DPDKToolchainCpp::Device *device;
  char dst_mac[6];
  uint32_t src_ip, dst_ip;
  uint16_t src_port, dst_port;
  uint32_t payload_length;

  packet_data_t *pkt;
  DCReqData *dcHdr;

  packet_data_t *generate_packet() {
    auto pkt = reinterpret_cast<packet_data_t *>(
        rte_malloc(NULL, sizeof(packet_data_t), 64));
    pkt->data = (unsigned char *)pkt->mbuf;
    pkt->length = payload_length + sizeof(ethernet_header_t) +
                  sizeof(ipv4_header_t) + sizeof(udp_header_t);
    pkt->header_tail = pkt->headers;
    packet_header_t *ethh = pkt->header_tail++;
    packet_header_t *ipv4h = pkt->header_tail++;
    packet_header_t *udph = pkt->header_tail++;
    ethh->data = pkt->data;
    ethh->length = sizeof(ethernet_header_t);
    ipv4h->data = pkt->data + sizeof(ethernet_header_t);
    ipv4h->length = sizeof(ipv4_header_t);
    udph->data = pkt->data + sizeof(ethernet_header_t) + sizeof(ipv4_header_t);
    udph->length = sizeof(udp_header_t);

    auto eth = reinterpret_cast<ethernet_header_t *>(ethh->data);
    auto ipv4 = reinterpret_cast<ipv4_header_t *>(ipv4h->data);
    auto udp = reinterpret_cast<udp_header_t *>(udph->data);
    rte_memcpy(eth->dst, dst_mac, 6);
    rte_memcpy(eth->src, device->getMac(), 6);
    eth->type = ETH_PROTO_TYPE_IPV4;
    ipv4->src = src_ip;
    ipv4->dst = dst_ip;
    ipv4->proto = IP_PROTO_TYPE_UDP;
    ipv4->version = 4;
    ipv4->ihl = ipv4h->length >> 2;
    ipv4->tos = 0;
    ipv4->len = htons(ipv4h->length + udph->length + payload_length);
    ipv4->id = 0;
    ipv4->frag_off = 0;
    ipv4->ttl = 24;
    udp->sport = htons(src_port);
    udp->dport = htons(dst_port);
    udp->len = htons(udph->length + payload_length);
    return pkt;
  }

public:
  DataCol(int portId, const char *ip) {
    src_ip = DPDKToolchainCpp::s2ipv4(ip);
    dst_ip = DPDKToolchainCpp::s2ipv4(ip);
    src_port = DC_PORT;
    dst_port = DC_PORT;
    DPDKToolchainCpp::s2macaddr((char *)dst_mac, "00:00:00:00:00:00");
    payload_length = sizeof(DCReqData);

    int num_rx_desc = 16384;
    int num_tx_desc = 8192;
    device = new DPDKToolchainCpp::Device(portId, 1, 1, num_tx_desc,
                                          num_rx_desc, 32, 32);

    // fprintf(stderr, "rx_desc %d tx_desc %d\n", num_rx_desc, num_tx_desc);

    pkt = generate_packet();
    dcHdr = reinterpret_cast<DCReqData *>(
        pkt->data + sizeof(ethernet_header_t) + sizeof(ipv4_header_t) +
        sizeof(udp_header_t));
  }
  DataCol(const DataCol &) = delete;
  ~DataCol() {
    rte_free(pkt);
    delete device;
  }

  void flush() {
    device->m_txqueue[0]->flush();
  }

  void sendReq(DCReqData data, bool doFlush = true) {
    *dcHdr = data;
    void *ptr = device->m_txqueue[0]->getPacket(pkt, pkt->length);
    rte_memcpy(ptr, pkt->data, pkt->length);
    if (doFlush)
      flush();
  }

  void recv(packet_data_t *container) {
    while (1) {
      int rc = device->m_rxqueue[0]->getPacket(container);
      if (rc != 0)
        continue;
      if (container->length < sizeof(DCRespPkt)) {
        DPDKToolchainCpp::freePacket(container);
        continue;
      }
      auto *resp = reinterpret_cast<DCRespPkt *>(container->data);
      if (resp->ethernet.type != ETH_PROTO_TYPE_IPV4 ||
          resp->ipv4.dst != src_ip || resp->ipv4.proto != IP_PROTO_TYPE_UDP ||
          resp->udp.dport != htons(DC_PORT)) {
        DPDKToolchainCpp::freePacket(container);
        continue;
      }
      break;
    }
  }
};

#endif
