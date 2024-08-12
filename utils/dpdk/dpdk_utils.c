/*
 * Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of NVIDIA CORPORATION &
 * AFFILIATES (the "Company") and all right, title, and interest in and to the
 * software product, including all associated intellectual property rights, are
 * and shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_sft.h>
#include <rte_malloc.h>

#include <doca_log.h>
#include <doca_mmap.h>
#include <doca_buf_inventory.h>

#include "dpdk_utils.h"

#ifdef GPU_SUPPORT
#include "gpu_init.h"
#endif

DOCA_LOG_REGISTER(NUTILS);

#define RSS_KEY_LEN 40

#ifndef IPv6_BYTES
#define IPv6_BYTES_FMT "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"\
			"%02x%02x:%02x%02x:%02x%02x:%02x%02x"
#define IPv6_BYTES(addr) \
	addr[0],  addr[1], addr[2],  addr[3], addr[4],  addr[5], addr[6],  addr[7], \
	addr[8],  addr[9], addr[10], addr[11], addr[12], addr[13], addr[14], addr[15]
#endif

struct dpdk_mempool_shadow {
    struct doca_dev *device;	/* DOCA device used to register memory */
    struct doca_mmap **mmap_arr;	/* DOCA mmap array that has mapped the packet buffers */
    uint32_t nb_mmaps;		/* Number of elements in mmap_arr */
};

/*
 * Bind port to all the peer ports
 *
 * @port_id [in]: port ID
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
bind_hairpin_queues(uint16_t port_id) {
    /* Configure the Rx and Tx hairpin queues for the selected port */
    int result = 0, peer_port, peer_ports_len;
    uint16_t peer_ports[RTE_MAX_ETHPORTS];

    /* bind current Tx to all peer Rx */
    peer_ports_len = rte_eth_hairpin_get_peer_ports(port_id, peer_ports, RTE_MAX_ETHPORTS, 1);
    if (peer_ports_len < 0) {
        DOCA_LOG_ERR("Failed to get hairpin peer Rx ports of port %d, (%d)", port_id, peer_ports_len);
        return DOCA_ERROR_DRIVER;
    }
    for (peer_port = 0; peer_port < peer_ports_len; peer_port++) {
        result = rte_eth_hairpin_bind(port_id, peer_ports[peer_port]);
        if (result < 0) {
            DOCA_LOG_ERR("Failed to bind hairpin queues (%d)", result);
            return DOCA_ERROR_DRIVER;
        }
    }
    /* bind all peer Tx to current Rx */
    peer_ports_len = rte_eth_hairpin_get_peer_ports(port_id, peer_ports, RTE_MAX_ETHPORTS, 0);
    if (peer_ports_len < 0) {
        DOCA_LOG_ERR("Failed to get hairpin peer Tx ports of port %d, (%d)", port_id, peer_ports_len);
        return DOCA_ERROR_DRIVER;
    }

    for (peer_port = 0; peer_port < peer_ports_len; peer_port++) {
        result = rte_eth_hairpin_bind(peer_ports[peer_port], port_id);
        if (result < 0) {
            DOCA_LOG_ERR("Failed to bind hairpin queues (%d)", result);
            return DOCA_ERROR_DRIVER;
        }
    }
    return DOCA_SUCCESS;
}

/*
 * Unbind port from all its peer ports
 *
 * @port_id [in]: port ID
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
unbind_hairpin_queues(uint16_t port_id) {
    /* Configure the Rx and Tx hairpin queues for the selected port */
    int result = 0, peer_port, peer_ports_len;
    uint16_t peer_ports[RTE_MAX_ETHPORTS];

    /* unbind current Tx from all peer Rx */
    peer_ports_len = rte_eth_hairpin_get_peer_ports(port_id, peer_ports, RTE_MAX_ETHPORTS, 1);
    if (peer_ports_len < 0) {
        DOCA_LOG_ERR("Failed to get hairpin peer Tx ports of port %d, (%d)", port_id, peer_ports_len);
        return DOCA_ERROR_DRIVER;
    }

    for (peer_port = 0; peer_port < peer_ports_len; peer_port++) {
        result = rte_eth_hairpin_unbind(port_id, peer_ports[peer_port]);
        if (result < 0) {
            DOCA_LOG_ERR("Failed to bind hairpin queues (%d)", result);
            return DOCA_ERROR_DRIVER;
        }
    }
    /* unbind all peer Tx from current Rx */
    peer_ports_len = rte_eth_hairpin_get_peer_ports(port_id, peer_ports, RTE_MAX_ETHPORTS, 0);
    if (peer_ports_len < 0) {
        DOCA_LOG_ERR("Failed to get hairpin peer Tx ports of port %d, (%d)", port_id, peer_ports_len);
        return DOCA_ERROR_DRIVER;
    }
    for (peer_port = 0; peer_port < peer_ports_len; peer_port++) {
        result = rte_eth_hairpin_unbind(peer_ports[peer_port], port_id);
        if (result < 0) {
            DOCA_LOG_ERR("Failed to bind hairpin queues (%d)", result);
            return DOCA_ERROR_DRIVER;
        }
    }
    return DOCA_SUCCESS;
}

