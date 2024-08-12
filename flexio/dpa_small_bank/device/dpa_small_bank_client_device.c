#include <stddef.h>
#include "wrapper_flexio_device.h"
#include "../common/dpa_small_bank_common.h"

flexio_dev_rpc_handler_t dpa_small_bank_client_device_init;
flexio_dev_rpc_handler_t dpa_small_bank_client_device_ping_packet;
flexio_dev_rpc_handler_t dpa_small_bank_client_device_stop;
flexio_dev_event_handler_t dpa_small_bank_client_device_event_handler;

#define MAX_THREADS 190

#define CONCURRENCY 64

struct Transition {
    size_t user1_id;
    size_t user2_id;

    uint8_t lock_user1_checking;
    uint8_t lock_user1_saving;
    uint8_t lock_user2_checking;
    uint8_t lock_user2_saving;
    uint8_t changed_user1_checking;
    uint8_t changed_user1_saving;
    uint8_t changed_user2_checking;
    uint8_t changed_user2_saving;

    uint32_t req_id;
    uint8_t transmit_type;
    uint8_t now_type;
    size_t user1_checking_balance;
    size_t user1_saving_balance;
    size_t user2_checking_balance;
    size_t user2_saving_balance;
}__attribute__((__aligned__(64)));

static struct device_context {
    uint32_t lkey;            /* Local memory key */
    uint32_t is_initalized;   /* Initialization flag */
    struct cq_ctx_t rqcq_ctx; /* RQ CQ context */
    struct cq_ctx_t sqcq_ctx; /* SQ CQ context */
    struct rq_ctx_t rq_ctx;   /* RQ context */
    struct sq_ctx_t sq_ctx;   /* SQ context */
    struct dt_ctx_t dt_ctx;   /* DT context */
    uint32_t packets_count;
    uint64_t req_id;
    struct Transition transitions[CONCURRENCY];
} __attribute__((__aligned__(64))) dev_ctxs[MAX_THREADS];

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

