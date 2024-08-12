/*
 * Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
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

#include <stddef.h>
#include "wrapper_flexio_device.h"
#include "../common/dpa_send_common.h"

flexio_dev_rpc_handler_t dpa_send_mt_device_init;            /* Device initialization function */
flexio_dev_rpc_handler_t dpa_send_mt_deivce_first_packet;    /* First packet function */
flexio_dev_rpc_handler_t dpa_send_mt_stop;
flexio_dev_event_handler_t dpa_send_mt_device_event_handler; /* Event handler function */

#define hdr_size 42
#define data_size 22
#define pkt_size (hdr_size + data_size)

#define MAX_THREADS 190
#define THREAD_CREDITS 8

// TO_DPA_DST = 0 means to a dpdk server, which will wait dpdk send first packet and set dst_addr to server bond_0 mac addr
// TO_DPA_DST = 1 means to a dpa sever, which will initiative send first packet and set dst_addr to 0x010101010101 - ...
#define TO_DPA_DST 0

// ONLY_SEND = 1 means only send packet, never receive packet
// ONLY_SEND = 0 means send and receive any packet
#define ONLY_SEND 1

// DISPLAY_COUNT = 1 means print the counter for received packet
// DISPLAY_COUNT = 0 means doesn't print the counter for received packet
#define DISPLAY_COUNT 0

/* Device context */
static struct device_context {
    uint32_t lkey;            /* Local memory key */
    uint32_t is_initalized;   /* Initialization flag */
    struct cq_ctx_t rqcq_ctx; /* RQ CQ context */
    struct cq_ctx_t sqcq_ctx; /* SQ CQ context */
    struct rq_ctx_t rq_ctx;   /* RQ context */
    struct sq_ctx_t sq_ctx;   /* SQ context */
    struct dt_ctx_t dt_ctx;   /* DT context */
    struct host_rq_ctx_t host_rq_ctx;
    struct host_sq_ctx_t host_sq_ctx;
    uint8_t credits;   /* Number of packet credits */
    uint8_t thread_index;
    uint8_t rq_on_host;
    uint8_t sq_on_host;
    uint32_t packets_count;
    uint64_t send_index;
} __attribute__((__aligned__(64))) dev_ctxs[MAX_THREADS];
// stop
uint8_t is_stop = 0;
/*
 * Initialize the CQ context
 *
 * @app_cq [in]: CQ HW context
 * @ctx [out]: CQ context
 */
static void
init_cq(const struct app_transfer_cq app_cq, struct cq_ctx_t *ctx) {
    ctx->cq_number = app_cq.cq_num;
    ctx->cq_ring = (struct flexio_dev_cqe64 *)app_cq.cq_ring_daddr;
    ctx->cq_dbr = (uint32_t *)app_cq.cq_dbr_daddr;

    ctx->cqe = ctx->cq_ring; /* Points to the first CQE */
    ctx->cq_idx = 0;
    ctx->cq_hw_owner_bit = 0x1;
    ctx->cq_idx_mask = ((1 << LOG_CQ_RING_DEPTH) - 1);
}

/*
 * Initialize the RQ context
 *
 * @app_rq [in]: RQ HW context
 * @ctx [out]: RQ context
 */
static void
init_rq(const struct app_transfer_wq app_rq, struct rq_ctx_t *ctx) {
    ctx->rq_number = app_rq.wq_num;
    ctx->rq_ring = (struct flexio_dev_wqe_rcv_data_seg *)app_rq.wq_ring_daddr;
    ctx->rq_dbr = (uint32_t *)app_rq.wq_dbr_daddr;
    ctx->rq_idx_mask = ((1 << LOG_RQ_RING_DEPTH) - 1);
}

/*
 * Initialize the SQ context
 *
 * @app_sq [in]: SQ HW context
 * @ctx [out]: SQ context
 */