/*
 * Set up all hairpin queues
 *
 * @port_id [in]: port ID
 * @peer_port_id [in]: peer port ID
 * @reserved_hairpin_q_list [in]: list of hairpin queues index
 * @hairpin_queue_len [in]: length of reserved_hairpin_q_list
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
setup_hairpin_queues(uint16_t port_id, uint16_t peer_port_id, uint16_t *reserved_hairpin_q_list, int hairpin_queue_len) {
    /* Port:
     *	0. RX queue
     *	1. RX hairpin queue rte_eth_rx_hairpin_queue_setup
     *	2. TX hairpin queue rte_eth_tx_hairpin_queue_setup
     */

    int result = 0, hairpin_q;
    uint16_t nb_tx_rx_desc = 2048;
    uint32_t manual = 1;
    uint32_t tx_exp = 1;
    struct rte_eth_hairpin_conf hairpin_conf = {
            .peer_count = 1,
            .manual_bind = !!manual,
            .tx_explicit = !!tx_exp,
            .peers[0] = {peer_port_id, 0},
    };

    for (hairpin_q = 0; hairpin_q < hairpin_queue_len; hairpin_q++) {
        // TX
        hairpin_conf.peers[0].queue = reserved_hairpin_q_list[hairpin_q];
        result = rte_eth_tx_hairpin_queue_setup(port_id, reserved_hairpin_q_list[hairpin_q], nb_tx_rx_desc,
            &hairpin_conf);
        if (result < 0) {
            DOCA_LOG_ERR("Failed to setup hairpin queues (%d)", result);
            return DOCA_ERROR_DRIVER;
        }

        // RX
        hairpin_conf.peers[0].queue = reserved_hairpin_q_list[hairpin_q];
        result = rte_eth_rx_hairpin_queue_setup(port_id, reserved_hairpin_q_list[hairpin_q], nb_tx_rx_desc,
            &hairpin_conf);
        if (result < 0) {
            DOCA_LOG_ERR("Failed to setup hairpin queues (%d)", result);
            return DOCA_ERROR_DRIVER;
        }
    }
    return DOCA_SUCCESS;
}

/*
 * Unbind hairpin queues from all ports
 *
 * @nb_ports [in]: number of ports
 */
static void
disable_hairpin_queues(uint16_t nb_ports) {
    doca_error_t result;
    uint16_t port_id;

    for (port_id = 0; port_id < nb_ports; port_id++) {
        if (!rte_eth_dev_is_valid_port(port_id))
            continue;
        result = unbind_hairpin_queues(port_id);
        if (result != DOCA_SUCCESS)
            DOCA_LOG_ERR("Disabling hairpin queues failed: err=%d, port=%u", result, port_id);
    }
}

/*
 * Bind hairpin queues to all ports
 *
 * @nb_ports [in]: number of ports
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
enable_hairpin_queues(uint8_t nb_ports) {
    uint16_t port_id;
    uint16_t n = 0;
    doca_error_t result;

    for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++) {
        if (!rte_eth_dev_is_valid_port(port_id))
            /* the device ID  might not be contiguous */
            continue;
        result = bind_hairpin_queues(port_id);
        if (result != DOCA_SUCCESS) {
            DOCA_LOG_ERR("Hairpin bind failed on port=%u", port_id);
            disable_hairpin_queues(port_id);
            return result;
        }
        if (++n >= nb_ports)
            break;
    }
    return DOCA_SUCCESS;
}

/*
 * Update rss action
 *
 * @queue_list [in]: queue indices to use
 * @nb_queues [in]: number of queues
 * @rss_conf [in]: rte_eth_rss_conf struct
 * @hash_func [in]: RSS hash function to apply
 * @level [in]: packet encapsulation level RSS hash types apply to
 * @action_rss [out]: updated rte_flow_action_rss struct
 */
static void
update_flow_action_rss(const uint16_t *queue_list, uint8_t nb_queues, const struct rte_eth_rss_conf *rss_conf,
    enum rte_eth_hash_function hash_func, uint32_t level, struct rte_flow_action_rss *action_rss) {
    action_rss->queue_num = nb_queues;
    action_rss->queue = queue_list;
    action_rss->types = rss_conf->rss_hf;
    action_rss->key_len = rss_conf->rss_key_len;
    action_rss->key = rss_conf->rss_key;
    action_rss->func = hash_func;
    action_rss->level = level;
}

