//
// Created by guojunyi on 9/4/22.
//

#include <mutex>
#include <iostream>

#include "dpdk_toolchain.h"
#include "dpdk_toolchain/protocol_headers.h"

namespace DPDKToolchainCpp {
    static int envInitialized = 0;
    static std::mutex envInitializeLock;
    static char dpdk_cpu_mask[128] = "0xff";
    static int dpdk_cpu_main_core = 0;

    [[maybe_unused]] void setDpdkPortMask(const char* mask, int main_core) {
        envInitializeLock.lock();
        if (envInitialized == 1) {
            envInitializeLock.unlock();
            throw DPDKToolChainException("Set mask and port before init dpdk");
            return ;
        }
        dpdk_cpu_main_core = main_core;
        strcpy(dpdk_cpu_mask, mask);
        envInitializeLock.unlock();
    }

    static
    void initDpdk() {
        if (envInitialized)
            return ;
        envInitializeLock.lock();
        if (envInitialized) {
            envInitializeLock.unlock();
            return ;
        }

        const int argc = 4;
        char argv[argc][1024] = {"./main", "-c", "", "--log-level=critical"};
        strcpy(argv[2], dpdk_cpu_mask);
        // sprintf(argv[4], "%d", dpdk_cpu_main_core);

        char **argv_p = (char**)calloc(argc, sizeof(char*));
        for (int i=0;i<argc;i++) {
            argv_p[i] = (char *) calloc(strlen(argv[i]) + 1, sizeof(char));
            strcpy(argv_p[i], argv[i]);
        }
        rte_eal_init(argc, argv_p);
        envInitialized = 1;
        envInitializeLock.unlock();
    }

    bool isPortUsable(int port) {
        initDpdk();
        int _port;
        RTE_ETH_FOREACH_DEV(_port) {
            if (_port == port) {
                return true;
            }
        }
        return false;
    }

    void printUsableDevices() {
        initDpdk();
        int _port;
        RTE_ETH_FOREACH_DEV(_port) {
            printf("Usable: %d\n", _port);
        }
    }

    Device::Device(int port, int num_rxq, int num_txq, int &num_tx_desc, int &num_rx_desc, int rx_batch_size, int tx_batch_size, rte_eth_conf conf) {
        initDpdk();
        this->m_port = port;
        if (!isPortUsable(port))
            throw DPDKToolChainException("Not a valid port");
        if (rte_eth_dev_info_get(this->m_port, &this->m_dev_info) < 0)
            throw DPDKToolChainException("Failed to get port info");
        if (num_rxq > this->m_dev_info.max_rx_queues)
            throw DPDKToolChainException("Too many rx queue");
        if (num_txq > this->m_dev_info.max_tx_queues)
            throw DPDKToolChainException("Too many tx queue");
        this->m_config = conf;
        this->m_config.rxmode.offloads &= this->m_dev_info.rx_offload_capa;
        this->m_config.txmode.offloads &= this->m_dev_info.tx_offload_capa;
        this->m_config.rx_adv_conf.rss_conf.rss_hf &= this->m_dev_info.flow_type_rss_offloads;

        this->m_num_rx_desc = num_rx_desc;
        this->m_num_tx_desc = num_tx_desc;
        if (rte_eth_dev_adjust_nb_rx_tx_desc(this->m_port, &m_num_rx_desc, &m_num_tx_desc) < 0)
            throw DPDKToolChainException("Failed to adjust num of rx tx desc");
        num_rx_desc = m_num_rx_desc;
        num_tx_desc = m_num_tx_desc;
        if (rte_eth_dev_configure(this->m_port, num_rxq, 1, &this->m_config) < 0)
            throw DPDKToolChainException("Failed to configure device");

        for (int i = 0; i < num_rxq; i ++) {
            auto up = std::make_unique<DeviceRxQueue>(this->m_port, i,
                                                      this->m_num_rx_desc, rx_batch_size, this->m_config);
            this->m_rxqueue.push_back(std::move(up));
        }

        for (int i = 0; i < num_txq; i ++) {
            auto up = std::make_unique<DeviceTxQueue>(this->m_port, i,
                                                      this->m_num_tx_desc, tx_batch_size, this->m_config);
            this->m_txqueue.push_back(std::move(up));
        }

        if (rte_eth_promiscuous_enable(this->m_port) < 0)
            throw DPDKToolChainException("Failed to enable promiscuous mode");
        // struct rte_eth_fc_conf fc_conf{};
        // memset(&fc_conf, 0, sizeof(fc_conf));
        // if (rte_eth_dev_flow_ctrl_get(this->m_port, &fc_conf) < 0)
        //     throw DPDKToolChainException("Faield to get flow control config");
        // fc_conf.mode = RTE_FC_NONE;
        // if (rte_eth_dev_flow_ctrl_set(this->m_port, &fc_conf) < 0)
        //     throw DPDKToolChainException("Failed to set flow control config");

        if (rte_eth_dev_start(this->m_port) < 0)
            throw DPDKToolChainException("Failed to start device");
        this->m_started = true;
    }