static void
init_sq(const struct app_transfer_wq app_sq, struct sq_ctx_t *ctx) {
    ctx->sq_number = app_sq.wq_num;
    ctx->sq_ring = (union flexio_dev_sqe_seg *)app_sq.wq_ring_daddr;
    ctx->sq_dbr = (uint32_t *)app_sq.wq_dbr_daddr;

    ctx->sq_wqe_seg_idx = 0;
    ctx->sq_dbr++;
    ctx->sq_idx_mask = ((1 << (LOG_SQ_RING_DEPTH + LOG_SQE_NUM_SEGS)) - 1);
}

/*
 * This is the main function of the dpa net device, called on each packet from dpa_send_device_event_handler()
 * Packet are received from the RQ, processed by changing MAC addresses and transmitted to the SQ.
 *
 * @dtctx [in]: This thread context
 */

static void prepare_packet(void *sq_data, size_t thread_index, struct flexio_dev_thread_ctx *dtctx) {
    (void)dtctx;
    struct ether_hdr *eth_hdr;
    struct ipv4_hdr *ip_hdr;
    struct udp_hdr *udp_hdr;

    eth_hdr = (struct ether_hdr *)sq_data;
    eth_hdr->src_addr = SRC_ADDR;
    eth_hdr->src_addr.addr_bytes[5] += thread_index;
#if TO_DPA_DST
    eth_hdr->dst_addr = DST_DPA_ADDR;
    eth_hdr->dst_addr.addr_bytes[5] += thread_index;
#else
    eth_hdr->dst_addr = DST_ADDR;
#endif
    eth_hdr->ether_type = cpu_to_be16(0x0800);

    ip_hdr = (struct ipv4_hdr *)(eth_hdr + 1);
    ip_hdr->version_ihl = 0x45;
    ip_hdr->type_of_service = 0;
    ip_hdr->total_length = cpu_to_be16(sizeof(uint64_t) * 2 + sizeof(struct udp_hdr) + sizeof(struct ipv4_hdr));
    ip_hdr->packet_id = cpu_to_be16(thread_index);
    ip_hdr->fragment_offset = cpu_to_be16(0);
    ip_hdr->time_to_live = 64;
    ip_hdr->next_proto_id = 17;
    ip_hdr->src_addr = cpu_to_be16(thread_index);
    ip_hdr->dst_addr = cpu_to_be16(thread_index);

    udp_hdr = (struct udp_hdr *)(ip_hdr + 1);
    udp_hdr->dgram_len = cpu_to_be16(sizeof(uint64_t) * 2 + sizeof(struct udp_hdr));
    udp_hdr->src_port = cpu_to_be16(thread_index);
    udp_hdr->dst_port = cpu_to_be16(thread_index);

    // why I write this?
    // memcpy(sq_data, eth_hdr, hdr_size);
}

static void prepare_packet_host(void *sq_data, size_t thread_index, struct flexio_dev_thread_ctx *dtctx) {
    struct udp_packet udp_pkt;
    udp_pkt.eth_hdr.src_addr = SRC_ADDR;
    udp_pkt.eth_hdr.src_addr.addr_bytes[5] += thread_index;
#if TO_DPA_DST
    udp_pkt.eth_hdr.dst_addr = DST_DPA_ADDR;
    udp_pkt.eth_hdr.dst_addr.addr_bytes[5] += thread_index;
#else
    udp_pkt.eth_hdr.dst_addr = DST_ADDR;
#endif
    udp_pkt.eth_hdr.ether_type = cpu_to_be16(0x0800);

    udp_pkt.ip_hdr.version_ihl = 0x45;
    udp_pkt.ip_hdr.type_of_service = 0;
    udp_pkt.ip_hdr.total_length = cpu_to_be16(sizeof(uint64_t) * 2 + sizeof(struct udp_hdr) + sizeof(struct ipv4_hdr));
    udp_pkt.ip_hdr.packet_id = cpu_to_be16(thread_index);
    udp_pkt.ip_hdr.fragment_offset = cpu_to_be16(0);
    udp_pkt.ip_hdr.time_to_live = 64;
    udp_pkt.ip_hdr.next_proto_id = 17;
    udp_pkt.ip_hdr.src_addr = cpu_to_be16(thread_index);
    udp_pkt.ip_hdr.dst_addr = cpu_to_be16(thread_index);

    udp_pkt.udp_hdr.dgram_len = cpu_to_be16(sizeof(uint64_t) * 2 + sizeof(struct udp_hdr));
    udp_pkt.udp_hdr.src_port = cpu_to_be16(thread_index);
    udp_pkt.udp_hdr.dst_port = cpu_to_be16(thread_index);

    flexio_dev_window_copy_to_host(dtctx, (uint64_t)sq_data, &udp_pkt, sizeof(udp_pkt));
}


