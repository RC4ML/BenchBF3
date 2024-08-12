#include "dpdk_common.h"
static volatile bool force_quit;

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define MAX_PKT_BURST 64

/*
 * Configurable number of RX/TX ring descriptors
 */
static uint16_t nb_rxd = 2048;
static uint16_t nb_txd = 2048;
// static uint32_t NUM_MBUFS = 1024 * 8;


/* ethernet addresses of ports */
static struct rte_ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static uint32_t l2fwd_enabled_port_mask = 1;

#define TX_RX_QUEUE 1

struct server_queue_conf server_queue_conf[RTE_MAX_LCORE];

static struct rte_eth_conf port_conf = {
    .rxmode = {
        .mq_mode = RTE_ETH_MQ_RX_RSS,
        .mtu = RTE_ETHER_MTU,
        .offloads = RTE_ETH_RX_OFFLOAD_RSS_HASH | RTE_ETH_RX_OFFLOAD_CHECKSUM,
    },
    .txmode = {
        .mq_mode = RTE_ETH_MQ_TX_NONE,
        .offloads = RTE_ETH_TX_OFFLOAD_IPV4_CKSUM | RTE_ETH_TX_OFFLOAD_UDP_CKSUM,
    },
    .rx_adv_conf = {
        .rss_conf = {
            .rss_key = rss_key,
            .rss_key_len = 40,
            .rss_hf = RTE_ETH_RSS_UDP,
        },
    },
};

struct rte_mempool *server_pktmbuf_pool;

class timestamps_acc_number {
public:
    uint64_t timestamp;
    uint32_t acc_number;
    bool operator < (const timestamps_acc_number &tmp) const {
        return (timestamp < tmp.timestamp);
    }
    bool operator > (const timestamps_acc_number &tmp) const {
        return (timestamp > tmp.timestamp);
    }
    bool operator == (const timestamps_acc_number &tmp) const {
        return timestamp == tmp.timestamp && acc_number == tmp.acc_number;
    }
};

/* Per-port statistics struct */
struct l2fwd_port_statistics {
    uint64_t tx[TX_RX_QUEUE];
    uint64_t rx[TX_RX_QUEUE];
    uint64_t tx_dropped[TX_RX_QUEUE];
} __rte_cache_aligned;
struct l2fwd_port_statistics port_statistics[RTE_MAX_ETHPORTS];
struct l2fwd_port_statistics port_statistics_period[RTE_MAX_ETHPORTS];

// struct rte_flow *flow[TX_RX_QUEUE];
// uint32_t dst_ip_mask[TX_RX_QUEUE] = {0, 1, 2, 3};
// #define DST_IP_MASK 0x3
// #define FULL_MASK 0xffffffff /* full mask */
// #define EMPTY_MASK 0x0		 /* empty mask */

#define MAX_TIMER_PERIOD 86400 /* 1 day max */
/* A tsc-based timer responsible for triggering statistics printout */
static uint64_t timer_period = 1; /* default period is 1 seconds */

std::vector<std::vector<timestamps_acc_number>> total_inqueue_timestamp_acc;
std::vector<std::vector<timestamps_acc_number>> total_dequeue_timestamp_acc;


/* Print out statistics on packets tx_dropped */
static void
print_stats(void) {
    uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
    uint64_t period_packets_dropped, period_packets_tx, period_packets_rx;

    unsigned portid, queueid;
    uint64_t period = timer_period / rte_get_timer_hz();

    const char clr[] = { 27, '[', '2', 'J', '\0' };
    const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

    /* Clear screen and move to top left */
    printf("%s%s", clr, topLeft);

    printf("\nPort statistics ====================================");

    for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
        total_packets_dropped = 0;
        total_packets_tx = 0;
        total_packets_rx = 0;
        period_packets_dropped = 0;
        period_packets_tx = 0;
        period_packets_rx = 0;
        /* skip disabled ports */
        if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
            break;
        printf("\nStatistics for port %u ------------------------------", portid);
        for (queueid = 0; queueid < TX_RX_QUEUE; queueid++) {
            printf("\n     Queue %u --------------------------------------"
                "\n     Packets sent: %24" PRIu64
                "\n     Packets received: %20" PRIu64
                "\n     Packets tx_dropped: %18" PRIu64,
                queueid,
                port_statistics[portid].tx[queueid],
                port_statistics[portid].rx[queueid],
                port_statistics[portid].tx_dropped[queueid]);
            total_packets_dropped += port_statistics[portid].tx_dropped[queueid];
            total_packets_tx += port_statistics[portid].tx[queueid];
            total_packets_rx += port_statistics[portid].rx[queueid];

            period_packets_dropped += port_statistics[portid].tx_dropped[queueid] - port_statistics_period[portid].tx_dropped[queueid];
            period_packets_tx += port_statistics[portid].tx[queueid] - port_statistics_period[portid].tx[queueid];
            period_packets_rx += port_statistics[portid].rx[queueid] - port_statistics_period[portid].rx[queueid];
        }

        printf("\nTotal for port %u ------------------------------"
            "\nPackets sent: %24" PRIu64
            "\nPackets received: %20" PRIu64
            "\nPackets tx_dropped: %18" PRIu64,
            portid,
            total_packets_tx,
            total_packets_rx,
            total_packets_dropped);

        printf("\nPeriod statistics ==============================="
            "\nPackets sent speed: %18" PRIu64
            "\nPackets received speed: %14" PRIu64
            "\nPackets tx_dropped speed: %12" PRIu64,
            period_packets_tx / period,
            period_packets_rx / period,
            period_packets_dropped / period);
        printf("\n====================================================\n");
        memcpy(&port_statistics_period[portid], &port_statistics[portid], sizeof(port_statistics[portid]));
    }

    fflush(stdout);
}