/*
 * Initialize DPDK SFT offload rule
 *
 * @app_dpdk_config [in]: application DPDK configuration values
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
dpdk_sft_init(const struct application_dpdk_config *app_dpdk_config) {
    doca_error_t result;
    int ret = 0;
    uint8_t port_id = 0;
    uint8_t queue_index;
    uint8_t nb_queues = app_dpdk_config->port_config.nb_queues;
    uint8_t nb_hairpin_q = app_dpdk_config->port_config.nb_hairpin_q;
    uint16_t queue_list[nb_queues];
    uint16_t hairpin_queue_list[nb_hairpin_q];
    uint32_t level = 0;
    struct rte_sft_conf sft_config = {
            .nb_queues = nb_queues,					/* Num of queues */
            .nb_max_entries = (1 << 20),				/* Max number of connections */
            .tcp_ct_enable = app_dpdk_config->sft_config.enable_ct,	/* Enable TCP Connection Tracking */
            .ipfrag_enable = app_dpdk_config->sft_config.enable_frag,	/* Enable fragmented packets */
            .reorder_enable = 1,					/* Enable reorder */
            .default_aging = 60,					/* Default aging */
            .nb_max_ipfrag = 4096,					/* Max of IP fragments */
            .app_data_len = 1,						/* Application's data length */
    };
    struct rte_flow_action_rss action_rss;
    struct rte_flow_action_rss action_rss_hairpin;
    struct rte_sft_error sft_error;
    uint8_t rss_key[RSS_KEY_LEN];
    struct rte_eth_rss_conf rss_conf = {
            .rss_key = rss_key,
            .rss_key_len = RSS_KEY_LEN,
    };

    ret = rte_sft_init(&sft_config, &sft_error);
    if (ret < 0) {
        DOCA_LOG_ERR("SFT init failed, ret=%d", ret);
        return DOCA_ERROR_DRIVER;
    }

    ret = rte_eth_dev_rss_hash_conf_get(port_id, &rss_conf);
    if (ret < 0) {
        DOCA_LOG_ERR("Get port RSS configuration failed, ret=%d", ret);
        ret = rte_sft_fini(&sft_error);
        if (ret < 0)
            DOCA_LOG_ERR("SFT fini failed, error=%d", ret);
        return DOCA_ERROR_DRIVER;
    }

    for (queue_index = 0; queue_index < nb_queues; queue_index++)
        queue_list[queue_index] = queue_index;
    if (sft_config.ipfrag_enable)
        rss_conf.rss_hf = RTE_ETH_RSS_IP;
    update_flow_action_rss(queue_list, nb_queues, &rss_conf, RTE_ETH_HASH_FUNCTION_DEFAULT, level, &action_rss);

    for (queue_index = 0; queue_index < nb_hairpin_q; queue_index++)
        hairpin_queue_list[queue_index] = nb_queues + queue_index;
    update_flow_action_rss(hairpin_queue_list, nb_hairpin_q, &rss_conf,
        RTE_ETH_HASH_FUNCTION_DEFAULT, level, &action_rss_hairpin);

    result = create_rules_sft_offload(app_dpdk_config, &action_rss, &action_rss_hairpin);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create SFT offload rule");
        return result;
    }

    return DOCA_SUCCESS;
}

/*
 * Creates a new mempool in memory to hold the mbufs
 *
 * @total_nb_mbufs [in]: the number of elements in the mbuf pool
 * @mbuf_pool [out]: the allocated pool
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
allocate_mempool(const uint32_t total_nb_mbufs, struct rte_mempool **mbuf_pool) {
    *mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", total_nb_mbufs, MBUF_CACHE_SIZE, 0,
        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (*mbuf_pool == NULL) {
        DOCA_LOG_ERR("Cannot allocate mbuf pool");
        return DOCA_ERROR_DRIVER;
    }
    return DOCA_SUCCESS;
}

#ifdef GPU_SUPPORT
/*
 * Unmap GPU resources
 *
 * @app_dpdk_config [in]: application DPDK configuration values
 */
static void
dpdk_gpu_unmap(struct application_dpdk_config *app_dpdk_config) {
    int result = 0;
    uint16_t port_id;
    uint16_t n;

    for (port_id = 0, n = 0; port_id < RTE_MAX_ETHPORTS; port_id++) {
        if (!rte_eth_dev_is_valid_port(port_id))
            continue;
        struct rte_eth_dev_info dev_info;
        struct rte_pktmbuf_extmem *ext_mem = &app_dpdk_config->pipe.ext_mem;

        result = rte_eth_dev_info_get(port_id, &dev_info);
        if (result != 0)
            DOCA_LOG_ERR("Failed getting device (port %u) info, error=%s", port_id, strerror(-result));

        result = rte_dev_dma_unmap(dev_info.device, ext_mem->buf_ptr, ext_mem->buf_iova, ext_mem->buf_len);
        if (result != 0)
            DOCA_LOG_ERR("Could not DMA unmap EXT memory");
        if (++n >= app_dpdk_config->port_config.nb_ports)
            break;
    }
}
#endif

/*
 * Initialize all the port resources
 *
 * @mbuf_pool [in]: packet mbuf pool
 * @port [in]: the port ID
 * @app_config [in]: application DPDK configuration values
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
port_init(struct rte_mempool *mbuf_pool, uint8_t port, struct application_dpdk_config *app_config) {
    doca_error_t result;
    int ret = 0;
    int symmetric_hash_key_length = RSS_KEY_LEN;
    const uint16_t nb_hairpin_queues = app_config->port_config.nb_hairpin_q;
    const uint16_t rx_rings = app_config->port_config.nb_queues;
    const uint16_t tx_rings = app_config->port_config.nb_queues;
    const uint16_t rss_support = !!(app_config->port_config.rss_support &&
        (app_config->port_config.nb_queues > 1));
    bool isolated = !!app_config->port_config.isolated_mode;
    uint16_t q, queue_index;
    uint16_t rss_queue_list[nb_hairpin_queues];
    struct rte_ether_addr addr;
    struct rte_eth_dev_info dev_info;
    struct rte_flow_error error;
    uint8_t symmetric_hash_key[RSS_KEY_LEN] = {
            0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
            0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
            0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
    };
    const struct rte_eth_conf port_conf_default = {
            .lpbk_mode = app_config->port_config.lpbk_support,
            .rx_adv_conf = {
                    .rss_conf = {
                            .rss_key_len = symmetric_hash_key_length,
                            .rss_key = symmetric_hash_key,
                            .rss_hf = (RTE_ETH_RSS_IP | RTE_ETH_RSS_UDP | RTE_ETH_RSS_TCP),
                    },
            },
    };
    struct rte_eth_conf port_conf = port_conf_default;

    ret = rte_eth_dev_info_get(port, &dev_info);
    if (ret < 0) {
        DOCA_LOG_ERR("Failed getting device (port %u) info, error=%s", port, strerror(-ret));
        return DOCA_ERROR_DRIVER;
    }
    port_conf.rxmode.mq_mode = rss_support ? RTE_ETH_MQ_RX_RSS : RTE_ETH_MQ_RX_NONE;

#ifdef GPU_SUPPORT
    if (app_config->pipe.gpu_support) {
        struct rte_pktmbuf_extmem *ext_mem = &app_config->pipe.ext_mem;

        /* Mapped the memory region to the devices (ports) */
        DOCA_LOG_DBG("GPU Support, DMA map to GPU");
        ret = rte_dev_dma_map(dev_info.device, ext_mem->buf_ptr, ext_mem->buf_iova, ext_mem->buf_len);
        if (ret < 0) {
            DOCA_LOG_ERR("Could not DMA map EXT memory - (%d)", ret);
            return DOCA_ERROR_DRIVER;
        }
    }
