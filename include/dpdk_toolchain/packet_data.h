#ifndef __OMNISTACK_PACKET_DATA_H
#define __OMNISTACK_PACKET_DATA_H

#include<stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
    typedef struct packet_header {
        uint8_t length;
        unsigned char *data;
    } packet_header_t;

    typedef struct packet_data {
        uint16_t cnt;               // Read only for developer. Referrence count
        uint16_t length;            // 数据总长度
        uint16_t channel;           // Read only for developer. Target channel
        uint16_t offset;            // Currently decoded, also the length of all headers
        uint16_t nic;               // Read only for developer. (or you have some requirement.) 来自的classifier id
        uint16_t mbuf_type;         // Origin, DPDK
        uint32_t custom_mask;       // Moudle can use this to transport other information.
        void *custom_payload;
        struct packet_data *next;   // next packet in list, in order to support multi-packet return-value
        packet_header_t *header_tail;
        unsigned char *data;
        uint64_t padding[2];
        // a cache line end here
        char mbuf[1024];
        packet_header_t headers[4];
        unsigned char header_data[256];
    } packet_data_t;

#ifdef __cplusplus
}
#endif

#endif