static void __unused
receive_packet_on_dpa(struct device_context *dev_ctx) {
    uint32_t data_sz;
    char *rq_data;

    // uint64_t cycle = __dpa_thread_cycles();

    /* Extract relevant data from CQE */
    rq_data = receive_packet(&dev_ctx->rqcq_ctx, &dev_ctx->rq_ctx, &data_sz);
    (void)rq_data;
    // cycle -= *(uint64_t *)(rq_data + hdr_size);
    // LOG_I("r: %u %ld %ld\n", data_sz, __dpa_thread_cycles(), *(uint64_t *)(rq_data + sizeof(struct ether_hdr)));
    if (data_sz != pkt_size) {
        return;
    }
    // if (*(uint64_t *)(rq_data + hdr_size + sizeof(uint64_t)) != dev_ctx->send_index) {
    //     LOG_I("mismatch: want %lu, get %lu\n", dev_ctx->send_index, *(uint64_t *)(rq_data + hdr_size + sizeof(uint64_t)));
    //     return;
    // }
    dev_ctx->credits++;
}

static void send_packet_dpa(struct flexio_dev_thread_ctx *dtctx, struct device_context *dev_ctx) {
    char *sq_data = get_next_send_buf(&dev_ctx->dt_ctx, LOG_WQ_DATA_ENTRY_BSIZE);
    struct ether_hdr *eth_hdr = (struct ether_hdr *)(sq_data);
    eth_hdr->src_addr = SRC_ADDR;
    eth_hdr->src_addr.addr_bytes[5] += dev_ctx->thread_index;
#if TO_DPA_DST
    eth_hdr->dst_addr = DST_DPA_ADDR;
    eth_hdr->dst_addr.addr_bytes[5] += dev_ctx->thread_index;
#else
    eth_hdr->dst_addr = DST_ADDR;
#endif

    prepare_send_packet(&dev_ctx->sq_ctx, sq_data, pkt_size);

    // uint64_t *data_ptr = (uint64_t *)(sq_data + hdr_size);
    // *(data_ptr + 1) = ++dev_ctx->send_index;
    // *data_ptr = __dpa_thread_cycles();
    // LOG_I("s: %ld\n", *data_ptr);
    dev_ctx->credits--;
    finish_send_packet(dtctx, &dev_ctx->sq_ctx);
}

static void __unused
receive_packet_on_host(struct device_context *dev_ctx) {
    uint32_t data_sz;
    char *rq_data;

    // uint64_t cycle = __dpa_thread_cycles();

    /* Extract relevant data from CQE */
    rq_data = receive_packet(&dev_ctx->rqcq_ctx, &dev_ctx->rq_ctx, &data_sz);
    if (data_sz != pkt_size) {
        return;
    }
    (void)rq_data;
    // cycle -= *(uint64_t *)(rq_data_dpa + hdr_size);
    // LOG_I("r: %u %ld %ld\n", data_sz, __dpa_thread_cycles(), *(uint64_t *)(rq_data_dpa + sizeof(struct ether_hdr)));
    // TODO need this ?

    // if (*(uint64_t *)(rq_data_dpa + hdr_size + sizeof(uint64_t)) != dev_ctx->send_index) {
    //     LOG_I("mismatch: want %lu, get %lu\n", dev_ctx->send_index, *(uint64_t *)(rq_data_dpa + hdr_size + sizeof(uint64_t)));
    //     return;
    // }
    dev_ctx->credits++;
}

