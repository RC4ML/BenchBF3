#pragma once

/* Convert logarithm to value */
#define L2V(l) (1UL << (l))
/* Convert logarithm to mask */
#define L2M(l) (L2V(l) - 1)

#if defined(__x86_64__) || defined(__aarch64__)

#include <libflexio/flexio.h>

#define DevP(type) flexio_uintptr_t

#include <stdio.h>

#elif defined(__riscv)
#define DevP(type) type *

#include <libflexio-dev/flexio_dev.h>

#else
#define DevP(type)
#endif

#define LOG_LEVEL_OFF 0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_DEBUG 2
#define LOG_LEVEL_INFO 3

#define CUR_LOG_LEVEL LOG_LEVEL_INFO

#ifndef CUR_LOG_LEVEL
#define CUR_LOG_LEVEL LOG_LEVEL_OFF
#endif

static inline const char *__get_file_name(const char *file_name) {
    if (strrchr(file_name, '/') == NULL) {
        return file_name;
    }
    return strrchr(file_name, '/') + 1;
}

#define __FILENAME__ __get_file_name(__FILE__)

#if defined(__x86_64__) || defined(__aarch64__)
#define LOG_ERROR HOST_LOG_ERROR
#define LOG_DEBUG HOST_LOG_DEBUG
#define LOG_INFO HOST_LOG_INFO
#define PrintRaw(...) printf(__VA_ARGS__)
#define LOG_F(stream_id, ...)
#elif defined(__riscv)
#define LOG_ERROR DEV_LOG_ERROR
#define LOG_DEBUG DEV_LOG_DEBUG
#define LOG_INFO DEV_LOG_INFO
#define PrintRaw(...) flexio_dev_print(__VA_ARGS__)
#define LOG_F(stream_id, ...) flexio_dev_msg(stream_id, FLEXIO_MSG_DEV_INFO, __VA_ARGS__)
#endif

#define DEV_LOG_ERROR(...)                                                        \
    flexio_dev_print("[ERROR][%s:%d][%s]", __FILENAME__, __LINE__, __FUNCTION__); \
    flexio_dev_print(__VA_ARGS__)
#define DEV_LOG_DEBUG(...)                                                        \
    flexio_dev_print("[DEBGU][%s:%d][%s]", __FILENAME__, __LINE__, __FUNCTION__); \
    flexio_dev_print(__VA_ARGS__)
#define DEV_LOG_INFO(...)                                                        \
    flexio_dev_print("[INFO][%s:%d][%s]", __FILENAME__, __LINE__, __FUNCTION__); \
    flexio_dev_print(__VA_ARGS__)
#define HOST_LOG_ERROR(...)                                             \
    printf("[ERROR][%s:%d][%s]", __FILENAME__, __LINE__, __FUNCTION__); \
    printf(__VA_ARGS__)
#define HOST_LOG_DEBUG(...)                                             \
    printf("[DEBGU][%s:%d][%s]", __FILENAME__, __LINE__, __FUNCTION__); \
    printf(__VA_ARGS__)
#define HOST_LOG_INFO(...)                                             \
    printf("[INFO][%s:%d][%s]", __FILENAME__, __LINE__, __FUNCTION__); \
    printf(__VA_ARGS__)

#if CUR_LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_E LOG_ERROR
#else
#define LOG_ERROR(...)
#endif

#if CUR_LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_D LOG_DEBUG
#else
#define LOG_D(...)
#endif

#if CUR_LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_I LOG_INFO
#else
#define LOG_I(...)
#endif

#define NAME_OF(x) #x

#if defined(__x86_64__) || defined(__aarch64__)
#define Show_d(x) (printf("[%s]: %d\n", NAME_OF(x), x))
#define Show_ld(x) (printf("[%s]: %ld\n", NAME_OF(x), x))
#define Show_x(x) (printf("[%s]: %#x\n", NAME_OF(x), x))
#elif defined(__riscv)
#define Show_d(x) (flexio_dev_print("[%s]: %d\n", NAME_OF(x), x))
#define Show_ld(x) (flexio_dev_print("[%s]: %ld\n", NAME_OF(x), x))
#define Show_x(x) (flexio_dev_print("[%s]: %#x\n", NAME_OF(x), x))
#endif

#define LOG2VALUE(l) (1UL << (l)) /* 2^l */

struct app_transfer_cq {
    uint32_t cq_num;
    uint32_t log_cq_depth;
    flexio_uintptr_t cq_ring_daddr;
    flexio_uintptr_t cq_dbr_daddr;
} __attribute__((__packed__, aligned(8)));