#endif
    /* Configure the Ethernet device */
    ret = rte_eth_dev_configure(port, rx_rings + nb_hairpin_queues, tx_rings + nb_hairpin_queues, &port_conf);
    if (ret < 0) {
        DOCA_LOG_ERR("Failed to configure the ethernet device - (%d)", ret);
        return DOCA_ERROR_DRIVER;
    }
    if (port_conf_default.rx_adv_conf.rss_conf.rss_hf != port_conf.rx_adv_conf.rss_conf.rss_hf) {
        DOCA_LOG_DBG("Port %u modified RSS hash function based on hardware support, requested:%#" PRIx64
            " configured:%#" PRIx64 "",
            port, port_conf_default.rx_adv_conf.rss_conf.rss_hf,
            port_conf.rx_adv_conf.rss_conf.rss_hf);
    }

    /* Enable RX in promiscuous mode for the Ethernet device */
    ret = rte_eth_promiscuous_enable(port);
    if (ret < 0) {
        DOCA_LOG_ERR("Failed to Enable RX in promiscuous mode - (%d)", ret);
        return DOCA_ERROR_DRIVER;
    }

    /* Allocate and set up RX queues according to number of cores per Ethernet port */
    for (q = 0; q < rx_rings; q++) {
        ret = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (ret < 0) {
            DOCA_LOG_ERR("Failed to set up RX queues - (%d)", ret);
            return DOCA_ERROR_DRIVER;
        }
    }

    /* Allocate and set up TX queues according to number of cores per Ethernet port */
    for (q = 0; q < tx_rings; q++) {
        ret = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE, rte_eth_dev_socket_id(port), NULL);
        if (ret < 0) {
            DOCA_LOG_ERR("Failed to set up TX queues - (%d)", ret);
            return DOCA_ERROR_DRIVER;
        }
    }

    /* Enabled hairpin queue before port start */
    if (nb_hairpin_queues && app_config->port_config.self_hairpin && rte_eth_dev_is_valid_port(port ^ 1)) {
        /* Hairpin to both self and peer */
        assert((nb_hairpin_queues % 2) == 0);
        for (queue_index = 0; queue_index < nb_hairpin_queues / 2; queue_index++)
            rss_queue_list[queue_index] = app_config->port_config.nb_queues + queue_index * 2;
        result = setup_hairpin_queues(port, port, rss_queue_list, nb_hairpin_queues / 2);
        if (result != DOCA_SUCCESS) {
            DOCA_LOG_ERR("Cannot hairpin self port %" PRIu8 ", ret: %s",
                port, doca_error_get_descr(result));
            return result;
        }
        for (queue_index = 0; queue_index < nb_hairpin_queues / 2; queue_index++)
            rss_queue_list[queue_index] = app_config->port_config.nb_queues + queue_index * 2 + 1;
        result = setup_hairpin_queues(port, port ^ 1, rss_queue_list, nb_hairpin_queues / 2);
        if (result != DOCA_SUCCESS) {
            DOCA_LOG_ERR("Cannot hairpin peer port %" PRIu8 ", ret: %s",
                port ^ 1, doca_error_get_descr(result));
            return result;
        }
    } else if (nb_hairpin_queues) {
        /* Hairpin to self or peer */
        for (queue_index = 0; queue_index < nb_hairpin_queues; queue_index++)
            rss_queue_list[queue_index] = app_config->port_config.nb_queues + queue_index;
        if (rte_eth_dev_is_valid_port(port ^ 1))
            result = setup_hairpin_queues(port, port ^ 1, rss_queue_list, nb_hairpin_queues);
        else
            result = setup_hairpin_queues(port, port, rss_queue_list, nb_hairpin_queues);
        if (result != DOCA_SUCCESS) {
            DOCA_LOG_ERR("Cannot hairpin port %" PRIu8 ", ret=%d", port, result);
            return result;
        }
    }

    /* Set isolated mode (true or false) before port start */
    ret = rte_flow_isolate(port, isolated, &error);
    if (ret < 0) {
        DOCA_LOG_ERR("Port %u could not be set isolated mode to %s (%s)",
            port, isolated ? "true" : "false", error.message);
        return DOCA_ERROR_DRIVER;
    }
    if (isolated)
        DOCA_LOG_INFO("Ingress traffic on port %u is in isolated mode",
            port);

    /* Start the Ethernet port */
    ret = rte_eth_dev_start(port);
    if (ret < 0) {
        DOCA_LOG_ERR("Cannot start port %" PRIu8 ", ret=%d", port, ret);
        return DOCA_ERROR_DRIVER;
    }

    /* Display the port MAC address */
    rte_eth_macaddr_get(port, &addr);
    DOCA_LOG_DBG("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "",
        (unsigned int)port, addr.addr_bytes[0], addr.addr_bytes[1], addr.addr_bytes[2], addr.addr_bytes[3],
        addr.addr_bytes[4], addr.addr_bytes[5]);

    /*
     * Check that the port is on the same NUMA node as the polling thread
     * for best performance.
     */
    if (rte_eth_dev_socket_id(port) > 0 && rte_eth_dev_socket_id(port) != (int)rte_socket_id()) {
        DOCA_LOG_WARN("Port %u is on remote NUMA node to polling thread", port);
        DOCA_LOG_WARN("\tPerformance will not be optimal");
    }
    return DOCA_SUCCESS;
}

