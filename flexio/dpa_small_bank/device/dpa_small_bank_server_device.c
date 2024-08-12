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
#include "../common/dpa_small_bank_common.h"

flexio_dev_rpc_handler_t dpa_small_bank_server_device_init;
flexio_dev_event_handler_t dpa_small_bank_server_device_event_handler;

#define MAX_THREADS 190

/* Device context */
static struct device_context {
    uint32_t lkey;            /* Local memory key */
    uint32_t is_initalized;   /* Initialization flag */
    struct cq_ctx_t rqcq_ctx; /* RQ CQ context */
    struct cq_ctx_t sqcq_ctx; /* SQ CQ context */
    struct rq_ctx_t rq_ctx;   /* RQ context */
    struct sq_ctx_t sq_ctx;   /* SQ context */
    struct dt_ctx_t dt_ctx;   /* DT context */
    uint32_t finish_count;   /* Number of processed packets */
    uint32_t abort_count;
} __attribute__((__aligned__(64))) dev_ctxs[MAX_THREADS];


struct Savings {
    size_t custom_id;
    size_t balance;
    struct spinlock_s mutex;
}__attribute__((__aligned__(32)));

struct Savings *savings_global;

struct Checking {
    size_t custom_id;
    size_t balance;
    struct spinlock_s mutex;
}__attribute__((__aligned__(32)));

struct Checking *checking_global;

void init_saving_and_checking(flexio_uintptr_t global_addr_ptr) {
    savings_global = (struct Savings *)global_addr_ptr;
    for (size_t i = 0;i < NUM_ACCOUNTS;i++) {
        savings_global[i * NUM_SCALE].custom_id = i;
        savings_global[i * NUM_SCALE].balance = SIZE_MAX / 2;
        spin_init(&savings_global[i * NUM_SCALE].mutex);
    }
    checking_global = (struct Checking *)(savings_global + NUM_ACCOUNTS);
    for (size_t i = 0;i < NUM_ACCOUNTS;i++) {
        checking_global[i * NUM_SCALE].custom_id = i;
        checking_global[i * NUM_SCALE].balance = SIZE_MAX / 2;
        spin_init(&checking_global[i * NUM_SCALE].mutex);
    }
}

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