static void send_packet_on_host(struct flexio_dev_thread_ctx *dtctx, struct device_context *dev_ctx) {
    char *sq_data = get_next_send_buf(&dev_ctx->dt_ctx, LOG_WQ_DATA_ENTRY_BSIZE);
    char *sq_data_dpa = host_sq_addr_to_dpa_addr(sq_data, &dev_ctx->host_sq_ctx);

    struct ether_hdr *eth_hdr = (struct ether_hdr *)(sq_data_dpa);
    eth_hdr->src_addr = SRC_ADDR;
    eth_hdr->src_addr.addr_bytes[5] += dev_ctx->thread_index;
#if TO_DPA_DST
    eth_hdr->dst_addr = DST_DPA_ADDR;
    eth_hdr->dst_addr.addr_bytes[5] += dev_ctx->thread_index;
#else
    eth_hdr->dst_addr = DST_ADDR;
#endif

    prepare_send_packet(&dev_ctx->sq_ctx, sq_data, pkt_size);

    // uint64_t *data_ptr = (uint64_t *)(sq_data_dpa + hdr_size);
    // *(data_ptr + 1) = ++dev_ctx->send_index;
    // *data_ptr = __dpa_thread_cycles();
    dev_ctx->credits--;
    finish_send_packet_host(dtctx, &dev_ctx->sq_ctx);
}


/*
 * Called by host to initialize the device context
 *
 * @data [in]: pointer to the device context from the host
 * @return: This function always returns 0
 */
__dpa_rpc__ uint64_t
dpa_send_mt_device_init(uint64_t data) {
    struct queue_config_data *shared_data = (struct queue_config_data *)data;
    Assert(shared_data->thread_index < MAX_THREADS);

    struct device_context *dev_ctx = &dev_ctxs[shared_data->thread_index];

    dev_ctx->lkey = shared_data->sq_data.wqd_mkey_id;
    dev_ctx->host_rq_ctx.rkey = shared_data->rq_data.wqd_mkey_id;
    // this is trick used field
    dev_ctx->host_rq_ctx.rq_window_id = shared_data->new_buffer_mkey_id;
    dev_ctx->host_rq_ctx.host_rx_buff = (void *)shared_data->rq_data.wqd_daddr;

    dev_ctx->host_sq_ctx.rkey = shared_data->sq_data.wqd_mkey_id;
    // this is trick used field
    dev_ctx->host_sq_ctx.sq_window_id = shared_data->new_buffer_mkey_id;
    dev_ctx->host_sq_ctx.host_tx_buff = (void *)shared_data->sq_data.wqd_daddr;


    init_cq(shared_data->rq_cq_data, &dev_ctx->rqcq_ctx);
    init_rq(shared_data->rq_data, &dev_ctx->rq_ctx);
    init_cq(shared_data->sq_cq_data, &dev_ctx->sqcq_ctx);
    init_sq(shared_data->sq_data, &dev_ctx->sq_ctx);

    init_send_sq(&dev_ctx->sq_ctx, LOG2VALUE(LOG_SQ_RING_DEPTH), dev_ctx->lkey);
    dev_ctx->dt_ctx.sq_tx_buff = (void *)shared_data->sq_data.wqd_daddr;
    dev_ctx->dt_ctx.tx_buff_idx = 0;
    dev_ctx->dt_ctx.data_idx_mask = ((1 << (LOG_SQ_RING_DEPTH)) - 1);

    dev_ctx->rq_on_host = shared_data->rq_data.daddr_on_host;
    dev_ctx->sq_on_host = shared_data->sq_data.daddr_on_host;

    dev_ctx->credits = THREAD_CREDITS;
    dev_ctx->thread_index = (uint8_t)shared_data->thread_index;
    dev_ctx->send_index = 1;

    dev_ctx->is_initalized = 1;
    LOG_I("Thread %u init success\n", shared_data->thread_index);

    __dpa_thread_memory_writeback();
    return 0;
}