/*
 * Destroy all DPDK ports
 *
 * @app_dpdk_config [in]: application DPDK configuration values
 * @nb_ports [in]: number of ports to destroy
 */
static void
dpdk_ports_fini(struct application_dpdk_config *app_dpdk_config, uint16_t nb_ports) {
    int result;
    int port_id;
    uint16_t n;

    for (port_id = nb_ports, n = 0; port_id >= 0; port_id--) {
        if (!rte_eth_dev_is_valid_port(port_id))
            continue;
        result = rte_eth_dev_stop(port_id);
        if (result != 0)
            DOCA_LOG_ERR("rte_eth_dev_stop(): err=%d, port=%u", result, port_id);

        result = rte_eth_dev_close(port_id);
        if (result != 0)
            DOCA_LOG_ERR("rte_eth_dev_close(): err=%d, port=%u", result, port_id);
        if (++n >= app_dpdk_config->port_config.nb_ports)
            break;
    }

    /* Free the memory pool used by the ports for rte_pktmbufs */
    if (app_dpdk_config->mbuf_pool != NULL)
        rte_mempool_free(app_dpdk_config->mbuf_pool);
#ifdef GPU_SUPPORT
    if (app_dpdk_config->pipe.gpu_support) {
        rte_free(app_dpdk_config->pipe.ext_mem.buf_ptr);
        app_dpdk_config->pipe.ext_mem.buf_ptr = NULL;
    }
#endif
}

/*
 * Initialize all DPDK ports
 *
 * @app_config [in]: application DPDK configuration values
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
dpdk_ports_init(struct application_dpdk_config *app_config) {
    doca_error_t result;
    int ret;
    uint16_t port_id;
    uint16_t n;
    const uint16_t nb_ports = app_config->port_config.nb_ports;
    const uint32_t total_nb_mbufs = app_config->port_config.nb_queues * nb_ports * NUM_MBUFS;

    /* Initialize mbufs mempool */
#ifdef GPU_SUPPORT
    if (app_config->pipe.gpu_support)
        result = allocate_mempool_gpu(total_nb_mbufs, &app_config->pipe, &app_config->mbuf_pool);
    else
        result = allocate_mempool(total_nb_mbufs, &app_config->mbuf_pool);
#else
    result = allocate_mempool(total_nb_mbufs, &app_config->mbuf_pool);
#endif
    if (result != DOCA_SUCCESS)
        return result;

    /*
     * It's a must for any SFT usage.
     * Enable metadata to be delivered to application in the packets mbuf, the metadata is user configurable,
     * with DOCA Flow offering a metadata scheme
     */
    if (app_config->port_config.enable_mbuf_metadata || app_config->sft_config.enable) {
        ret = rte_flow_dynf_metadata_register();
        if (ret < 0) {
            DOCA_LOG_ERR("Metadata register failed, ret=%d", ret);
            return DOCA_ERROR_DRIVER;
        }
    }

    for (port_id = 0, n = 0; port_id < RTE_MAX_ETHPORTS; port_id++) {
        if (!rte_eth_dev_is_valid_port(port_id))
            continue;
        result = port_init(app_config->mbuf_pool, port_id, app_config);
        if (result != DOCA_SUCCESS) {
            DOCA_LOG_ERR("Cannot init port %" PRIu8, port_id);
            dpdk_ports_fini(app_config, port_id);
#ifdef GPU_SUPPORT
            if (app_config->pipe.gpu_support)
                dpdk_gpu_unmap(app_config);
#endif
            return result;
        }
        if (++n >= nb_ports)
            break;
    }
    return DOCA_SUCCESS;
}

