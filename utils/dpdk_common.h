#pragma once

#include "common.hpp"

#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_thash.h>



#define RECEIVE_TYPE 0
#define TRANSMIT_TYPE 1
#define DELAY_TYPE 2


#define MEMPOOL_CACHE_SIZE 256
#define MAX_QUEUE_PER_LCORE 1

// bf2 bond0 mac addr
struct rte_ether_addr DST_ADDR1 = { {0xa0, 0x88, 0xc2, 0x31, 0xf7, 0xee} };
struct rte_ether_addr DST_ADDR2 = { {0xa0, 0x88, 0xc2, 0x32, 0x04, 0x40} };

const char *MEMPOOL_NAME = "mbuf_pool";

static uint8_t rss_key[40] = { 0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
                              0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
                              0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
                              0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
                              0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa };

struct rte_ether_addr increment_mac_address(struct rte_ether_addr mac) {
    for (int i = RTE_ETHER_ADDR_LEN - 1; i >= 0; --i) {
        if (mac.addr_bytes[i] != 0xFF) {
            mac.addr_bytes[i]++;
            break;
        } else {
            mac.addr_bytes[i] = 0x00;
        }
    }
    return mac;
}


struct Object_8 {
    uint8_t data[8];
    /* data */
};

struct Object_16 {
    uint8_t data[16];
    /* data */
};

struct Object_22 {
    uint8_t data[22];
    /* data */
};

struct Object_32 {
    uint8_t data[32];
    /* data */
};

struct Object_64 {
    uint8_t data[64];
    /* data */
} __rte_cache_aligned;

struct Object_86 {
    uint8_t data[86];
    /* data */
};

struct Object_128 {
    uint8_t data[128];
    /* data */
} __rte_cache_aligned;

struct Object_214 {
    uint8_t data[214];
    /* data */
};

struct Object_256 {
    uint8_t data[256];
    /* data */
} __rte_cache_aligned;

struct Object_512 {
    uint8_t data[512];
    /* data */
} __rte_cache_aligned;

struct Object_470 {
    uint8_t data[470];
    /* data */
};

struct Object_1k {
    uint8_t data[982];
    /* data */
};
struct Object_1024 {
    uint8_t data[1024];
    /* data */
} __rte_cache_aligned;

struct Object_1_5k {
    uint8_t data[1458];
};

struct Object_1472 {
    uint8_t data[1472];
    /* data */
};

struct Object_2k {
    uint8_t data[2006];
};

struct Object_2048 {
    uint8_t data[2048];
};

struct Object_4k {
    uint8_t data[4054];
};

struct Object_4096 {
    uint8_t data[4096];
};

struct Object_8k {
    uint8_t data[8150];
};

struct Object_8192 {
    uint8_t data[8192];
};

enum class lcore_type {
    RECEIVE,
    SEND,
    DMA_ENGINE,
};

struct server_queue_conf {
    unsigned n_tx_rx_queue;
    unsigned tx_rx_queue_list[MAX_QUEUE_PER_LCORE];
    uint16_t portid;
    lcore_type type;
} __rte_cache_aligned;


struct client_queue_conf {
    uint8_t type; // 0 is rx and 1 is tx
    uint32_t port_id;
    unsigned n_rx_queue;
    unsigned rx_queue_list[MAX_QUEUE_PER_LCORE];
    unsigned n_tx_queue;
    unsigned tx_queue_list[MAX_QUEUE_PER_LCORE];
} __rte_cache_aligned;