    Device::~Device() noexcept(false) {
        if (this->m_started) {
            if (rte_eth_dev_stop(this->m_port) < 0) {
                throw DPDKToolChainException("Failed to stop interface");
            }
        }
    }

    DeviceRxQueue::DeviceRxQueue(int port, int q_index, int num_rx_desc, int batch_size, rte_eth_conf &conf) {
        this->m_port = port;
        this->m_queue_index = q_index;
        this->m_num_rx_desc = num_rx_desc;
        this->m_batch_size = batch_size;

        this->m_config = conf;

        this->m_buffer.resize(this->m_batch_size, nullptr);
        this->m_buffer_data_size = 0;

        auto buff_size = this->m_config.rxmode.mtu + 64 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM;

        char name[RTE_MEMPOOL_NAMESIZE];
        sprintf(name, "__DTCPP_PORT_%d_RXQ_%d", this->m_port, this->m_queue_index);

        this->m_mempool = rte_mempool_create(
                name, NB_MBUF, buff_size, MEMPOOL_CACHE_SIZE,
                sizeof(struct rte_pktmbuf_pool_private),
                rte_pktmbuf_pool_init, nullptr,
                rte_pktmbuf_init, nullptr,
                rte_eth_dev_socket_id(this->m_port), 0
        );

        if (this->m_mempool == nullptr)
            throw DPDKToolChainException("Failed to create mempool for rx queue");

        struct rte_eth_dev_info dev_info = {};
        if (rte_eth_dev_info_get(this->m_port, &dev_info) < 0)
            throw DPDKToolChainException("RxQueue: Failed to get port info");

        this->m_rxq_conf = dev_info.default_rxconf;
        this->m_rxq_conf.offloads = conf.rxmode.offloads;
        if (rte_eth_rx_queue_setup(this->m_port, this->m_queue_index, this->m_num_rx_desc,
                                   rte_eth_dev_socket_id(this->m_port), &this->m_rxq_conf, this->m_mempool) < 0) {
            throw DPDKToolChainException("Failed to setup rx queue");
        }
    }

    DeviceRxQueue::~DeviceRxQueue() noexcept(false) {
        if (this->m_mempool != nullptr) {
            rte_mempool_free(this->m_mempool);
            this->m_mempool = nullptr;
        }
    }

    DeviceTxQueue::DeviceTxQueue(int port, int q_index, int num_tx_desc, int batch_size, rte_eth_conf &conf) {
        this->m_port = port;
        this->m_queue_index = q_index;
        this->m_num_tx_desc = num_tx_desc;
        this->m_batch_size = batch_size;

        this->m_config = conf;

        this->m_buffer.resize(this->m_batch_size, nullptr);
        this->m_buffer_data_size = 0;

        auto buff_size = this->m_config.rxmode.mtu + 64 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM;

        char name[RTE_MEMPOOL_NAMESIZE];
        sprintf(name, "__DTCPP_PORT_%d_TXQ_%d", this->m_port, this->m_queue_index);

        this->m_mempool = rte_mempool_create(
                name, NB_MBUF, buff_size, MEMPOOL_CACHE_SIZE,
                sizeof(struct rte_pktmbuf_pool_private),
                rte_pktmbuf_pool_init, nullptr,
                rte_pktmbuf_init, nullptr,
                rte_eth_dev_socket_id(this->m_port), 0
        );

        if (this->m_mempool == nullptr)
            throw DPDKToolChainException("Failed to create mempool for tx queue");

        struct rte_eth_dev_info dev_info = {};
        if (rte_eth_dev_info_get(this->m_port, &dev_info) < 0)
            throw DPDKToolChainException("RxQueue: Failed to get port info");

        this->m_txq_conf = dev_info.default_txconf;
        this->m_txq_conf.offloads = conf.txmode.offloads;

        if (rte_eth_tx_queue_setup(this->m_port, this->m_queue_index, this->m_num_tx_desc,
                                   rte_eth_dev_socket_id(this->m_port), &this->m_txq_conf) < 0)
            throw DPDKToolChainException("Failed to setup tx queue");

        for (int i = 0; i < this->m_batch_size; i ++) {
            this->m_buffer[i] = rte_pktmbuf_alloc(this->m_mempool);
            if (this->m_buffer[i] == nullptr)
                throw DPDKToolChainException("Failed to allocate packet");
        }
    }

