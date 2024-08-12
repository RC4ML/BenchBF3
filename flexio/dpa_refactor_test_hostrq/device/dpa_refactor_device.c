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
#include "../common/dpa_refactor_common.h"

flexio_dev_rpc_handler_t dpa_refactor_device_init;            /* Device initialization function */
flexio_dev_event_handler_t dpa_refactor_device_event_handler; /* Event handler function */

/* Device context */
static struct
{
    uint32_t lkey;            /* Local memory key */
    uint32_t is_initalized;   /* Initialization flag */
    struct cq_ctx_t rqcq_ctx; /* RQ CQ context */
    struct cq_ctx_t sqcq_ctx; /* SQ CQ context */
    struct rq_ctx_t rq_ctx;   /* RQ context */
    struct sq_ctx_t sq_ctx;   /* SQ context */
    struct dt_ctx_t dt_ctx;   /* DT context */
    uint32_t packets_count;   /* Number of processed packets */
} dev_ctx = {0};

flexio_uintptr_t host_rq_addr, dpa_rq_addr;
uint32_t host_rq_lkey, dpa_rq_lkey;
/*
 * Initialize the CQ context
 *
 * @app_cq [in]: CQ HW context
 * @ctx [out]: CQ context
 */
static void
init_cq(const struct app_transfer_cq app_cq, struct cq_ctx_t *ctx)
{
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
init_rq(const struct app_transfer_wq app_rq, struct rq_ctx_t *ctx)
{
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
init_sq(const struct app_transfer_wq app_sq, struct sq_ctx_t *ctx)
{
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

/*
 * Called by host to initialize the device context
 *
 * @data [in]: pointer to the device context from the host
 * @return: This function always returns 0
 */
static uint64_t *rq_buff;
__dpa_rpc__ uint64_t
dpa_refactor_device_init(uint64_t data)
{
    struct queue_config_data *shared_data = (struct queue_config_data *)data;

    dev_ctx.lkey = shared_data->sq_data.wqd_mkey_id;
    init_cq(shared_data->rq_cq_data, &dev_ctx.rqcq_ctx);
    init_rq(shared_data->rq_data, &dev_ctx.rq_ctx);
    init_cq(shared_data->sq_cq_data, &dev_ctx.sqcq_ctx);
    init_sq(shared_data->sq_data, &dev_ctx.sq_ctx);

    init_send_sq(&dev_ctx.sq_ctx, LOG2VALUE(LOG_SQ_RING_DEPTH), dev_ctx.lkey);
    dev_ctx.dt_ctx.sq_tx_buff = (void *)shared_data->sq_data.wqd_daddr;
    dev_ctx.dt_ctx.tx_buff_idx = 0;
    dev_ctx.dt_ctx.data_idx_mask = ((1 << (LOG_SQ_RING_DEPTH)) - 1);
    dev_ctx.is_initalized = 1;

    rq_buff = (void *)shared_data->rq_data.wqd_daddr;

    uint64_t *tmp_ptr = dev_ctx.dt_ctx.sq_tx_buff;
    uint64_t sum = 0;
    for (size_t i = 0; i < LOG2VALUE(13); i++)
    {
        for (size_t j = 0; j < LOG2VALUE(12); j += 8)
        {
            sum += *tmp_ptr;
            tmp_ptr++;
        }
    }
    flexio_dev_print("finish first %ld\n", sum);

    dpa_rq_addr = shared_data->new_buffer_addr;
    dpa_rq_lkey = shared_data->new_buffer_mkey_id;
    host_rq_addr = shared_data->rq_data.wqd_daddr;
    host_rq_lkey = shared_data->rq_data.wqd_mkey_id;
    // tmp_ptr = (uint64_t *)shared_data->rq_data.wqd_daddr;
    // for (size_t i = 0; i < 2048; i++)
    // {
    //     for (size_t j = 0; j < LOG2VALUE(LOG_WQ_DATA_ENTRY_BSIZE); j += 8)
    //     {
    //         sum += *tmp_ptr;
    //         tmp_ptr++;
    //     }
    // }
    // flexio_dev_print("finish load %ld\n", sum);

    return 0;
}

struct wqe_data_seg
{
    __be32 byte_count;
    __be32 lkey;
    __be64 addr;
};

void init_rq_ring(void)
{
    int num_of_wqes = LOG2VALUE(LOG_RQ_RING_DEPTH);
    size_t data_chunk_bsize = LOG2VALUE(LOG_WQ_DATA_ENTRY_BSIZE);

    struct wqe_data_seg *rx_wqes, *dseg;
    rx_wqes = (struct wqe_data_seg *)dev_ctx.rq_ctx.rq_ring;
    dseg = rx_wqes;

    flexio_uintptr_t host_addr = host_rq_addr, dpa_addr = dpa_rq_addr;

    for (int i = 0; i < num_of_wqes; i++)
    {
        dseg->byte_count = cpu_to_be32(data_chunk_bsize);
        if (i % 2 == 0)
        {
            dseg->lkey = cpu_to_be32(host_rq_lkey);
            dseg->addr = cpu_to_be64(host_addr);
        }
        else
        {
            dseg->lkey = cpu_to_be32(dpa_rq_lkey);
            dseg->addr = cpu_to_be64(dpa_addr);
        }

        dseg++;
        host_addr += data_chunk_bsize;
        dpa_addr += data_chunk_bsize;
    }
    __dpa_thread_system_fence();

    __dpa_thread_memory_writeback();
}

struct element
{
    struct element *next;
    size_t padding[7];
};

void begin_dpa_rq_loop(void)
{
    size_t num_elem = 49152;
    struct element *data = (struct element *)dpa_rq_addr;

    for (size_t i = 0; i < num_elem; i++)
    {
        data[i].next = &data[(i + 1) % num_elem];
        for (int j = 0; j < 7; j++)
        {
            data[i].padding[j] = 0;
        }
    }
    register size_t t1, t2, count;
    long loop_total = 20000000;
    struct element *p = data;

    long loop_num = 0;
    t1 = __dpa_thread_cycles();

    while (loop_num < loop_total)
    {
        p = p->next;
        loop_num++;
    }
    t2 = __dpa_thread_cycles();

    count = loop_num;
    flexio_dev_print("%ld %ld %p\n", t2 - t1, count, (void *)p);

    sleep_(20);

    loop_total = num_elem;
    loop_num = 0;
    p = data;
    t1 = __dpa_thread_cycles();

    while (loop_num < loop_total)
    {
        p = p->next;
        loop_num++;
    }
    t2 = __dpa_thread_cycles();
    count = loop_num;

    flexio_dev_print("%ld %ld %p\n", t2 - t1, count, (void *)p);
}

/*
 * This function is called when a new packet is received to RQ's CQ.
 * Upon receiving a packet, the function will iterate over all received packets and process them.
 * Once all packets in the CQ are processed, the CQ will be rearmed to receive new packets events.
 */

static const size_t batch_size = 4096;
static const size_t count_size = 1;
static const size_t rq_pre_load = 0;
void
    __dpa_global__
    dpa_refactor_device_event_handler(uint64_t __unused arg0)
{
    struct flexio_dev_thread_ctx *dtctx;

    flexio_dev_get_thread_ctx(&dtctx);

    if (dev_ctx.is_initalized == 0)
        flexio_dev_thread_reschedule();

    register uint64_t load_num = 0;
    register uint64_t begin_time = 0;
    static uint64_t dummy = 0;

    char *sq_data;
    uint64_t pos = 0;
    static size_t now_pos = 0;
    register size_t now_pos_loop = now_pos;
    while (pos < batch_size / 64)
    {
        sq_data = NULL;
        while (flexio_dev_cqe_get_owner(dev_ctx.rqcq_ctx.cqe) != dev_ctx.rqcq_ctx.cq_hw_owner_bit)
        {
            flexio_dev_print("receive packet\n");
            // init_rq_ring();
            // begin_dpa_rq_loop();
            flexio_dev_print("finish init packet\n");

            sleep_(10000);
            flexio_dev_print("finish sleep\n");

            uint32_t data_sz;
            char *rq_data = receive_packet(&dev_ctx.rqcq_ctx, &dev_ctx.rq_ctx, &data_sz);
            if (data_sz == 73)
            {
                step_rq(&dev_ctx.rq_ctx);
                step_cq(&dev_ctx.rqcq_ctx);

                continue;
            }
            register uint64_t *tmp_ptr = (uint64_t *)rq_data;

            tmp_ptr = (uint64_t *)(rq_data + (pos + now_pos_loop) * LOG2VALUE(LOG_WQ_DATA_ENTRY_BSIZE));
            // __dpa_thread_memory_fence(__DPA_R, __DPA_R);

            // begin_time = __dpa_thread_cycles();
            // for (uint32_t ci = 0; ci < count_size; ci++)
            // {
            //     for (uint32_t i = 0; i < (LOG2VALUE(LOG_WQ_DATA_ENTRY_BSIZE)) / 8; i += 8)
            //     {
            //         load_num += *tmp_ptr;
            //         tmp_ptr += 8;
            //     }
            // }
            // __dpa_thread_memory_fence(__DPA_R, __DPA_R);

            // begin_time = __dpa_thread_cycles() - begin_time;

            for (uint32_t i = 0; i < (LOG2VALUE(LOG_WQ_DATA_ENTRY_BSIZE)) / 8; i += 8)
            {
                __dpa_thread_memory_fence(__DPA_R, __DPA_R);
                begin_time = __dpa_thread_cycles();
                load_num += *tmp_ptr;
                begin_time = __dpa_thread_cycles() - begin_time;
                __dpa_thread_memory_fence(__DPA_R, __DPA_W);
                *(tmp_ptr + 7) = begin_time;
                tmp_ptr += 8;
            }

            dummy += load_num;

            // flexio_dev_print("%lu,%lu\n", pos + now_pos_loop, begin_time);
            flexio_dev_print("%lu", pos + now_pos_loop);
            tmp_ptr = (uint64_t *)(rq_data + (pos + now_pos_loop) * LOG2VALUE(LOG_WQ_DATA_ENTRY_BSIZE));
            for (uint32_t i = 0; i < (LOG2VALUE(LOG_WQ_DATA_ENTRY_BSIZE)) / 8; i += 8)
            {
                flexio_dev_print(",%lu", *(tmp_ptr + 7));
                tmp_ptr += 8;
            }
            flexio_dev_print("\n");

            if ((uint64_t *)rq_data != rq_buff)
            {
                LOG_E("%p %p %u Assert failed\n", (void *)rq_data, (void *)rq_buff, data_sz);
            }
            pos = (pos + count_size);

            sq_data = get_next_send_buf(&dev_ctx.dt_ctx, LOG_WQ_DATA_ENTRY_BSIZE);
            memcpy(sq_data, rq_data, 14);
            for (int byte = 0; byte < 6; byte++)
            {
                char tmp = sq_data[byte];
                sq_data[byte] = sq_data[byte + 6];
                /* dst and src MACs are aligned one after the other in the ether header */
                sq_data[byte + 6] = tmp;
            }
            prepare_send_packet(&dev_ctx.sq_ctx, sq_data, data_sz);
            step_rq(&dev_ctx.rq_ctx);
            step_cq(&dev_ctx.rqcq_ctx);
            break;
        }
        while (sq_data != NULL && flexio_dev_cqe_get_owner(dev_ctx.rqcq_ctx.cqe) != dev_ctx.rqcq_ctx.cq_hw_owner_bit)
        {
            step_rq(&dev_ctx.rq_ctx);
            step_cq(&dev_ctx.rqcq_ctx);
        }
        if (sq_data != NULL)
        {
            __dpa_thread_memory_fence(__DPA_R, __DPA_R);

            uint64_t *tmp_ptr = dev_ctx.dt_ctx.sq_tx_buff;
            for (size_t i = 0; i < LOG2VALUE(13); i++)
            {
                for (size_t j = 0; j < LOG2VALUE(12); j += 8)
                {
                    load_num += *tmp_ptr;
                    tmp_ptr++;
                }
            }
            __dpa_thread_memory_fence(__DPA_R, __DPA_R);

            tmp_ptr = rq_buff;
            for (size_t i = 0; i < rq_pre_load; i++)
            {
                for (size_t j = 0; j < LOG2VALUE(LOG_WQ_DATA_ENTRY_BSIZE); j += 8)
                {
                    load_num += *tmp_ptr;
                    tmp_ptr++;
                }
            }
            __dpa_thread_memory_fence(__DPA_R, __DPA_W);

            finish_send_packet(dtctx, &dev_ctx.sq_ctx);
        }
    }
    now_pos += pos;
    now_pos %= batch_size;
    flexio_dev_print("%lu %lu\n", dummy, load_num);

    __dpa_thread_fence(__DPA_MEMORY, __DPA_W, __DPA_W);
    flexio_dev_cq_arm(dtctx, dev_ctx.rqcq_ctx.cq_idx, dev_ctx.rqcq_ctx.cq_number);
    flexio_dev_thread_reschedule();
}