doca_error_t
dpdk_queues_and_ports_init(struct application_dpdk_config *app_dpdk_config) {
    doca_error_t result;
    int ret = 0;

    /* Check that DPDK enabled the required ports to send/receive on */
    ret = rte_eth_dev_count_avail();
    if (app_dpdk_config->port_config.nb_ports > 0 && ret < app_dpdk_config->port_config.nb_ports) {
        DOCA_LOG_ERR("Application will only function with %u ports, num_of_ports=%d",
            app_dpdk_config->port_config.nb_ports, ret);
        return DOCA_ERROR_DRIVER;
    }

    /* Check for available logical cores */
    ret = rte_lcore_count();
    if (app_dpdk_config->port_config.nb_queues > 0 && ret < app_dpdk_config->port_config.nb_queues) {
        DOCA_LOG_ERR("At least %u cores are needed for the application to run, available_cores=%d",
            app_dpdk_config->port_config.nb_queues, ret);
        return DOCA_ERROR_DRIVER;
    }
    app_dpdk_config->port_config.nb_queues = ret;

    if (app_dpdk_config->reserve_main_thread)
        app_dpdk_config->port_config.nb_queues -= 1;
#ifdef GPU_SUPPORT
    /* Enable GPU device and initialization the resources */
    if (app_dpdk_config->pipe.gpu_support) {
        DOCA_LOG_DBG("Enabling GPU support");
        gpu_init(&app_dpdk_config->pipe);
    }
#endif

    if (app_dpdk_config->port_config.nb_ports > 0) {
        result = dpdk_ports_init(app_dpdk_config);
        if (result != DOCA_SUCCESS) {
            DOCA_LOG_ERR("Ports allocation failed");
            goto gpu_cleanup;
        }
    }

    /* Enable hairpin queues */
    if (app_dpdk_config->port_config.nb_hairpin_q > 0) {
        result = enable_hairpin_queues(app_dpdk_config->port_config.nb_ports);
        if (result != DOCA_SUCCESS)
            goto ports_cleanup;
    }

    if (app_dpdk_config->sft_config.enable) {
        result = dpdk_sft_init(app_dpdk_config);
        if (result != DOCA_SUCCESS)
            goto hairpin_queues_cleanup;
    }

    return DOCA_SUCCESS;

hairpin_queues_cleanup:
    disable_hairpin_queues(RTE_MAX_ETHPORTS);
ports_cleanup:
    dpdk_ports_fini(app_dpdk_config, RTE_MAX_ETHPORTS);
#ifdef GPU_SUPPORT
    if (app_dpdk_config->pipe.gpu_support)
        dpdk_gpu_unmap(app_dpdk_config);
#endif
gpu_cleanup:
#ifdef GPU_SUPPORT
    if (app_dpdk_config->pipe.gpu_support)
        gpu_fini(&(app_dpdk_config->pipe));
#endif
    return result;
}

void
dpdk_queues_and_ports_fini(struct application_dpdk_config *app_dpdk_config) {
    int result = 0;
    struct rte_sft_error sft_error;
#ifdef GPU_SUPPORT
    if (app_dpdk_config->pipe.gpu_support) {
        DOCA_LOG_DBG("GPU support cleanup");
        gpu_fini(&(app_dpdk_config->pipe));
    }
#endif

    if (app_dpdk_config->sft_config.enable) {
        print_offload_rules_counter();
        result = rte_sft_fini(&sft_error);
        if (result < 0)
            DOCA_LOG_ERR("SFT fini failed, error=%d", result);
    }

#ifdef GPU_SUPPORT
    if (app_dpdk_config->pipe.gpu_support)
        dpdk_gpu_unmap(app_dpdk_config);
#endif

    disable_hairpin_queues(RTE_MAX_ETHPORTS);

    dpdk_ports_fini(app_dpdk_config, RTE_MAX_ETHPORTS);
}

/*
 * Print ether address
 *
 * @dmac [in]: destination mac address
 * @smac [in]: source mac address
 * @ethertype [in]: eth type
 */
static void
print_ether_addr(const struct rte_ether_addr *dmac, const struct rte_ether_addr *smac,
    const uint32_t ethertype) {
    char dmac_buf[RTE_ETHER_ADDR_FMT_SIZE];
    char smac_buf[RTE_ETHER_ADDR_FMT_SIZE];

    rte_ether_format_addr(dmac_buf, RTE_ETHER_ADDR_FMT_SIZE, dmac);
    rte_ether_format_addr(smac_buf, RTE_ETHER_ADDR_FMT_SIZE, smac);
    DOCA_LOG_DBG("DMAC=%s, SMAC=%s, ether_type=0x%04x", dmac_buf, smac_buf, ethertype);
}

/*
 * Print L2 header
 *
 * @packet [in]: packet mbuf
 */
static void
print_l2_header(const struct rte_mbuf *packet) {
    struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(packet, struct rte_ether_hdr *);

    print_ether_addr(&eth_hdr->dst_addr, &eth_hdr->src_addr, htonl(eth_hdr->ether_type) >> 16);
}

/*
 * Print IPV4 address
 *
 * @dip [in]: destination IP address
 * @sip [in]: source IP address
 * @packet_type [in]: packet type
 */
static void
print_ipv4_addr(const rte_be32_t dip, const rte_be32_t sip, const char *packet_type) {
    DOCA_LOG_DBG("DIP=%d.%d.%d.%d, SIP=%d.%d.%d.%d, %s",
        (dip & 0xff000000) >> 24,
        (dip & 0x00ff0000) >> 16,
        (dip & 0x0000ff00) >> 8,
        (dip & 0x000000ff),
        (sip & 0xff000000) >> 24,
        (sip & 0x00ff0000) >> 16,
        (sip & 0x0000ff00) >> 8,
        (sip & 0x000000ff),
        packet_type);
}

/*
 * Print IPV6 address
 *
 * @dst_addr [in]: destination IP address
 * @src_addr [in]: source IP address
 * @packet_type [in]: packet type
 */
static void
print_ipv6_addr(const uint8_t dst_addr[16], const uint8_t src_addr[16], const char *packet_type) {
    DOCA_LOG_DBG("DIPv6="IPv6_BYTES_FMT", SIPv6="IPv6_BYTES_FMT", %s",
        IPv6_BYTES(dst_addr), IPv6_BYTES(src_addr), packet_type);
}