static void
l2fwd_main_lcore_show_status(void) {
    uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
    unsigned lcore_id;

    prev_tsc = 0;
    timer_tsc = 0;
    lcore_id = rte_lcore_id();
    if (lcore_id != rte_get_main_lcore()) {
        RTE_LOG(INFO, L2FWD, "lcore %u don't be main lcore\n", lcore_id);
        return;
    }
    sleep(1);
    while (!force_quit) {
        cur_tsc = rte_rdtsc();
        diff_tsc = cur_tsc - prev_tsc;

        /* advance the timer */
        timer_tsc += diff_tsc;

        /* if timer has reached its timeout */
        if (unlikely(timer_tsc >= timer_period)) {

            print_stats();
            /* reset the timer */
            timer_tsc = 0;
        }

        prev_tsc = cur_tsc;
        usleep(1000);
    }
}

/* main processing loop */
static void
l2fwd_main_loop(void) {
    struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
    unsigned lcore_id;
    unsigned i = 0, j = 0, queueid, nb_rx;
    struct server_queue_conf *qconf;

    lcore_id = rte_lcore_id();
    qconf = &server_queue_conf[lcore_id];

    if (qconf->n_tx_rx_queue == 0) {
        RTE_LOG(INFO, L2FWD, "lcore %u has nothing to do\n", lcore_id);
        return;
    }

    RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);
    uint16_t port_id = qconf->portid;
    // struct rte_ether_addr target_src_hdr { 0x00, 0x02, 0x00, 0x00, 0x03, 0x00 };
    uint32_t out_timestamp_last = 0;
    uint64_t out_timestamp_acc = 0;
    uint32_t last_seq_num = 0;

    uint32_t target_length = 1082;
    while (!force_quit) {
        /*
         * Read packet from RX queues
         */
        for (i = 0; i < qconf->n_tx_rx_queue; i++) {

            queueid = qconf->tx_rx_queue_list[i];
            nb_rx = rte_eth_rx_burst(port_id, queueid,
                pkts_burst, MAX_PKT_BURST);
            for (j = 0; j < nb_rx; j++) {
                if (rte_pktmbuf_pkt_len(pkts_burst[j]) == target_length) {
                    port_statistics[port_id].rx[queueid]++;
                    uint32_t *data = rte_pktmbuf_mtod_offset(pkts_burst[j], uint32_t *, 54);
                    uint32_t in_depth = ntohl(data[0]);
                    uint32_t out_depth = ntohl(data[2]);

                    uint32_t in_timestamp = ntohl(data[1]);
                    uint32_t out_timestamp = ntohl(data[3]);
                    uint32_t seq_num = ntohl(data[4]);

                    if (seq_num < last_seq_num && seq_num > 10000000) {
                        rte_pktmbuf_free(pkts_burst[j]);
                        continue;
                    }
                    last_seq_num = seq_num;
                    if (out_timestamp < out_timestamp_last && out_timestamp < 100000) {
                        out_timestamp_acc += (1 << 18);
                    }
                    out_timestamp_last = out_timestamp;
                    // printf("in_depth %u in_timestamp %lu out_depth %u out_timestamp %lu\n", in_depth, in_timestamp + out_timestamp_acc, out_depth, out_timestamp + out_timestamp_acc);
                    if (out_depth > 19) {
                        total_inqueue_timestamp_acc[queueid].push_back({ out_timestamp_acc + out_timestamp - in_timestamp,  in_depth });
                        total_dequeue_timestamp_acc[queueid].push_back({ out_timestamp_acc + out_timestamp, out_depth });
                    }
                } else {
                    port_statistics[port_id].tx_dropped[queueid]++;
                }
                rte_pktmbuf_free(pkts_burst[j]);
            }
        }
    }
}