__dpa_rpc__ uint64_t dpa_send_mt_deivce_first_packet(uint64_t __unused dummy) {
    struct flexio_dev_thread_ctx *dtctx;
    flexio_dev_get_thread_ctx(&dtctx);
    for (size_t i = 0;i < MAX_THREADS;i++) {
        struct device_context *dev_ctx = &dev_ctxs[i];
        char *sq_data;
        if (!dev_ctx->is_initalized) {
            continue;
        }
        if (dev_ctx->sq_on_host) {
            dev_ctx->host_sq_ctx.dpa_tx_buff = get_host_buffer_with_dtctx(dtctx, dev_ctx->host_sq_ctx.sq_window_id, dev_ctx->host_sq_ctx.rkey, dev_ctx->host_sq_ctx.host_tx_buff);

            for (size_t entry = 0;entry < LOG2VALUE(LOG_SQ_RING_DEPTH);entry++) {
                sq_data = get_next_send_buf(&dev_ctx->dt_ctx, LOG_WQ_DATA_ENTRY_BSIZE);
                prepare_packet_host(sq_data, i, dtctx);
            }
#if TO_DPA_DST
            send_packet_on_host(dtctx, dev_ctx);
#endif
        } else {
            for (size_t entry = 0;entry < LOG2VALUE(LOG_SQ_RING_DEPTH);entry++) {
                sq_data = get_next_send_buf(&dev_ctx->dt_ctx, LOG_WQ_DATA_ENTRY_BSIZE);
                prepare_packet(sq_data, i, dtctx);
            }
#if TO_DPA_DST
            send_packet_dpa(dtctx, dev_ctx);
#endif
        }
    }
    return 0;
}

__dpa_rpc__ uint64_t dpa_send_mt_stop(uint64_t __unused dummy) {
    is_stop = 1;
    return 0;
}
/*
 * This function is called when a new packet is received to RQ's CQ.
 * Upon receiving a packet, the function will iterate over all received packets and process them.
 * Once all packets in the CQ are processed, the CQ will be rearmed to receive new packets events.
 */