/*
 * Print L3 header
 *
 * @packet [in]: packet mbuf
 */
static void
print_l3_header(const struct rte_mbuf *packet) {
    if (RTE_ETH_IS_IPV4_HDR(packet->packet_type)) {
        struct rte_ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(packet,
            struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));

        print_ipv4_addr(htonl(ipv4_hdr->dst_addr), htonl(ipv4_hdr->src_addr),
            rte_get_ptype_l4_name(packet->packet_type));
    } else if (RTE_ETH_IS_IPV6_HDR(packet->packet_type)) {
        struct rte_ipv6_hdr *ipv6_hdr = rte_pktmbuf_mtod_offset(packet,
            struct rte_ipv6_hdr *, sizeof(struct rte_ether_hdr));

        print_ipv6_addr(ipv6_hdr->dst_addr, ipv6_hdr->src_addr,
            rte_get_ptype_l4_name(packet->packet_type));
    }
}

/*
 * Print L4 header
 *
 * @packet [in]: packet mbuf
 */
static void
print_l4_header(const struct rte_mbuf *packet) {
    uint8_t *l4_hdr;
    struct rte_ipv4_hdr *ipv4_hdr;
    const struct rte_tcp_hdr *tcp_hdr;
    const struct rte_udp_hdr *udp_hdr;

    if (!RTE_ETH_IS_IPV4_HDR(packet->packet_type))
        return;

    ipv4_hdr = rte_pktmbuf_mtod_offset(packet,
        struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
    l4_hdr = (typeof(l4_hdr))ipv4_hdr + rte_ipv4_hdr_len(ipv4_hdr);

    switch (ipv4_hdr->next_proto_id) {
    case IPPROTO_UDP:
        udp_hdr = (typeof(udp_hdr))l4_hdr;
        DOCA_LOG_DBG("UDP- DPORT %u, SPORT %u",
            rte_be_to_cpu_16(udp_hdr->dst_port),
            rte_be_to_cpu_16(udp_hdr->src_port));
        break;

    case IPPROTO_TCP:
        tcp_hdr = (typeof(tcp_hdr))l4_hdr;
        DOCA_LOG_DBG("TCP- DPORT %u, SPORT %u",
            rte_be_to_cpu_16(tcp_hdr->dst_port),
            rte_be_to_cpu_16(tcp_hdr->src_port));
        break;

    default:
        DOCA_LOG_DBG("Unsupported L4 protocol!");
    }
}

/*
 * Callback which creates a DOCA mmap out of a RTE chunk
 * This callback can be used with 'rte_mempool_mem_iter()'
 *
 * @mp [in]: The RTE memory pool
 * @opaque [in]: Opaque representing pointer to 'struct dpdk_mempool_shadow'
 * @memhdr [in]: Header describing the RTE chunk
 * @mem_idx [in]: Index of the RTE chunk
 */
static void
dpdk_chunk_to_mmap_cb(struct rte_mempool *mp, void *opaque, struct rte_mempool_memhdr *memhdr, unsigned int mem_idx) {
    (void)mp;
    (void)mem_idx;

    struct dpdk_mempool_shadow *mempool_shadow = opaque;
    struct doca_mmap *new_mmap;
    doca_error_t result;

    result = doca_mmap_create(&new_mmap);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to create mmap");
        return;
    }

    result = doca_mmap_set_memrange(new_mmap, memhdr->addr, memhdr->len);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to set memory range of memory map (input): %s", doca_error_get_descr(result));
        doca_mmap_destroy(new_mmap);
        return;
    }
    result = doca_mmap_add_dev(new_mmap, mempool_shadow->device);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to add device to mmap: %s", doca_error_get_descr(result));
        doca_mmap_destroy(new_mmap);
        return;
    }

    result = doca_mmap_start(new_mmap);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to start memory map: %s", doca_error_get_descr(result));
        doca_mmap_destroy(new_mmap);
        return;
    }

    mempool_shadow->mmap_arr[mempool_shadow->nb_mmaps++] = new_mmap;
}

struct dpdk_mempool_shadow *
    dpdk_mempool_shadow_create(struct rte_mempool *mbuf_pool, struct doca_dev *device) {
    uint32_t nb_iterated_chunks, nb_chunks;
    struct dpdk_mempool_shadow *mempool_shadow = rte_zmalloc(NULL, sizeof(*mempool_shadow), 0);

    if (mempool_shadow == NULL) {
        DOCA_LOG_ERR("Dynamic allocation failed");
        return NULL;
    }
    mempool_shadow->device = device;

    nb_chunks = mbuf_pool->nb_mem_chunks;
    mempool_shadow->mmap_arr = (struct doca_mmap **)rte_zmalloc(NULL, sizeof(struct doca_mmap *) * nb_chunks, 0);
    if (mempool_shadow->mmap_arr == NULL) {
        DOCA_LOG_ERR("Dynamic allocation failed");
        dpdk_mempool_shadow_destroy(mempool_shadow);
        return NULL;
    }

    /* Register each chunk in the mempool to an mmap */
    nb_iterated_chunks = rte_mempool_mem_iter(mbuf_pool, dpdk_chunk_to_mmap_cb, mempool_shadow);
    if (nb_iterated_chunks != mempool_shadow->nb_mmaps) {
        dpdk_mempool_shadow_destroy(mempool_shadow);
        return NULL;
    }

    return mempool_shadow;
}


