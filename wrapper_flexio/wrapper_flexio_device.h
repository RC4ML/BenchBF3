#pragma once

#include <libflexio-libc/stdio.h>
#include <libflexio-libc/string.h>
#include <libflexio-dev/flexio_dev_debug.h>
#include <libflexio-dev/flexio_dev.h>
#include <libflexio-dev/flexio_dev_queue_access.h>
#include <libflexio-dev/flexio_dev_endianity.h>
// /opt/mellanox/doca/lib/aarch64-linux-gnu/dpa_llvm/lib/clang/15.0.7/include/dpaintrin.h
#include <dpaintrin.h> //__dpa_thread_cycles

#include "common_cross.h"

#define TEST_DOUBLE 2

struct DevQP {
    uint32_t qp_num;
} __attribute__((__packed__, aligned(8)));

int get_double(int a);

#define Assert(condition)                         \
    if (!(condition))                             \
    {                                             \
        LOG_E("Assert failed: %s\n", #condition); \
        return -1;                                \
    }

static inline void sleep_(uint64_t x) {
    uint64_t interval = x * 10000000; // wouldn't exceed the 32bit timer two times
    do {
        volatile uint64_t sleep;
        for (sleep = 0; sleep < interval; sleep++)
            ;
    } while (0);
}

flexio_uintptr_t get_host_buffer(uint32_t window_id, uint32_t mkey, void *haddr);
flexio_uintptr_t get_host_buffer_with_dtctx(struct flexio_dev_thread_ctx *dtctx, uint32_t window_id, uint32_t mkey, void *haddr);

/* CQ Context */
struct cq_ctx_t {
    uint32_t cq_number;               /* CQ number */
    struct flexio_dev_cqe64 *cq_ring; /* CQEs buffer */
    struct flexio_dev_cqe64 *cqe;     /* Current CQE */
    uint32_t cq_idx;                  /* Current CQE IDX */
    uint8_t cq_hw_owner_bit;          /* HW/SW ownership */
    uint32_t *cq_dbr;                 /* CQ doorbell record */
    uint32_t cq_idx_mask;
};

/* RQ Context */
struct rq_ctx_t {
    uint32_t rq_number;                          /* RQ number */
    struct flexio_dev_wqe_rcv_data_seg *rq_ring; /* WQEs buffer */
    uint32_t *rq_dbr;                            /* RQ doorbell record */
    uint32_t rq_idx_mask;
};

/* SQ Context */
struct sq_ctx_t {
    uint32_t sq_number;                /* SQ number */
    uint32_t sq_wqe_seg_idx;           /* WQE segment index */
    union flexio_dev_sqe_seg *sq_ring; /* SQEs buffer */
    uint32_t *sq_dbr;                  /* SQ doorbell record */
    uint32_t sq_pi;                    /* SQ producer index */
    uint32_t sq_idx_mask;
};

/* SQ data buffer */
struct dt_ctx_t {
    void *sq_tx_buff;     /* SQ TX buffer */
    uint32_t tx_buff_idx; /* TX buffer index */
    uint32_t data_idx_mask;
};

struct host_rq_ctx_t {
    uint32_t rkey;            /* receive memory key, used for receive queue */
    uint32_t rq_window_id;   // often same as sq_window_id;

    void *host_rx_buff;
    flexio_uintptr_t dpa_rx_buff;
};

struct host_sq_ctx_t {
    uint32_t rkey;
    uint32_t sq_window_id; // often same as sq_window_id;

    void *host_tx_buff;
    flexio_uintptr_t dpa_tx_buff;
};

// used for rx_buff on DPA
__attribute__((__unused__)) static void *receive_packet(struct cq_ctx_t *cq_ctx, struct rq_ctx_t *rq_ctx, uint32_t *len) {
    uint32_t rq_wqe_idx = flexio_dev_cqe_get_wqe_counter(cq_ctx->cqe);
    *len = flexio_dev_cqe_get_byte_cnt(cq_ctx->cqe);
    return flexio_dev_rwqe_get_addr(&rq_ctx->rq_ring[rq_wqe_idx & rq_ctx->rq_idx_mask]);
}

// used for rx_buff on Arm/Host
__attribute__((__unused__)) static void *receive_packet_host(struct cq_ctx_t *cq_ctx, struct rq_ctx_t *rq_ctx, struct host_rq_ctx_t *host_rq_ctx, uint32_t *len) {
    uint32_t rq_wqe_idx = flexio_dev_cqe_get_wqe_counter(cq_ctx->cqe);
    *len = flexio_dev_cqe_get_byte_cnt(cq_ctx->cqe);
    // return (void *)(((flexio_uintptr_t)flexio_dev_rwqe_get_addr(&rq_ctx->rq_ring[rq_wqe_idx & rq_ctx->rq_idx_mask]) + host_rq_ctx->dpa_rx_buff - (flexio_uintptr_t)host_rq_ctx->host_rx_buff));

    return (void *)(((flexio_uintptr_t)flexio_dev_rwqe_get_addr(&rq_ctx->rq_ring[rq_wqe_idx & rq_ctx->rq_idx_mask]) - (flexio_uintptr_t)host_rq_ctx->host_rx_buff + host_rq_ctx->dpa_rx_buff));
}

// get a dpa address from a received packet on Arm/Host
__attribute__((__unused__)) inline static void *host_rq_addr_to_dpa_addr(void *host_addr, struct host_rq_ctx_t *host_rq_ctx) {
    return (void *)((flexio_uintptr_t)host_addr - (flexio_uintptr_t)host_rq_ctx->host_rx_buff + host_rq_ctx->dpa_rx_buff);
}

__attribute__((__unused__)) inline static void *host_sq_addr_to_dpa_addr(void *host_addr, struct host_sq_ctx_t *host_sq_ctx) {
    return (void *)((flexio_uintptr_t)host_addr - (flexio_uintptr_t)host_sq_ctx->host_tx_buff + host_sq_ctx->dpa_tx_buff);
}

/*
 * Get next data buffer entry
 *
 * @dt_ctx [in]: Data transfer context
 * @log_dt_entry_sz [in]: Log of data transfer entry size
 * @return: Data buffer entry
 */
__attribute__((__unused__)) inline static void *
get_next_send_buf(struct dt_ctx_t *dt_ctx, uint32_t log_dt_entry_sz) {
    uint32_t mask = ((dt_ctx->tx_buff_idx++ & dt_ctx->data_idx_mask) << log_dt_entry_sz);
    char *buff_p = (char *)dt_ctx->sq_tx_buff;

    return buff_p + mask;
}

/*
 * Get next SQE from the SQ ring
 *
 * @sq_ctx [in]: SQ context
 * @return: pointer to next SQE
 */
inline static union flexio_dev_sqe_seg *
get_next_sqe(struct sq_ctx_t *sq_ctx) {
    return &sq_ctx->sq_ring[sq_ctx->sq_wqe_seg_idx++ & sq_ctx->sq_idx_mask];
}

inline static union flexio_dev_sqe_seg *
get_next_data_sqe(struct sq_ctx_t *sq_ctx) {
    union flexio_dev_sqe_seg *res = &sq_ctx->sq_ring[(sq_ctx->sq_wqe_seg_idx + 2) & sq_ctx->sq_idx_mask];
    sq_ctx->sq_wqe_seg_idx += 4;
    return res;
}

inline static void init_send_sq(struct sq_ctx_t *sq_ctx, uint32_t entry_size, uint32_t lkey) {
    for (uint32_t i = 0; i < entry_size; i++) {
        union flexio_dev_sqe_seg *swqe;
        swqe = get_next_sqe(sq_ctx);
        flexio_dev_swqe_seg_ctrl_set(swqe, i, sq_ctx->sq_number,
            MLX5_CTRL_SEG_CE_CQE_ON_CQE_ERROR, FLEXIO_CTRL_SEG_SEND_EN);

        swqe = get_next_sqe(sq_ctx);
        flexio_dev_swqe_seg_eth_set(swqe, 0, 0, 0, NULL);

        swqe = get_next_sqe(sq_ctx);
        flexio_dev_swqe_seg_mem_ptr_data_set(swqe, 0, lkey, 0);

        swqe = get_next_sqe(sq_ctx);
    }
    sq_ctx->sq_wqe_seg_idx = 0;
}

// construct a send packet ring buffer
inline static void prepare_send_packet(struct sq_ctx_t *sq_ctx, void *data_addr, uint32_t data_sz) {
    union flexio_dev_sqe_seg *swqe;

    swqe = get_next_data_sqe(sq_ctx);

    swqe->mem_ptr_send_data.byte_count = cpu_to_be32(data_sz);
    swqe->mem_ptr_send_data.addr = cpu_to_be64((uint64_t)data_addr);
}

inline static void
finish_send_packet(struct flexio_dev_thread_ctx *dtctx, struct sq_ctx_t *sq_ctx) {
    register uint32_t sq_number = sq_ctx->sq_number;
    register uint32_t sq_pi = ++sq_ctx->sq_pi;
    __dpa_thread_memory_writeback();
    flexio_dev_qp_sq_ring_db(dtctx, sq_pi, sq_number);
    // __dpa_thread_fence(__DPA_MEMORY, __DPA_W, __DPA_W);
}

inline static void
finish_send_packet_batch(struct flexio_dev_thread_ctx *dtctx, struct sq_ctx_t *sq_ctx, size_t batch_size) {
    register uint32_t sq_number = sq_ctx->sq_number;
    sq_ctx->sq_pi += batch_size;
    register uint32_t sq_pi = sq_ctx->sq_pi;
    __dpa_thread_memory_writeback();
    flexio_dev_qp_sq_ring_db(dtctx, sq_pi, sq_number);
    // __dpa_thread_fence(__DPA_MEMORY, __DPA_W, __DPA_W);
}

inline static void
finish_send_packet_host(struct flexio_dev_thread_ctx *dtctx, struct sq_ctx_t *sq_ctx) {
    register uint32_t sq_number = sq_ctx->sq_number;
    register uint32_t sq_pi = ++sq_ctx->sq_pi;
    __dpa_thread_memory_writeback();
    __dpa_thread_window_writeback();
    flexio_dev_qp_sq_ring_db(dtctx, sq_pi, sq_number);
    // __dpa_thread_fence(__DPA_MEMORY, __DPA_W, __DPA_W);
}

inline static void
finish_send_packet_host_batch(struct flexio_dev_thread_ctx *dtctx, struct sq_ctx_t *sq_ctx, size_t batch_size) {
    register uint32_t sq_number = sq_ctx->sq_number;
    sq_ctx->sq_pi += batch_size;
    register uint32_t sq_pi = sq_ctx->sq_pi;
    __dpa_thread_memory_writeback();
    __dpa_thread_window_writeback();
    flexio_dev_qp_sq_ring_db(dtctx, sq_pi, sq_number);
    // __dpa_thread_fence(__DPA_MEMORY, __DPA_W, __DPA_W);
}


/*
 * Increase consumer index of the CQ,
 * Once a CQE is polled, the consumer index is increased.
 * Upon completing a CQ epoch, the HW owner bit is flipped.
 *
 * @cq_ctx [in]: CQ context
 */

inline static void
step_rq(struct rq_ctx_t *rq_ctx) {
    // __dpa_thread_fence(__DPA_MEMORY, __DPA_W, __DPA_W);
    flexio_dev_dbr_rq_inc_pi(rq_ctx->rq_dbr);
}
inline static void
step_cq(struct cq_ctx_t *cq_ctx) {
    uint32_t cq_idx = ++cq_ctx->cq_idx;
    uint32_t *cq_dbr = cq_ctx->cq_dbr;
    cq_ctx->cqe = &cq_ctx->cq_ring[cq_idx & cq_ctx->cq_idx_mask];
    /* check for wrap around */
    if (!(cq_idx & cq_ctx->cq_idx_mask))
        cq_ctx->cq_hw_owner_bit = !cq_ctx->cq_hw_owner_bit;

    // __dpa_thread_fence(__DPA_MEMORY, __DPA_W, __DPA_W);
    flexio_dev_dbr_cq_set_ci(cq_dbr, cq_idx);
    // __dpa_thread_memory_writeback();
}