__dpa_rpc__ uint64_t
dpa_small_bank_client_device_init(uint64_t data) {
    struct queue_config_data *shared_data = (struct queue_config_data *)data;
    Assert(shared_data->thread_index < MAX_THREADS);

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

static void prepare_packet(void *sq_data, size_t thread_index, struct flexio_dev_thread_ctx *dtctx) {
    (void)dtctx;
    struct ether_hdr *eth_hdr;
    eth_hdr = (struct ether_hdr *)sq_data;
    eth_hdr->src_addr = CLIENT_ADDR;
    eth_hdr->src_addr.addr_bytes[5] += thread_index;
    eth_hdr->dst_addr = SERVER_ADDR;
    eth_hdr->dst_addr.addr_bytes[5] += thread_index;
    eth_hdr->ether_type = cpu_to_be16(0x0800);
}

static void send_ping_packet(struct flexio_dev_thread_ctx *dtctx, struct device_context *dev_ctx) {
    char *sq_data = get_next_send_buf(&dev_ctx->dt_ctx, LOG_WQ_DATA_ENTRY_BSIZE);

    struct PingRPCReq *req = (struct PingRPCReq *)(sq_data + 14);
    req->type = RPC_PING;

    prepare_send_packet(&dev_ctx->sq_ctx, sq_data, 14 + sizeof(struct PingRPCReq));
    finish_send_packet(dtctx, &dev_ctx->sq_ctx);
}

__dpa_rpc__ uint64_t dpa_small_bank_client_device_ping_packet(uint64_t __unused dummy) {
    struct flexio_dev_thread_ctx *dtctx;
    flexio_dev_get_thread_ctx(&dtctx);

    for (size_t i = 0;i < MAX_THREADS;i++) {
        struct device_context *dev_ctx = &dev_ctxs[i];
        char *sq_data;
        if (!dev_ctx->is_initalized) {
            continue;
        }
        for (size_t entry = 0;entry < LOG2VALUE(LOG_SQ_RING_DEPTH);entry++) {
            sq_data = get_next_send_buf(&dev_ctx->dt_ctx, LOG_WQ_DATA_ENTRY_BSIZE);
            prepare_packet(sq_data, i, dtctx);
        }
        send_ping_packet(dtctx, dev_ctx);
    }
    return 0;
}

__dpa_rpc__ uint64_t dpa_small_bank_client_device_stop(uint64_t __unused dummy) {
    is_stop = 1;
    return 0;
}
static void init_transition(struct Transition *trans, size_t index) {
    trans->user1_id = 0;
    trans->user2_id = 0;
    trans->lock_user1_checking = trans->lock_user1_saving
        = trans->lock_user2_checking = trans->lock_user2_saving = 0;
    trans->req_id = index;
    if (index * 100 < 15 * CONCURRENCY) {
        trans->transmit_type = Amalgamate;
        trans->user1_id = __dpa_thread_inst_ret() % NUM_ACCOUNTS;
        trans->user2_id = __dpa_thread_cycles() % NUM_ACCOUNTS;
        trans->lock_user1_checking = WRITE_LOCK;
        trans->lock_user2_saving = WRITE_LOCK;
        trans->changed_user1_checking = 1;
        trans->changed_user2_saving = 1;
    } else if (index * 100 < 30 * CONCURRENCY) {
        trans->transmit_type = Balance;
        trans->user1_id = __dpa_thread_inst_ret() % NUM_ACCOUNTS;
        trans->user2_id = trans->user1_id;
    } else if (index * 100 < 45 * CONCURRENCY) {
        trans->transmit_type = DepositChecking;
        trans->user1_id = __dpa_thread_inst_ret() % NUM_ACCOUNTS;
        trans->user2_id = trans->user1_id;
        trans->lock_user1_checking = WRITE_LOCK;
        trans->changed_user1_checking = 1;
    } else if (index * 100 < 70 * CONCURRENCY) {
        trans->transmit_type = SendPayment;
        trans->user1_id = __dpa_thread_inst_ret() % NUM_ACCOUNTS;
        trans->user2_id = __dpa_thread_cycles() % NUM_ACCOUNTS;
        trans->lock_user1_checking = WRITE_LOCK;
        trans->lock_user2_checking = WRITE_LOCK;
        trans->changed_user1_checking = 1;
        trans->changed_user2_checking = 1;
    } else if (index * 100 < 85 * CONCURRENCY) {
        trans->transmit_type = TransactSavings;
        trans->user1_id = __dpa_thread_inst_ret() % NUM_ACCOUNTS;
        trans->lock_user1_saving = WRITE_LOCK;
        trans->changed_user1_saving = 1;
    } else {
        trans->transmit_type = WriteCheck;
        trans->user1_id = __dpa_thread_inst_ret() % NUM_ACCOUNTS;
        trans->lock_user1_checking = WRITE_LOCK;
        trans->changed_user1_checking = 1;
    }
}

static void reload_transition(struct Transition *now_trans, struct GetDataReq *req) {
    req->type = RPC_GET_DATA;
    req->req_number = now_trans->req_id;
    req->user1_id = now_trans->user1_id;
    req->user2_id = now_trans->user2_id;
    req->get_user1_checking = now_trans->lock_user1_checking;
    req->get_user1_saving = now_trans->lock_user1_saving;
    req->get_user2_checking = now_trans->lock_user2_checking;
    req->get_user2_saving = now_trans->lock_user2_saving;
}

static void process_packet(struct flexio_dev_thread_ctx *dtctx, struct device_context *dev_ctx) {
    uint32_t data_sz;
    char *rq_data;

    /* Extract relevant data from CQE */
    rq_data = receive_packet(&dev_ctx->rqcq_ctx, &dev_ctx->rq_ctx, &data_sz);

    switch (*(uint8_t *)(rq_data + 14)) {
    case RPC_PING: {
        for (size_t i = 0;i < CONCURRENCY;i++) {
            struct Transition *now_trans = dev_ctx->transitions + i;
            init_transition(now_trans, i);
        }
        for (size_t i = 0;i < CONCURRENCY;i++) {
            struct Transition *now_trans = dev_ctx->transitions + i;
            char *sq_data = get_next_send_buf(&dev_ctx->dt_ctx, LOG_WQ_DATA_ENTRY_BSIZE);
            struct GetDataReq *req = (struct GetDataReq *)(sq_data + 14);
            reload_transition(now_trans, req);
            prepare_send_packet(&dev_ctx->sq_ctx, sq_data, 14 + sizeof(struct GetDataReq));
        }
        finish_send_packet_batch(dtctx, &dev_ctx->sq_ctx, CONCURRENCY);
        break;
    }
    case RPC_GET_DATA: {
        struct GetDataResp *resp = (struct GetDataResp *)(rq_data + 14);

        struct Transition *now_trans = dev_ctx->transitions + (resp->req_number % CONCURRENCY);
        now_trans->user1_checking_balance = resp->user1_checking;
        now_trans->user1_saving_balance = resp->user1_saving;
        now_trans->user2_checking_balance = resp->user2_checking;
        now_trans->user2_saving_balance = resp->user2_saving;

        char *sq_data = get_next_send_buf(&dev_ctx->dt_ctx, LOG_WQ_DATA_ENTRY_BSIZE);
        struct GetLockReq *req = (struct GetLockReq *)(sq_data + 14);
        req->type = RPC_GET_LOCK;
        req->req_number = resp->req_number;
        req->user1_id = now_trans->user1_id;
        req->user2_id = now_trans->user2_id;

        req->lock_user1_checking = now_trans->lock_user1_checking;
        req->lock_user1_saving = now_trans->lock_user1_saving;
        req->lock_user2_checking = now_trans->lock_user2_checking;
        req->lock_user2_saving = now_trans->lock_user2_saving;

        prepare_send_packet(&dev_ctx->sq_ctx, sq_data, 14 + sizeof(struct GetLockReq));
        finish_send_packet(dtctx, &dev_ctx->sq_ctx);

        break;
    }
    case RPC_GET_LOCK: {
        struct GetLockResp *resp = (struct GetLockResp *)(rq_data + 14);

        struct Transition *now_trans = dev_ctx->transitions + (resp->req_number % CONCURRENCY);
        char *sq_data = get_next_send_buf(&dev_ctx->dt_ctx, LOG_WQ_DATA_ENTRY_BSIZE);

        if (resp->success) {
            struct FinishReq *req = (struct FinishReq *)(sq_data + 14);

            req->type = RPC_FINISH;
            req->req_number = resp->req_number;

            req->user1_id = now_trans->user1_id;
            req->user2_id = now_trans->user2_id;

            req->unlock_user1_checking = now_trans->lock_user1_checking;
            req->unlock_user1_saving = now_trans->lock_user1_saving;
            req->unlock_user2_checking = now_trans->lock_user2_checking;
            req->unlock_user2_saving = now_trans->lock_user2_saving;

            req->changed_user1_checking = now_trans->changed_user1_checking;
            req->changed_user1_saving = now_trans->changed_user1_saving;
            req->changed_user2_checking = now_trans->changed_user2_checking;
            req->changed_user2_saving = now_trans->changed_user2_saving;

            // flexio_dev_print("%u\n", now_trans->transmit_type);
            // flexio_dev_print("%ld\n", req->user2_checking);
            // flexio_dev_print("%p %p %p\n", (void *)(req), (void *)(&req->user2_checking), (void *)(&req->user2_saving));
            // flexio_dev_print("%ld\n", req->user2_saving);

            // flexio_dev_print("%ld\n", req->user1_checking);
            // switch (now_trans->transmit_type) {
            // case Amalgamate:
            //     req->user2_saving = now_trans->user1_checking_balance + now_trans->user2_saving_balance;
            //     req->user1_checking = 0;
            //     break;
            // case Balance:
            //     break;
            // case DepositChecking:
            //     req->user1_checking = now_trans->user1_checking_balance + 1;
            //     break;
            // case SendPayment:
            //     req->user1_checking = now_trans->user1_checking_balance - 1;
            //     req->user2_checking = now_trans->user2_checking_balance + 1;
            //     break;
            // case TransactSavings:
            //     req->user1_saving = now_trans->user1_saving_balance - 1;
            //     break;
            // case WriteCheck:
            //     req->user1_checking = now_trans->user1_checking_balance - 1;
            //     break;
            // }
            prepare_send_packet(&dev_ctx->sq_ctx, sq_data, 14 + sizeof(struct FinishReq));
        } else {
            now_trans->user1_id = __dpa_thread_inst_ret() % NUM_ACCOUNTS;
            now_trans->user2_id = __dpa_thread_cycles() % NUM_ACCOUNTS;
            now_trans->req_id = resp->req_number + CONCURRENCY;

            struct GetDataReq *req = (struct GetDataReq *)(sq_data + 14);
            reload_transition(now_trans, req);
            prepare_send_packet(&dev_ctx->sq_ctx, sq_data, 14 + sizeof(struct GetDataReq));
        }
        finish_send_packet(dtctx, &dev_ctx->sq_ctx);
        break;
    }
    case RPC_FINISH: {
        struct FinishResp *resp = (struct FinishResp *)(rq_data + 14);
        struct Transition *now_trans = dev_ctx->transitions + (resp->req_number % CONCURRENCY);
        now_trans->user1_id = __dpa_thread_inst_ret() % NUM_ACCOUNTS;
        now_trans->user2_id = __dpa_thread_cycles() % NUM_ACCOUNTS;
        now_trans->req_id = resp->req_number + CONCURRENCY;

        char *sq_data = get_next_send_buf(&dev_ctx->dt_ctx, LOG_WQ_DATA_ENTRY_BSIZE);
        struct GetDataReq *req = (struct GetDataReq *)(sq_data + 14);
        reload_transition(now_trans, req);
        prepare_send_packet(&dev_ctx->sq_ctx, sq_data, 14 + sizeof(struct GetDataReq));
        finish_send_packet(dtctx, &dev_ctx->sq_ctx);
        break;
    }
    default:
        LOG_I("INVALID TYPE\n");
    }
}

void __dpa_global__ dpa_small_bank_client_device_event_handler(uint64_t index) {
    struct flexio_dev_thread_ctx *dtctx;
    struct device_context *dev_ctx = &dev_ctxs[index];
    flexio_dev_get_thread_ctx(&dtctx);
    if (dev_ctx->is_initalized == 0) {
        flexio_dev_thread_reschedule();
    }

    flexio_dev_print("thread %ld begin\n", index);
    while (!is_stop) {
        while (flexio_dev_cqe_get_owner(dev_ctx->rqcq_ctx.cqe) != dev_ctx->rqcq_ctx.cq_hw_owner_bit) {
            process_packet(dtctx, dev_ctx);
            step_rq(&dev_ctx->rq_ctx);
            step_cq(&dev_ctx->rqcq_ctx);
        }
    }
}
