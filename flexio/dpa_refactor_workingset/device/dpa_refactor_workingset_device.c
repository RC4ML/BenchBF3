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
#include "../common/dpa_refactor_workingset_common.h"

flexio_dev_rpc_handler_t dpa_refactor_workingset_device_init;            /* Device initialization function */
flexio_dev_event_handler_t dpa_refactor_workingset_device_event_handler; /* Event handler function */

#define TARGET_PKT_SIZE 1024

uint64_t total_dummy = 0;

// if TEST_BANDWIDTH = 0, means we will return every packet just swap mac addr
// if TEST_BANDWIDTH = 1, means we will return one packet every BATCH_SIZE packet, and with credits
#define TEST_BANDWIDTH 0
// like ARM/HOST
#define BATCH_SIZE 16

/* Device context */
static struct {
    uint32_t lkey;            /* Local memory key, used for send queue */
    uint32_t is_initalized;   /* Initialization flag */
    struct cq_ctx_t rqcq_ctx; /* RQ CQ context */
    struct cq_ctx_t sqcq_ctx; /* SQ CQ context */
    struct rq_ctx_t rq_ctx;   /* RQ context */
    struct sq_ctx_t sq_ctx;   /* SQ context */
    struct dt_ctx_t dt_ctx;   /* DT context */
    struct host_rq_ctx_t host_rq_ctx;
    uint32_t packets_count;   /* Number of processed packets */
    uint8_t rq_on_host;
    uint8_t sq_on_host;
} dev_ctx = { 0 };

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
 * This is the main function of the dpa net device, called on each packet from dpa_refactor_device_event_handler()
 * Packet are received from the RQ, processed by changing MAC addresses and transmitted to the SQ.
 *
 * @dtctx [in]: This thread context
 */
static void
process_packet(struct flexio_dev_thread_ctx *dtctx) {
    uint32_t data_sz;
    char *rq_data;
    // char tmp;
    /* MAC address has 6 bytes: ff:ff:ff:ff:ff:ff */
    // const int nb_mac_address_bytes = 6;

    /* Extract relevant data from CQE */
    rq_data = receive_packet(&dev_ctx.rqcq_ctx, &dev_ctx.rq_ctx, &data_sz);
    if (data_sz == TARGET_PKT_SIZE) {
        uint64_t dummy = 0;
        size_t *tmp_ptr = (size_t *)rq_data;
        for (;;tmp_ptr++) {
            size_t tmp = *tmp_ptr;
            if (tmp == MAGIC_NUMBER) {
                break;
            }
            dummy += tmp;
        }
        total_dummy += dummy;

    }
    char *sq_data;
    sq_data = rq_data;

#if TEST_BANDWIDTH
    if (dev_ctx.packets_count % BATCH_SIZE != BATCH_SIZE - 1) {
        return;
    }
    size_t src_mac = *(size_t *)sq_data;
    size_t dst_mac = *(size_t *)(sq_data + 6);

    *(size_t *)(sq_data) = dst_mac;
    *(size_t *)(sq_data + 6) = (src_mac & 0x0000FFFFFFFFFFFF) | (0x0008ll << 48);
    struct udp_packet *udp_packet = (struct udp_packet *)(sq_data);
    udp_packet->ip_hdr.time_to_live = 60;
    *(size_t *)(udp_packet + 1) = BATCH_SIZE;

    prepare_send_packet(&dev_ctx.sq_ctx, sq_data, data_sz);
    finish_send_packet(dtctx, &dev_ctx.sq_ctx);
#else
    size_t src_mac = *(size_t *)sq_data;
    size_t dst_mac = *(size_t *)(sq_data + 6);

    *(size_t *)(sq_data) = dst_mac;
    *(size_t *)(sq_data + 6) = (src_mac & 0x0000FFFFFFFFFFFF) | (0x0008ll << 48);

    prepare_send_packet(&dev_ctx.sq_ctx, sq_data, data_sz);
    finish_send_packet(dtctx, &dev_ctx.sq_ctx);

#endif
}


static void
process_packet_host(struct flexio_dev_thread_ctx *dtctx) {
    uint32_t data_sz;
    char *rq_data;
    char *rq_data_dpa;
    // char tmp;
    /* MAC address has 6 bytes: ff:ff:ff:ff:ff:ff */
    // const int nb_mac_address_bytes = 6;

    /* Extract relevant data from CQE */
    rq_data = receive_packet(&dev_ctx.rqcq_ctx, &dev_ctx.rq_ctx, &data_sz);
    rq_data_dpa = host_rq_addr_to_dpa_addr(rq_data, &dev_ctx.host_rq_ctx);
    if (data_sz == TARGET_PKT_SIZE) {
        uint64_t dummy = 0;
        size_t *tmp_ptr = (size_t *)rq_data_dpa;
        for (;;tmp_ptr++) {
            size_t tmp = *tmp_ptr;
            if (tmp == MAGIC_NUMBER) {
                break;
            }
            dummy += tmp;
        }
        total_dummy += dummy;
    }
    char *sq_data;
    char *sq_data_dpa;
    sq_data = rq_data;
    sq_data_dpa = rq_data_dpa;

#if TEST_BANDWIDTH
    if (dev_ctx.packets_count % BATCH_SIZE != BATCH_SIZE - 1) {
        return;
    }
    size_t src_mac = *(size_t *)sq_data_dpa;
    size_t dst_mac = *(size_t *)(sq_data_dpa + 6);

    *(size_t *)(sq_data_dpa) = dst_mac;
    *(size_t *)(sq_data_dpa + 6) = (src_mac & 0x0000FFFFFFFFFFFF) | (0x0008ll << 48);
    struct udp_packet *udp_packet = (struct udp_packet *)(sq_data_dpa);
    udp_packet->ip_hdr.time_to_live = 60;
    *(size_t *)(udp_packet + 1) = BATCH_SIZE;

    prepare_send_packet(&dev_ctx.sq_ctx, sq_data, data_sz);
    finish_send_packet_host(dtctx, &dev_ctx.sq_ctx);

#else
    size_t src_mac = *(size_t *)sq_data_dpa;
    size_t dst_mac = *(size_t *)(sq_data_dpa + 6);

    *(size_t *)(sq_data_dpa) = dst_mac;
    *(size_t *)(sq_data_dpa + 6) = (src_mac & 0x0000FFFFFFFFFFFF) | (0x0008ll << 48);


    prepare_send_packet(&dev_ctx.sq_ctx, sq_data, data_sz);
    finish_send_packet_host(dtctx, &dev_ctx.sq_ctx);
#endif
}

