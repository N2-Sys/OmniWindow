
// 使用库里的头不太方便，自己实现一个
#ifndef __OMNISTACK_PROTOCOL_HEADERS_H
#define __OMNISTACK_PROTOCOL_HEADERS_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#pragma pack(1)
typedef struct ethernet_header {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t type;
} ethernet_header_t;

#ifndef BIGENDIAN
#define ETH_PROTO_TYPE_IPV4 0x0008
#define ETH_PROTO_TYPE_IPV6 0xDD86
#define ETH_PROTO_TYPE_ARP 0x0608
#else
#define ETH_PROTO_TYPE_IPV4 0x0800
#define ETH_PROTO_TYPE_IPV6 0x86DD
#define ETH_PROTO_TYPE_ARP 0x0806
#endif

#ifndef BIGENDIAN
#define ARP_PROTO_HW_ETH 0x0100
#define ARP_PROTO_OP_REQUEST 0x0100
#define ARP_PROTO_OP_REPLY 0x0200
#else
#define ARP_PROTO_HW_ETH 0x0001
#define ARP_PROTO_OP_REQUEST 0x0001
#define ARP_PROTO_OP_REPLY 0x0002
#endif
#define ARP_PROTO_PACKET_SIZE 20
typedef struct arp_header {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t mac_length;
    uint8_t ip_length;
    uint16_t op_type;
} arp_header_t;

typedef struct arp_body {
    uint8_t src_mac[6];
    uint32_t src_ip;
    uint8_t dst_mac[6];
    uint32_t dst_ip;
} arp_body_t;

#pragma pack(1)
typedef struct ipv4_header {
#ifndef BIGENDIAN
    uint8_t     ihl: 4;
    uint8_t     version: 4;
#else
    uint8_t     version: 4;
    uint8_t     ihl: 4;
#endif
    uint8_t     tos;
    uint16_t    len;
    uint16_t    id;
    uint16_t    frag_off;
    uint8_t     ttl;
    uint8_t     proto;
    uint16_t    chksum;
    uint32_t    src;
    uint32_t    dst;
} ipv4_header_t;

// #pragma pack(1)
// typedef struct ipv6_header {
//     uint32_t    version: 4;
//     uint32_t    tc: 8;
//     uint32_t    fl: 20;
//     uint16_t    plen;
//     uint8_t     nh;
//     uint8_t     hlim;
//     __uint128_t src;
//     __uint128_t dst;
// } ipv6_header_t;

// If you wonder why I wrote like this, you should see how linux define it.
#pragma pack(1)
typedef struct ipv6_header {
#ifndef BIGENDIAN
	uint8_t		priority:4,
				version:4;
#else
	uint8_t		version:4,
				priority:4;
#endif
	uint8_t		fl[3];
	uint16_t	plen;
	uint8_t		nh;
	uint8_t		hlim;
    __uint128_t src;
    __uint128_t dst;
} ipv6_header_t;

#define IP_PROTO_TYPE_TCP 0x06
#define IP_PROTO_TYPE_UDP 0x11

#define TCP_OPTION_KIND_EOL 0x00
#define TCP_OPTION_KIND_NOP 0x01
#define TCP_OPTION_KIND_MSS 0x02
#define TCP_OPTION_KIND_WSOPT 0x03
#define TCP_OPTION_KIND_SACK_PREMITTED 0x04
#define TCP_OPTION_KIND_SACK 0x05
#define TCP_OPTION_KIND_TSPOT 0x08

#define TCP_OPTION_LENGTH_EOL 0x01
#define TCP_OPTION_LENGTH_NOP 0x01
#define TCP_OPTION_LENGTH_MSS 0x04
#define TCP_OPTION_LENGTH_WSOPT 0x03
#define TCP_OPTION_LENGTH_SACK_PREMITTED 0x02
#define TCP_OPTION_LENGTH_TSPOT 0x0A

typedef struct tcp_header {
    uint16_t    sport;
    uint16_t    dport;
    uint32_t    seq;
    uint32_t    ACK;
    union {
    uint16_t    tcpflags;
    struct {
#ifndef BIGENDIAN
    
    uint16_t    reserved:4;
    uint16_t    dataofs:4;
    uint16_t    fin:1;
    uint16_t    syn:1;
    uint16_t    rst:1;
    uint16_t    psh:1;
    uint16_t    ack:1;
    uint16_t    urg:1;
    uint16_t    ece:1;
    uint16_t    cwr:1;
#else
    uint16_t    dataofs:4;
    uint16_t    reserved:4;
    uint16_t    cwr:1;
    uint16_t    ece:1;
    uint16_t    urg:1;
    uint16_t    ack:1;
    uint16_t    psh:1;
    uint16_t    rst:1;
    uint16_t    syn:1;
    uint16_t    fin:1;
#endif
    };
    };
    uint16_t    window;
    uint16_t    chksum;
    uint16_t    urgptr;
} tcp_header_t;

typedef struct udp_header {
    uint16_t sport;
    uint16_t dport;
    uint16_t len;
    uint16_t chksum;
} udp_header_t;
#pragma pack()

#endif