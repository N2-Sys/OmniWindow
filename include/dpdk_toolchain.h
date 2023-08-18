//
// Created by guojunyi on 9/4/22.
//

#ifndef SPEED_TEST_DPDK_TOOLCHAIN_H
#define SPEED_TEST_DPDK_TOOLCHAIN_H

#include <rte_dev.h>
#include <rte_dev_info.h>
#include <rte_flow.h>
#include <rte_hash.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>

#include <string>
#include <utility>
#include <vector>
#include <memory>

#include "dpdk_toolchain/packet_data.h"

#define TCP_GT_32(a,b) ((int32_t)((a)-(b))>0)
#define TCP_GE_32(a,b) ((int32_t)((a)-(b))>=0)
#define TCP_LT_32(a,b) ((int32_t)((a)-(b))<0)
#define TCP_LE_32(a,b) ((int32_t)((a)-(b))<=0)

namespace DPDKToolchainCpp{

    static inline
    void s2macaddr(char* dst, const char* val) {
        uint32_t a[6];
        sscanf(val, "%x:%x:%x:%x:%x:%x", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]);
        for(int i=0;i<6;i++)
            dst[i] = a[i];
    }

    static inline
    uint32_t s2ipv4(const char* val) {
        uint32_t a, b, c, d;
        sscanf(val, "%u.%u.%u.%u", &a, &b, &c, &d);
        return a | (b << 8) | (c << 16) | (d << 24);
    }

#define CORE_MAX_BUF_SIZE 8600
#define NB_MBUF 16384
#define MEMPOOL_CACHE_SIZE 256

    // Mellanox Linux's driver key
    static uint8_t default_rsskey_40bytes[40] = {
        0xd1, 0x81, 0xc6, 0x2c, 0xf7, 0xf4, 0xdb, 0x5b,
        0x19, 0x83, 0xa2, 0xfc, 0x94, 0x3e, 0x1a, 0xdb,
        0xd9, 0x38, 0x9e, 0x6b, 0xd1, 0x03, 0x9c, 0x2c,
        0xa7, 0x44, 0x99, 0xad, 0x59, 0x3d, 0x56, 0xd9,
        0xf3, 0x25, 0x3c, 0x06, 0x2a, 0xdc, 0x1f, 0xfc
    };

    const struct rte_eth_conf CORE_DEFAULT_DPDK_PORT_CONF = {
        .link_speeds = 0,
        .rxmode = {
            .mq_mode = ETH_MQ_RX_RSS,
            .mtu = CORE_MAX_BUF_SIZE,
            .max_lro_pkt_size = 0,
            .split_hdr_size = 0,
            .offloads = DEV_RX_OFFLOAD_RSS_HASH
        },
        .txmode = {
            .mq_mode = ETH_MQ_TX_NONE,
            .offloads =
                DEV_TX_OFFLOAD_VLAN_INSERT |
                DEV_TX_OFFLOAD_IPV4_CKSUM  |
                DEV_TX_OFFLOAD_UDP_CKSUM   |
                DEV_TX_OFFLOAD_TCP_CKSUM   |
                DEV_TX_OFFLOAD_SCTP_CKSUM  |
                DEV_TX_OFFLOAD_TCP_TSO,
        },
        .lpbk_mode = 0,
        .rx_adv_conf = {
            .rss_conf = {
                .rss_key = default_rsskey_40bytes,
                .rss_key_len = sizeof(default_rsskey_40bytes),
                .rss_hf =
                    ETH_RSS_FRAG_IPV4           |
                    ETH_RSS_NONFRAG_IPV4_UDP    |
                    ETH_RSS_NONFRAG_IPV4_TCP    |
                    ETH_RSS_TCP
            }
        }
    };

    class DPDKToolChainException : public std::exception {
    public:
        inline
        explicit DPDKToolChainException(std::string desc = "") : std::exception() {
            this->desc = std::move(desc);
        }

        [[nodiscard]] inline
        std::string description() const {
            return this->desc;
        }

        [[nodiscard]] inline
        const char * what () const noexcept override {
            return this->desc.c_str();
        }

    private:
        std::string desc;
    };

    class DeviceRxQueue {
    public:
        DeviceRxQueue(int port, int q_index, int num_rx_desc, int batch_size, rte_eth_conf &conf);
        virtual ~DeviceRxQueue() noexcept(false);

        /**
         * @param container used to contain a packet's metadat
         * @return 0 if a packet receieved -1 if no packet
         */
        int getPacket(packet_data_t* container);

    private:
        int m_port;
        int m_queue_index;
        int m_num_rx_desc;
        int m_batch_size;

        struct rte_eth_conf m_config{};

        struct rte_mempool* m_mempool = nullptr;

        std::vector<struct rte_mbuf*> m_buffer;
        int m_buffer_data_size;
        int m_buffer_data_index = 0;

        struct rte_eth_rxconf m_rxq_conf{};
    };

    class DeviceTxQueue {
    public:
        DeviceTxQueue(int port, int q_index, int num_tx_desc, int batch_size, rte_eth_conf& conf);

        /**
         * Get Write Ptr
         * @param container used to generate offloading data nullptr for no need
         * @param len payload length
         * @return writeable ptr
         */
        uint8_t* getPacket(packet_data_t* container, uint32_t len);
        void flush();

    private:
        int m_port;
        int m_queue_index;
        int m_num_tx_desc;
        int m_batch_size;

        struct rte_eth_conf m_config{};

        struct rte_mempool* m_mempool = nullptr;

        std::vector<struct rte_mbuf*> m_buffer;
        int m_buffer_data_size;

        struct rte_eth_txconf m_txq_conf{};
    };

    class Device {
    public:
        Device(int port, int num_rxq, int num_txq, int &num_tx_desc, int &num_rx_desc, int rx_batch_size, int tx_batch_size, rte_eth_conf conf = CORE_DEFAULT_DPDK_PORT_CONF);
        Device(const Device& dev) = delete;
        virtual ~Device() noexcept(false);

        const char* getMac();

        std::vector<std::unique_ptr<DeviceRxQueue> > m_rxqueue = {};
        std::vector<std::unique_ptr<DeviceTxQueue> > m_txqueue = {};

    private:
        bool m_started = false;

        int m_port = -1;

        struct rte_eth_conf m_config{};
        struct rte_eth_dev_info m_dev_info{};

        uint16_t m_num_rx_desc = 0;
        uint16_t m_num_tx_desc = 0;
    };

    bool isPortUsable(int port);
    [[maybe_unused]] void setDpdkPortMask(const char* mask, int main_core);

    [[maybe_unused]] void freePacket(packet_data_t* container);
    [[maybe_unused]] void printUsableDevices();
    [[maybe_unused]] void printDeviceInfo(int port);
    [[maybe_unused]] void bindCore(int core);
}

#endif //SPEED_TEST_DPDK_TOOLCHAIN_H