static int
l2fwd_launch_one_lcore(__rte_unused void *dummy) {

    if (rte_lcore_id() == rte_get_main_lcore()) {
        l2fwd_main_lcore_show_status();
    } else {
        l2fwd_main_loop();
    }
    return 0;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint32_t port_mask) {
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90  /* 9s (90 * 100ms) in total */
    uint16_t portid;
    uint8_t count, all_ports_up, print_flag = 0;
    struct rte_eth_link link;
    int ret;
    char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];

    printf("\nChecking link status");
    fflush(stdout);
    for (count = 0; count <= MAX_CHECK_TIME; count++) {
        if (force_quit)
            return;
        all_ports_up = 1;
        RTE_ETH_FOREACH_DEV(portid) {
            if (force_quit)
                return;
            if ((port_mask & (1 << portid)) == 0)
                continue;
            memset(&link, 0, sizeof(link));
            ret = rte_eth_link_get_nowait(portid, &link);
            if (ret < 0) {
                all_ports_up = 0;
                if (print_flag == 1)
                    printf("Port %u link get failed: %s\n",
                        portid, rte_strerror(-ret));
                continue;
            }
            /* print link status if flag set */
            if (print_flag == 1) {
                rte_eth_link_to_str(link_status_text,
                    sizeof(link_status_text), &link);
                printf("Port %d %s\n", portid,
                    link_status_text);
                continue;
            }
            /* clear all_ports_up flag if any link down */
            if (link.link_status == RTE_ETH_LINK_DOWN) {
                all_ports_up = 0;
                break;
            }
        }
        /* after finally printing all link status, get out */
        if (print_flag == 1)
            break;

        if (all_ports_up == 0) {
            printf(".");
            fflush(stdout);
            rte_delay_ms(CHECK_INTERVAL);
        }

        /* set the print_flag if all ports up or timeout */
        if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
            print_flag = 1;
            printf("done\n");
        }
    }
}

static void
signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\n\nSignal %d received, preparing to exit...\n",
            signum);
        force_quit = true;
    }
}

std::vector<timestamps_acc_number> MergeResult(std::vector<std::vector<timestamps_acc_number>> &arrs) {
    if (arrs.size() == 1) {
        return arrs[0];
    }
    std::vector<timestamps_acc_number>res;
    std::priority_queue<timestamps_acc_number, std::vector<timestamps_acc_number>, std::greater<timestamps_acc_number>>pq;

    std::vector<size_t>arr_pos(arrs.size(), 0);
    for (size_t i = 0;i < arrs.size();i++) {
        pq.push(arrs[i][0]);
    }

    while (!pq.empty()) {
        timestamps_acc_number top = pq.top();
        pq.pop();
        res.push_back(top);
        size_t arr_index = 0;
        for (size_t i = 0;i < arrs.size();i++) {
            if (arr_pos[i] < arrs[i].size() && arrs[i][arr_pos[i]] == top) {
                arr_index = i;
                break;
            }
        }
        arr_pos[arr_index]++;
        if (arr_pos[arr_index] < arrs[arr_index].size()) {
            pq.push(arrs[arr_index][arr_pos[arr_index]]);
        }
    }
    return res;
}