void
__dpa_global__
dpa_send_mt_device_event_handler(uint64_t index) {
    struct flexio_dev_thread_ctx *dtctx;
    struct device_context *dev_ctx = &dev_ctxs[index];
    flexio_dev_get_thread_ctx(&dtctx);
    if (dev_ctx->is_initalized == 0) {
        flexio_dev_thread_reschedule();
    }

    flexio_dev_print("thread %ld begin\n", index);

    uint8_t rq_on_host = dev_ctx->rq_on_host;
    uint8_t sq_on_host = dev_ctx->sq_on_host;

    // this is useless but a simple way to init dpa_rx_buff
    if (rq_on_host) {
        dev_ctx->host_rq_ctx.dpa_rx_buff = get_host_buffer_with_dtctx(dtctx, dev_ctx->host_rq_ctx.rq_window_id, dev_ctx->host_rq_ctx.rkey, dev_ctx->host_rq_ctx.host_rx_buff);
    } else {
        // in this situation, we set dpa_buff to host_buff, to prevent misuse of receive_packet_host
        dev_ctx->host_rq_ctx.dpa_rx_buff = (flexio_uintptr_t)dev_ctx->host_rq_ctx.host_rx_buff;
    }

    if (sq_on_host) {
        dev_ctx->host_sq_ctx.dpa_tx_buff = get_host_buffer_with_dtctx(dtctx, dev_ctx->host_sq_ctx.sq_window_id, dev_ctx->host_sq_ctx.rkey, dev_ctx->host_sq_ctx.host_tx_buff);
    } else {
        dev_ctx->host_sq_ctx.dpa_tx_buff = (flexio_uintptr_t)dev_ctx->host_sq_ctx.host_tx_buff;
    }
#if DISPLAY_COUNT && !ONLY_SEND
    register size_t pkt_count = 0;
#endif
    if (sq_on_host) {
        while (!is_stop) {
#if ONLY_SEND
            send_packet_on_host(dtctx, dev_ctx);
#else
            // if (dev_ctx->credits) {
            //     send_packet_on_host(dtctx, dev_ctx);
            // }
            send_packet_on_host(dtctx, dev_ctx);
            while (flexio_dev_cqe_get_owner(dev_ctx->rqcq_ctx.cqe) != dev_ctx->rqcq_ctx.cq_hw_owner_bit) {
                receive_packet_on_host(dev_ctx);
                step_rq(&dev_ctx->rq_ctx);
                step_cq(&dev_ctx->rqcq_ctx);
#if DISPLAY_COUNT
                pkt_count++;
                if (__builtin_expect((pkt_count == 1000000), 0)) {
                    dev_ctx->packets_count += 1000000;
                    pkt_count = 0;
                    if (index == 0) {
                        size_t sum = dev_ctxs[index].packets_count;
                        // size_t sum = 0;
                        // for (size_t i = 0; i < MAX_THREADS - 1; i += 16) {
                        //     flexio_dev_print("%3ld - %3ld:", i, i + 15);

                        //     for (size_t j = i; j <= i + 15 && j < MAX_THREADS - 1; j++) {
                        //         sum += dev_ctxs[j].packets_count;
                        //         flexio_dev_print(" %u", dev_ctxs[j].packets_count);
                        //     }
                        //     flexio_dev_print("\n");
                        // }
                        flexio_dev_print("sum : %ld\n", sum);
                    }
                }
#endif
            }
#endif
        }
    } else {
        while (!is_stop) {
#if ONLY_SEND
            send_packet_dpa(dtctx, dev_ctx);
#else
            // if (dev_ctx->credits) {
            //     send_packet_dpa(dtctx, dev_ctx);
            // }
            send_packet_dpa(dtctx, dev_ctx);
            while (flexio_dev_cqe_get_owner(dev_ctx->rqcq_ctx.cqe) != dev_ctx->rqcq_ctx.cq_hw_owner_bit) {
                receive_packet_on_dpa(dev_ctx);
                step_rq(&dev_ctx->rq_ctx);
                step_cq(&dev_ctx->rqcq_ctx);
#if DISPLAY_COUNT
                pkt_count++;
                if (__builtin_expect((pkt_count == 1000000), 0)) {
                    dev_ctx->packets_count += 1000000;
                    pkt_count = 0;
                    if (index == 0) {
                        size_t sum = dev_ctxs[index].packets_count;
                        // size_t sum = 0;
                        // for (size_t i = 0; i < MAX_THREADS - 1; i += 16) {
                        //     flexio_dev_print("%3ld - %3ld:", i, i + 15);

                        //     for (size_t j = i; j <= i + 15 && j < MAX_THREADS - 1; j++) {
                        //         sum += dev_ctxs[j].packets_count;
                        //         flexio_dev_print(" %u", dev_ctxs[j].packets_count);
                        //     }
                        //     flexio_dev_print("\n");
                        // }
                        flexio_dev_print("sum : %ld\n", sum);
                    }
                }
#endif
            }
#endif
        }
    }
    flexio_dev_print("thread %ld end\n", index);

    __dpa_thread_fence(__DPA_MEMORY, __DPA_W, __DPA_W);
    flexio_dev_cq_arm(dtctx, dev_ctx->rqcq_ctx.cq_idx, dev_ctx->rqcq_ctx.cq_number);
    flexio_dev_thread_reschedule();
}