/*
 * Called by host to initialize the device context
 *
 * @data [in]: pointer to the device context from the host
 * @return: This function always returns 0
 */
__dpa_rpc__ uint64_t
dpa_refactor_workingset_device_init(uint64_t data) {
    struct queue_config_data *shared_data = (struct queue_config_data *)data;

    dev_ctx.lkey = shared_data->sq_data.wqd_mkey_id;
    dev_ctx.host_rq_ctx.rkey = shared_data->rq_data.wqd_mkey_id;
    // this is trick used field
    dev_ctx.host_rq_ctx.rq_window_id = shared_data->new_buffer_mkey_id;
    dev_ctx.host_rq_ctx.host_rx_buff = (void *)shared_data->rq_data.wqd_daddr;

    init_cq(shared_data->rq_cq_data, &dev_ctx.rqcq_ctx);
    init_rq(shared_data->rq_data, &dev_ctx.rq_ctx);
    init_cq(shared_data->sq_cq_data, &dev_ctx.sqcq_ctx);
    init_sq(shared_data->sq_data, &dev_ctx.sq_ctx);

    init_send_sq(&dev_ctx.sq_ctx, LOG2VALUE(LOG_SQ_RING_DEPTH), dev_ctx.lkey);
    dev_ctx.dt_ctx.sq_tx_buff = (void *)shared_data->sq_data.wqd_daddr;
    dev_ctx.dt_ctx.tx_buff_idx = 0;
    dev_ctx.dt_ctx.data_idx_mask = ((1 << (LOG_SQ_RING_DEPTH)) - 1);

    dev_ctx.rq_on_host = shared_data->rq_data.daddr_on_host;
    dev_ctx.sq_on_host = shared_data->sq_data.daddr_on_host;

    dev_ctx.is_initalized = 1;
    LOG_I("Thread %u init success\n", shared_data->thread_index);
    return 0;
}

/*
 * This function is called when a new packet is received to RQ's CQ.
 * Upon receiving a packet, the function will iterate over all received packets and process them.
 * Once all packets in the CQ are processed, the CQ will be rearmed to receive new packets events.
 */
void
__dpa_global__
dpa_refactor_workingset_device_event_handler(uint64_t __unused arg0) {
    struct flexio_dev_thread_ctx *dtctx;

    flexio_dev_get_thread_ctx(&dtctx);

    if (dev_ctx.is_initalized == 0)
        flexio_dev_thread_reschedule();

    flexio_dev_print("thread main begin\n");

    uint8_t rq_on_host = dev_ctx.rq_on_host;

    if (rq_on_host) {
        dev_ctx.host_rq_ctx.dpa_rx_buff = get_host_buffer_with_dtctx(dtctx, dev_ctx.host_rq_ctx.rq_window_id, dev_ctx.host_rq_ctx.rkey, dev_ctx.host_rq_ctx.host_rx_buff);
    } else {
        // in this situation, we set dpa_buff to host_buff, to prevent misuse of receive_packet_host
        dev_ctx.host_rq_ctx.dpa_rx_buff = (flexio_uintptr_t)dev_ctx.host_rq_ctx.host_rx_buff;
    }
    //    uint64_t total_cycle = 0;
    //    uint64_t cnt = 0;
    //    uint64_t begin_time, end_time;
    //    uint64_t target_cnt = 128;
    //    uint64_t times[target_cnt + 10];
    //    flexio_dev_print("begin\n");
    while (dtctx != NULL) {
        while (flexio_dev_cqe_get_owner(dev_ctx.rqcq_ctx.cqe) != dev_ctx.rqcq_ctx.cq_hw_owner_bit) {
            //        begin_time = __dpa_thread_cycles();
            if (rq_on_host) {
                process_packet_host(dtctx);
            } else {
                process_packet(dtctx);
            }
            dev_ctx.packets_count++;
            step_rq(&dev_ctx.rq_ctx);
            step_cq(&dev_ctx.rqcq_ctx);

            //        end_time = __dpa_thread_cycles();
            //        total_cycle += end_time - begin_time;
            //        if (cnt < target_cnt) {
            //            times[cnt] = end_time - begin_time;
            //        }
            //        cnt++;
        }
    }

    //    flexio_dev_print("%ld %ld\n", total_cycle, cnt);
    //    printf("begin print\n");
    //    for (uint64_t i = 0; i < target_cnt; i++) {
    //        printf("%ld\n", times[i]);
    //    }
    //    printf("end print\n");
    __dpa_thread_fence(__DPA_MEMORY, __DPA_W, __DPA_W);
    flexio_dev_cq_arm(dtctx, dev_ctx.rqcq_ctx.cq_idx, dev_ctx.rqcq_ctx.cq_number);
    flexio_dev_thread_reschedule();
}