int main(int argc, char **argv) {
    struct server_queue_conf *qconf;
    int ret;
    uint16_t nb_ports;
    uint16_t nb_ports_available = 0;
    uint16_t portid;
    unsigned lcore_id, rx_lcore_id;
    unsigned int nb_lcores = 0;
    unsigned int nb_mbufs;
    int tx_rx_queue_count = 0;

    /* init EAL */
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
    argc -= ret;
    argv += ret;

    force_quit = false;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* convert to number of cycles */
    timer_period *= rte_get_timer_hz();

    nb_ports = rte_eth_dev_count_avail();
    if (nb_ports == 0)
        rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

    /* check port mask to possible port mask */
    if (l2fwd_enabled_port_mask & ~((1 << nb_ports) - 1))
        rte_exit(EXIT_FAILURE, "Invalid portmask; possible (0x%x)\n",
            (1 << nb_ports) - 1);

    rx_lcore_id = 0;
    qconf = NULL;

    /* Initialize the port/queue configuration of each logical core */
    RTE_ETH_FOREACH_DEV(portid) {

        ret = rte_eth_macaddr_get(portid,
            &l2fwd_ports_eth_addr[portid]);
        if (ret < 0)
            rte_exit(EXIT_FAILURE,
                "Cannot get MAC address: err=%d, port=%u\n",
                ret, portid);
        char name_buf[RTE_ETH_NAME_MAX_LEN];
        ret = rte_eth_dev_get_name_by_port(portid,
            name_buf);
        if (ret < 0)
            rte_exit(EXIT_FAILURE,
                "Cannot get port name: err=%d, port=%u\n",
                ret, portid);

        printf("Port %u %s, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
            portid, name_buf,
            l2fwd_ports_eth_addr[portid].addr_bytes[0],
            l2fwd_ports_eth_addr[portid].addr_bytes[1],
            l2fwd_ports_eth_addr[portid].addr_bytes[2],
            l2fwd_ports_eth_addr[portid].addr_bytes[3],
            l2fwd_ports_eth_addr[portid].addr_bytes[4],
            l2fwd_ports_eth_addr[portid].addr_bytes[5]);

        /* skip ports that are not enabled */
        if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
            continue;

        nb_ports_available++;
        tx_rx_queue_count = 0;
        while (tx_rx_queue_count < TX_RX_QUEUE) {
            /* get the lcore_id for this port */
            while (rte_lcore_is_enabled(rx_lcore_id) == 0 || rx_lcore_id == rte_get_main_lcore() ||
                server_queue_conf[rx_lcore_id].n_tx_rx_queue ==
                MAX_QUEUE_PER_LCORE) {
                rx_lcore_id++;
                if (rx_lcore_id >= RTE_MAX_LCORE)
                    rte_exit(EXIT_FAILURE, "Not enough cores\n");
            }

            if (qconf != &server_queue_conf[rx_lcore_id]) {
                /* Assigned a new logical core in the loop above. */
                qconf = &server_queue_conf[rx_lcore_id];
                nb_lcores++;
            }
            if (tx_rx_queue_count < TX_RX_QUEUE) {
                for (int i = 0; tx_rx_queue_count < TX_RX_QUEUE && i < MAX_QUEUE_PER_LCORE; i++) {
                    qconf->tx_rx_queue_list[i] = tx_rx_queue_count;
                    qconf->n_tx_rx_queue++;
                    tx_rx_queue_count++;
                }
                qconf->portid = portid;
                printf("Lcore %u: [R/T], port %d queue from %d to %d\n", rx_lcore_id, qconf->portid, qconf->tx_rx_queue_list[0], qconf->tx_rx_queue_list[qconf->n_tx_rx_queue - 1]);
            }
        }
    }

    nb_mbufs = RTE_MIN(300000, (nb_rxd + nb_txd) * nb_lcores + MEMPOOL_CACHE_SIZE * 2 * nb_lcores);
    server_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", nb_mbufs,
        MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id());
    if (server_pktmbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

    printf("addr %p length %ld %u\n", server_pktmbuf_pool->mem_list.stqh_first->addr, server_pktmbuf_pool->mem_list.stqh_first->len, server_pktmbuf_pool->nb_mem_chunks);

    total_dequeue_timestamp_acc.resize(TX_RX_QUEUE);
    total_inqueue_timestamp_acc.resize(TX_RX_QUEUE);
    for (int i = 0; i < TX_RX_QUEUE; i++) {
        total_dequeue_timestamp_acc[i].reserve(1000000000);
        total_inqueue_timestamp_acc[i].reserve(1000000000);
    }

    /* Initialise each port */
    RTE_ETH_FOREACH_DEV(portid) {
        struct rte_eth_rxconf rxq_conf;
        struct rte_eth_txconf txq_conf;
        struct rte_eth_dev_info dev_info;
        uint64_t rss_hf_tmp;

        /* skip ports that are not enabled */
        if ((l2fwd_enabled_port_mask & (1 << portid)) == 0) {
            printf("Skipping disabled port %u\n", portid);
            continue;
        }

        /* init port */
        printf("Initializing port %u... ", portid);
        fflush(stdout);

        ret = rte_eth_dev_info_get(portid, &dev_info);
        if (ret != 0)
            rte_exit(EXIT_FAILURE,
                "Error during getting device (port %u) info: %s\n",
                portid, strerror(-ret));

        if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
            port_conf.txmode.offloads |=
            RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

        rss_hf_tmp = port_conf.rx_adv_conf.rss_conf.rss_hf;
        port_conf.rx_adv_conf.rss_conf.rss_hf &= dev_info.flow_type_rss_offloads;
        if (port_conf.rx_adv_conf.rss_conf.rss_hf != rss_hf_tmp) {
            printf("Port %u modified RSS hash function based on hardware support,"
                "requested:%#" PRIx64 " configured:%#" PRIx64 "\n",
                portid,
                rss_hf_tmp,
                port_conf.rx_adv_conf.rss_conf.rss_hf);
        }
        ret = rte_eth_dev_configure(portid, TX_RX_QUEUE, TX_RX_QUEUE, &port_conf);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
                ret, portid);

        ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
            &nb_txd);
        if (ret < 0)
            rte_exit(EXIT_FAILURE,
                "Cannot adjust number of descriptors: err=%d, port=%u\n",
                ret, portid);

        ret = rte_eth_promiscuous_disable(portid);
        // ret = rte_eth_promiscuous_enable(portid);
        if (ret < 0)
            rte_exit(EXIT_FAILURE,
                "Can't set promiscuous: err=%d, port=%u\n",
                ret, portid);

        /* init RX queue */
        fflush(stdout);
        rxq_conf = dev_info.default_rxconf;
        rxq_conf.offloads = port_conf.rxmode.offloads;
        for (int i = 0; i < TX_RX_QUEUE; i++) {
            ret = rte_eth_rx_queue_setup(portid, i, nb_rxd,
                rte_eth_dev_socket_id(portid),
                &rxq_conf,
                server_pktmbuf_pool);
            if (ret < 0)
                rte_exit(EXIT_FAILURE, "rte_weth_rx_queue_setup:err=%d, port=%u\n",
                    ret, portid);
        }

        /* init TX queue */
        fflush(stdout);
        txq_conf = dev_info.default_txconf;
        txq_conf.offloads = port_conf.txmode.offloads;
        for (int i = 0; i < TX_RX_QUEUE; i++) {
            ret = rte_eth_tx_queue_setup(portid, i, nb_rxd,
                rte_eth_dev_socket_id(portid),
                &txq_conf);
            if (ret < 0)
                rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
                    ret, portid);
        }

        ret = rte_eth_dev_set_ptypes(portid, RTE_PTYPE_UNKNOWN, NULL,
            0);
        if (ret < 0)
            printf("Port %u, Failed to disable Ptype parsing\n",
                portid);

        /* Start device */
        ret = rte_eth_dev_start(portid);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
                ret, portid);

        printf("done: \n");
    }

    /* initialize port stats */
    memset(&port_statistics, 0, sizeof(port_statistics));
    memset(&port_statistics_period, 0, sizeof(port_statistics_period));

    if (!nb_ports_available) {
        rte_exit(EXIT_FAILURE,
            "All available ports are disabled. Please set portmask.\n");
    }

    check_all_ports_link_status(l2fwd_enabled_port_mask);

    ret = 0;
    /* launch per-lcore init on every lcore */
    rte_eal_mp_remote_launch(l2fwd_launch_one_lcore, NULL, CALL_MAIN);
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        if (rte_eal_wait_lcore(lcore_id) < 0) {
            ret = -1;
            break;
        }
    }

    RTE_ETH_FOREACH_DEV(portid) {
        if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
            continue;
        printf("Closing port %d...", portid);
        ret = rte_eth_dev_stop(portid);
        if (ret != 0)
            printf("rte_eth_dev_stop: err=%d, port=%d\n",
                ret, portid);
        rte_eth_dev_close(portid);
        printf(" Done\n");
    }

    /* clean up the EAL */
    rte_eal_cleanup();

    printf("stopped all threads, begin to sort!\n");
    // for (size_t i = 0;i < TX_RX_QUEUE;i++) {
    //     sort(total_inqueue_timestamp_acc[i].begin(), total_inqueue_timestamp_acc[i].end());
    //     sort(total_dequeue_timestamp_acc[i].begin(), total_dequeue_timestamp_acc[i].end());
    // }
    std::vector<timestamps_acc_number>inqueue_result = MergeResult(total_inqueue_timestamp_acc);
    std::vector<timestamps_acc_number>dequeue_result = MergeResult(total_dequeue_timestamp_acc);
    assert(inqueue_result.size() == dequeue_result.size());
    printf("sort success, begin to write result!\n");
    std::ofstream outfile("/home/cxz/study/stupid-doca/result.txt");
    for (size_t i = 0;i < (min_(inqueue_result.size(), 1000000));i++) {
        outfile << inqueue_result[i].timestamp << "," << inqueue_result[i].acc_number << "," << dequeue_result[i].timestamp << "," << dequeue_result[i].acc_number << std::endl;
    }
    outfile.close();
    printf("write result success!\n");
    printf("Bye...\n");

    return ret;
}