__dpa_rpc__ uint64_t
dpa_small_bank_server_device_init(uint64_t data) {
    struct queue_config_data *shared_data = (struct queue_config_data *)data;
    Assert(shared_data->thread_index < MAX_THREADS);

    if (shared_data->thread_index == 0) {
        init_saving_and_checking(shared_data->new_buffer_addr);
    }

    struct device_context *dev_ctx = &dev_ctxs[shared_data->thread_index];
    dev_ctx->lkey = shared_data->sq_data.wqd_mkey_id;

    init_cq(shared_data->rq_cq_data, &dev_ctx->rqcq_ctx);
    init_rq(shared_data->rq_data, &dev_ctx->rq_ctx);
    init_cq(shared_data->sq_cq_data, &dev_ctx->sqcq_ctx);
    init_sq(shared_data->sq_data, &dev_ctx->sq_ctx);

    init_send_sq(&dev_ctx->sq_ctx, LOG2VALUE(LOG_SQ_RING_DEPTH), dev_ctx->lkey);
    dev_ctx->dt_ctx.sq_tx_buff = (void *)shared_data->sq_data.wqd_daddr;
    dev_ctx->dt_ctx.tx_buff_idx = 0;
    dev_ctx->dt_ctx.data_idx_mask = ((1 << (LOG_SQ_RING_DEPTH)) - 1);

    dev_ctx->is_initalized = 1;
    LOG_I("Thread %u init success\n", shared_data->thread_index);
    return 0;
}
static void
process_packet(struct flexio_dev_thread_ctx *dtctx, struct device_context *dev_ctx) {
    uint32_t data_sz;
    char *rq_data;

    /* Extract relevant data from CQE */
    rq_data = receive_packet(&dev_ctx->rqcq_ctx, &dev_ctx->rq_ctx, &data_sz);

    char *sq_data;
    sq_data = rq_data;

    size_t src_mac = *(size_t *)sq_data;
    size_t dst_mac = *(size_t *)(sq_data + 6);
    *(size_t *)(sq_data) = dst_mac;
    *(size_t *)(sq_data + 6) = (src_mac & 0x0000FFFFFFFFFFFF) | (0x0008ll << 48);

    switch (*(uint8_t *)(sq_data + 14)) {
    case RPC_PING: {
        struct PingRPCReq *req = (struct PingRPCReq *)(sq_data + 14);
        struct PingRPCResp resp;
        resp.type = RPC_PING;
        resp.req_number = req->req_number;
        resp.status = 0;

        *(struct PingRPCResp *)(sq_data + 14) = resp;
        data_sz = 14 + sizeof(struct PingRPCResp);
        break;
    }
    case RPC_GET_DATA: {
        struct GetDataReq *req = (struct GetDataReq *)(sq_data + 14);
        struct GetDataResp resp;
        resp.type = RPC_GET_DATA;
        resp.req_number = req->req_number;
        resp.status = 0;

        size_t user1_id = req->user1_id;
        size_t user2_id = req->user2_id;
        if (req->get_user1_checking) {
            resp.user1_checking = checking_global[user1_id * NUM_SCALE].balance;
        }
        if (req->get_user1_saving) {
            resp.user1_saving = savings_global[user1_id * NUM_SCALE].balance;
        }
        if (req->get_user2_checking) {
            resp.user2_checking = checking_global[user2_id * NUM_SCALE].balance;
        }
        if (req->get_user2_saving) {
            resp.user2_saving = savings_global[user2_id * NUM_SCALE].balance;
        }

        *(struct GetDataResp *)(sq_data + 14) = resp;
        data_sz = 14 + sizeof(struct GetDataResp);
        break;
    }
    case RPC_GET_LOCK: {
        struct GetLockReq *req = (struct GetLockReq *)(sq_data + 14);
        struct GetLockResp resp;
        resp.type = RPC_GET_LOCK;
        resp.req_number = req->req_number;
        resp.status = 0;

        size_t user1_id = req->user1_id;
        size_t user2_id = req->user2_id;

        uint32_t lock_flag = 0;
        uint8_t lock_failed = 0;
        if (!lock_failed && req->lock_user1_checking) {
            lock_flag = spin_trylock(&checking_global[user1_id * NUM_SCALE].mutex);
            if (lock_flag != 0) {
                lock_failed = 1;
            } else {
                lock_flag += 1 << 0;
            }
        }
        if (!lock_failed && req->lock_user1_saving) {
            lock_flag = spin_trylock(&savings_global[user1_id * NUM_SCALE].mutex);
            if (lock_flag != 0) {
                lock_failed = 1;
            } else {
                lock_flag += 1 << 1;
            }
        }
        if (!lock_failed && req->lock_user2_checking) {
            lock_flag = spin_trylock(&checking_global[user2_id * NUM_SCALE].mutex);
            if (lock_flag != 0) {
                lock_failed = 1;
            } else {
                lock_flag += 1 << 2;
            }
        }
        if (!lock_failed && req->lock_user2_saving) {
            lock_flag = spin_trylock(&savings_global[user2_id * NUM_SCALE].mutex);
            if (lock_flag != 0) {
                lock_failed = 1;
            } else {
                lock_flag += 1 << 3;
            }
        }
        if (lock_failed) {
            if (lock_flag & (1 << 0)) {
                spin_unlock(&checking_global[user1_id * NUM_SCALE].mutex);
            }
            if (lock_flag & (1 << 1)) {
                spin_unlock(&savings_global[user1_id * NUM_SCALE].mutex);
            }
            if (lock_flag & (1 << 2)) {
                spin_unlock(&checking_global[user2_id * NUM_SCALE].mutex);
            }
            if (lock_flag & (1 << 3)) {
                spin_unlock(&savings_global[user2_id * NUM_SCALE].mutex);
            }
            dev_ctx->abort_count++;
        }

        resp.success = !lock_failed;
        *(struct GetLockResp *)(sq_data + 14) = resp;
        data_sz = 14 + sizeof(struct GetLockResp);
        break;
    }
    case RPC_FINISH: {
        struct FinishReq *req = (struct FinishReq *)(sq_data + 14);

        struct FinishResp resp;
        resp.type = RPC_FINISH;
        resp.req_number = req->req_number;
        resp.status = 0;
        resp.success = 1;

        size_t user1_id = req->user1_id;
        size_t user2_id = req->user2_id;
        if (req->changed_user1_checking) {
            checking_global[user1_id * NUM_SCALE].balance = req->user1_checking;
        }
        if (req->changed_user1_saving) {
            savings_global[user1_id * NUM_SCALE].balance = req->user1_saving;
        }
        if (req->changed_user2_checking) {
            checking_global[user2_id * NUM_SCALE].balance = req->user2_checking;
        }
        // can't access req->user2_saving
        // if (req->changed_user2_saving) {
        //     savings_global[user2_id * NUM_SCALE].balance = req->user2_saving;
        // }

        if (req->unlock_user1_checking) {
            spin_unlock(&checking_global[user1_id * NUM_SCALE].mutex);
        }
        if (req->unlock_user1_saving) {
            spin_unlock(&savings_global[user1_id * NUM_SCALE].mutex);
        }
        if (req->unlock_user2_checking) {
            spin_unlock(&checking_global[user2_id * NUM_SCALE].mutex);
        }
        if (req->unlock_user2_saving) {
            spin_unlock(&savings_global[user2_id * NUM_SCALE].mutex);
        }


        *(struct FinishResp *)(sq_data + 14) = resp;
        data_sz = 14 + sizeof(struct FinishResp);
        dev_ctx->finish_count++;
        if (dev_ctx->finish_count == 1000000 && dev_ctx->thread) {
            flexio_dev_print("%u %u\n", dev_ctx->finish_count, dev_ctx->abort_count);
            dev_ctx->finish_count = 0;
            dev_ctx->abort_count = 0;
        }
        break;
    }
    default:
        LOG_I("Unknown RPC type %d\n", *(uint8_t *)(sq_data + 14));
    }

    prepare_send_packet(&dev_ctx->sq_ctx, sq_data, data_sz);

    finish_send_packet(dtctx, &dev_ctx->sq_ctx);
}

void __dpa_global__ dpa_small_bank_server_device_event_handler(uint64_t index) {
    struct flexio_dev_thread_ctx *dtctx;
    struct device_context *dev_ctx = &dev_ctxs[index];
    flexio_dev_get_thread_ctx(&dtctx);

    if (dev_ctx->is_initalized == 0) {
        flexio_dev_thread_reschedule();
    }

    flexio_dev_print("thread %ld begin\n", index);

    while (dtctx != NULL) {
        while (flexio_dev_cqe_get_owner(dev_ctx->rqcq_ctx.cqe) != dev_ctx->rqcq_ctx.cq_hw_owner_bit) {
            process_packet(dtctx, dev_ctx);
            step_rq(&dev_ctx->rq_ctx);
            step_cq(&dev_ctx->rqcq_ctx);
        }
    }
}