/*
 * Function which creates a DOCA mmap out of a RTE external chunk.
 * This function will work with any chunk, not only RTE one.
 *
 * @mempool_shadow [in]: Pointer to 'struct dpdk_mempool_shadow'
 * @chunk [in]: Contiguous memory used as external chunk to DPDK
 * @len [in]: The length of chunk in bytes
 * @return: DOCA_SUCCESS on success, and doca_error_t otherwise
 */
static doca_error_t
dpdk_ext_chunk_to_mmap(struct dpdk_mempool_shadow *mempool_shadow, void *chunk, size_t len) {
    struct doca_mmap *new_mmap;
    doca_error_t result;

    result = doca_mmap_create(&new_mmap);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to create mmap");
        return result;
    }

    result = doca_mmap_set_memrange(new_mmap, chunk, len);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to set memory range of memory map (input): %s", doca_error_get_descr(result));
        doca_mmap_destroy(new_mmap);
        return result;
    }

    result = doca_mmap_add_dev(new_mmap, mempool_shadow->device);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to add device to mmap: %s", doca_error_get_descr(result));
        doca_mmap_destroy(new_mmap);
        return result;
    }

    result = doca_mmap_start(new_mmap);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to start memory map: %s", doca_error_get_descr(result));
        doca_mmap_destroy(new_mmap);
        return result;
    }

    mempool_shadow->mmap_arr[mempool_shadow->nb_mmaps++] = new_mmap;

    return DOCA_SUCCESS;
}

struct dpdk_mempool_shadow *
    dpdk_mempool_shadow_create_extbuf(const struct rte_pktmbuf_extmem **ext_mem, uint32_t ext_num, struct doca_dev *device) {
    doca_error_t result;
    uint32_t i;
    struct dpdk_mempool_shadow *mempool_shadow = rte_zmalloc(NULL, sizeof(*mempool_shadow), 0);

    if (mempool_shadow == NULL) {
        DOCA_LOG_ERR("Dynamic allocation failed");
        return NULL;
    }
    mempool_shadow->device = device;

    mempool_shadow->mmap_arr = (struct doca_mmap **)rte_zmalloc(NULL, sizeof(struct doca_mmap *) * ext_num, 0);
    if (mempool_shadow->mmap_arr == NULL) {
        DOCA_LOG_ERR("Dynamic allocation failed");
        dpdk_mempool_shadow_destroy(mempool_shadow);
        return NULL;
    }

    /* Register each chunk in the mempool to an mmap */
    for (i = 0; i < ext_num; ++i) {
        result = dpdk_ext_chunk_to_mmap(mempool_shadow, ext_mem[i]->buf_ptr, ext_mem[i]->buf_len);
        if (result != DOCA_SUCCESS) {
            dpdk_mempool_shadow_destroy(mempool_shadow);
            return NULL;
        }
    }

    return mempool_shadow;
}

void
dpdk_mempool_shadow_destroy(struct dpdk_mempool_shadow *mempool_shadow) {
    uint32_t mmap_idx;

    if (mempool_shadow->mmap_arr != NULL) {
        for (mmap_idx = 0; mmap_idx < mempool_shadow->nb_mmaps; mmap_idx++)
            doca_mmap_destroy(mempool_shadow->mmap_arr[mmap_idx]);
        rte_free(mempool_shadow->mmap_arr);
    }
    rte_free(mempool_shadow);
}

doca_error_t
dpdk_mempool_shadow_find_buf_by_data(struct dpdk_mempool_shadow *mempool_shadow, struct doca_buf_inventory *inventory,
    uintptr_t mem_range_start, size_t mem_range_size, struct doca_buf **out_buf) {
    uintptr_t mmap_begin;
    size_t mmap_len;
    uint32_t mmap_idx;

    /* For most cases, only single mmap exists */
    for (mmap_idx = 0; mmap_idx < mempool_shadow->nb_mmaps; mmap_idx++) {
        struct doca_mmap *mmap = mempool_shadow->mmap_arr[mmap_idx];

        doca_mmap_get_memrange(mmap, (void **)&mmap_begin, &mmap_len);
        /* Following checks that memory range is within the mmap while avoiding integer overflow */
        if (mmap_begin <= mem_range_start &&
            mem_range_start < mmap_begin + mmap_len &&
            mem_range_size <= mmap_begin + mmap_len - mem_range_start)
            return doca_buf_inventory_buf_get_by_data(inventory, mmap, (void *)mem_range_start, mem_range_size,
                out_buf);
    }

    return DOCA_ERROR_NOT_FOUND;
}

void
print_header_info(const struct rte_mbuf *packet, const bool l2, const bool l3, const bool l4) {
    if (l2)
        print_l2_header(packet);
    if (l3)
        print_l3_header(packet);
    if (l4)
        print_l4_header(packet);
}

doca_error_t
dpdk_init(int argc, char **argv) {
    int result;

    result = rte_eal_init(argc, argv);
    if (result < 0) {
        DOCA_LOG_ERR("EAL initialization failed");
        return DOCA_ERROR_DRIVER;
    }
    return DOCA_SUCCESS;
}

void
dpdk_fini(void) {
    int result;

    result = rte_eal_cleanup();
    if (result < 0) {
        DOCA_LOG_ERR("rte_eal_cleanup() failed, error=%d", result);
        return;
    }

    DOCA_LOG_DBG("DPDK fini is done");
}