    int DeviceRxQueue::getPacket(packet_data_t* container) {
        if (this->m_buffer_data_size == this->m_buffer_data_index) {
            this->m_buffer_data_size = rte_eth_rx_burst(this->m_port, this->m_queue_index,
                                                        this->m_buffer.data(), this->m_batch_size);
            this->m_buffer_data_index = 0;
        }
        if (this->m_buffer_data_size != this->m_buffer_data_index) {
            container->data = rte_pktmbuf_mtod(this->m_buffer[this->m_buffer_data_index], uint8_t *);
            container->length = this->m_buffer[this->m_buffer_data_index]->pkt_len;
            this->m_buffer_data_index ++;
            return 0;
        }
        return -1;
    }

    [[maybe_unused]] void freePacket(packet_data_t* container) {
        unsigned char* ptr = container->data - RTE_PKTMBUF_HEADROOM - sizeof(struct rte_mbuf);
        rte_pktmbuf_free((struct rte_mbuf*)ptr);
    }

    uint8_t *DeviceTxQueue::getPacket(packet_data_t* container, uint32_t len) {
        if (this->m_buffer_data_size == this->m_batch_size || this->m_buffer[this->m_buffer_data_size] == nullptr) {
            this->flush();
        }
        if (this->m_buffer_data_size == this->m_batch_size)
            return nullptr;

        auto m = this->m_buffer[this->m_buffer_data_size];
        m->pkt_len = m->data_len = len;
        m->nb_segs = 1;
        m->next = NULL;

        auto ret = rte_pktmbuf_mtod(m, uint8_t*);
        m->ol_flags = 0;

        if (container) {
            m->l2_len = sizeof(ethernet_header_t);
            ethernet_header_t* ethh = (ethernet_header_t*)(container->header_tail - 1)->data;
            switch (ethh->type) {
                case ETH_PROTO_TYPE_IPV4: {
                    ipv4_header_t* ipv4h = (ipv4_header_t*)(container->header_tail - 2)->data;
                    m->ol_flags = RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_IPV4;
                    m->l3_len = sizeof(ipv4_header_t);
                    ipv4h->chksum = 0;
                    switch (ipv4h->proto) {
                        case IP_PROTO_TYPE_TCP: {
                            m->ol_flags |= RTE_MBUF_F_TX_TCP_CKSUM;
                            tcp_header_t* tcph = (tcp_header_t*)(container->header_tail - 3)->data;
                            tcph->chksum = rte_ipv4_phdr_cksum((const struct rte_ipv4_hdr*)ipv4h, m->ol_flags);
                            break;
                        }
                        case IP_PROTO_TYPE_UDP: {
                            m->ol_flags |= RTE_MBUF_F_TX_UDP_CKSUM;
                            udp_header_t* udph = (udp_header_t*)(container->header_tail - 3)->data;
                            udph->chksum = rte_ipv4_phdr_cksum((const struct rte_ipv4_hdr*)ipv4h, m->ol_flags);
                            break;
                        }
                    }
                    break;
                }
                case ETH_PROTO_TYPE_IPV6: {
                    m->ol_flags = RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_IPV6;
                    break;
                }
            }
        }
        this->m_buffer_data_size ++;

        return ret;
    }

    void DeviceTxQueue::flush() {
        if (this->m_buffer_data_size != 0) {
            int flushed = 0;

            while (flushed < this->m_buffer_data_size) {
                auto sent = rte_eth_tx_burst(this->m_port, this->m_queue_index, this->m_buffer.data() + flushed,
                                             this->m_buffer_data_size - flushed);
                flushed += sent;
            }

            while (this->m_buffer_data_size < this->m_batch_size &&
                this->m_buffer[this->m_buffer_data_size] == nullptr) {
                this->m_buffer_data_size ++;
            }

            for (int i = 0; i < this->m_buffer_data_size; i ++) {
                this->m_buffer[i] = rte_pktmbuf_alloc(this->m_mempool);
                if (!this->m_buffer[i])
                    break;
            }

            this->m_buffer_data_size = 0;
        }
    }

    const char* Device::getMac() {
        static struct rte_ether_addr mac_addr;
        rte_eth_macaddr_get(this->m_port, &mac_addr);
        return reinterpret_cast<const char*>(mac_addr.addr_bytes);
    }

    void bindCore(int core) {
        cpu_set_t cpus;

        CPU_ZERO(&cpus);
        CPU_SET((unsigned)core,&cpus);
        if (rte_thread_set_affinity(&cpus) != 0)
            throw DPDKToolChainException("Failed to bind cpu core");
    }
}