struct app_transfer_wq {
    uint32_t wq_num;
    uint32_t wqd_mkey_id;
    flexio_uintptr_t wq_ring_daddr;
    flexio_uintptr_t wq_dbr_daddr;
    flexio_uintptr_t wqd_daddr;
    // 0 means addr on dpa address, 1 means addr on x86/arm address
    uint8_t daddr_on_host;
} __attribute__((__packed__, aligned(8)));

/* Transport data from HOST application to DEV application */
struct queue_config_data {
    struct app_transfer_cq rq_cq_data; /* device RQ's CQ */
    struct app_transfer_wq rq_data;    /* device RQ */
    struct app_transfer_cq sq_cq_data; /* device SQ's CQ */
    struct app_transfer_wq sq_data;    /* device SQ */
    uint32_t thread_index;
    flexio_uintptr_t new_buffer_addr;
    uint32_t new_buffer_mkey_id;
} __attribute__((__packed__, aligned(8)));

struct ether_addr {
    uint8_t addr_bytes[6];
};

struct ether_hdr {
    struct ether_addr dst_addr;
    struct ether_addr src_addr;
    uint16_t ether_type;
} __attribute__((__packed__));

struct ipv4_hdr {
    uint8_t version_ihl;
    uint8_t type_of_service;
    uint16_t total_length;
    uint16_t packet_id;
    uint16_t fragment_offset;
    uint8_t time_to_live;
    uint8_t next_proto_id;
    uint16_t hdr_checksum;
    uint32_t src_addr;
    uint32_t dst_addr;
} __attribute__((__packed__));

struct udp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t dgram_len;
    uint16_t dgram_cksum;
} __attribute__((__packed__));


struct udp_packet {
    struct ether_hdr eth_hdr;
    struct ipv4_hdr ip_hdr;
    struct udp_hdr udp_hdr;
    // no data element entry
} __attribute__((__packed__));

struct time_stamp {
    uint64_t sec;
    uint64_t n_sec;
};
struct ntp_header {
    struct time_stamp timestamp1;
    struct time_stamp timestamp2;
    struct time_stamp timestamp3;
    struct time_stamp timestamp4;
    uint64_t send_index;
};

// uint16_t ip_checksum(struct ipv4_hdr *iph)
// {
//     iph->hdr_checksum = 0;
//     uint32_t sum = 0;
//     uint16_t *ptr = (uint16_t *)iph;

//     for (unsigned long i = 0; i < sizeof(struct ipv4_hdr) / 2; i++)
//     {
//         sum += *ptr++;
//     }

//     while (sum >> 16)
//     {
//         sum = (sum & 0xffff) + (sum >> 16);
//     }

//     return ~sum;
// }

// uint16_t udp_checksum(struct ipv4_hdr *iph, struct udp_hdr *udph, void *data)
// {
//     uint32_t sum = 0;

//     // Add the pseudo-header (see RFC 768)
//     sum += (iph->src_addr >> 16) + (iph->src_addr & 0xffff);
//     sum += (iph->dst_addr >> 16) + (iph->dst_addr & 0xffff);
//     sum += cpu_to_be16(iph->next_proto_id); // UDP protocol number
//     sum += cpu_to_be16(udph->dgram_len);

//     // Add the UDP header
//     sum += udph->src_port + udph->dst_port + udph->dgram_len;

//     // Add the UDP data
//     uint16_t *ptr = (uint16_t *)data;
//     int len = be16_to_cpu(udph->dgram_len) - sizeof(struct udp_hdr);
//     while (len > 1)
//     {
//         sum += *ptr++;
//         len -= 2;
//     }
//     if (len > 0)
//     { // Add the padding if the packet length is odd
//         sum += *(uint8_t *)ptr;
//     }

//     // Fold the 32-bit sum to 16 bits
//     while (sum >> 16)
//     {
//         sum = (sum & 0xffff) + (sum >> 16);
//     }

//     return ~sum;
// }

#if defined(__x86_64__) || defined(__aarch64__)

static inline size_t
addr_to_num(struct ether_addr addr) {
    return static_cast<size_t>(addr.addr_bytes[0]) << 40 | static_cast<size_t>(addr.addr_bytes[1]) << 32 |
        static_cast<size_t>(addr.addr_bytes[2]) << 24 |
        static_cast<size_t>(addr.addr_bytes[3]) << 16 | static_cast<size_t>(addr.addr_bytes[4]) << 8 |
        static_cast<size_t>(addr.addr_bytes[5]);
}

#elif defined(__riscv)
static inline size_t
addr_to_num(struct ether_addr addr) {
    return (size_t)addr.addr_bytes[0] << 40 | (size_t)addr.addr_bytes[1] << 32 | (size_t)addr.addr_bytes[2] << 24 |
        (size_t)addr.addr_bytes[3] << 16 | (size_t)addr.addr_bytes[4] << 8 | (size_t)addr.addr_bytes[5];
}
#endif
