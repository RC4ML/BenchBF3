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

#include "utils_common.h"

DOCA_LOG_REGISTER(DPU_LOCAL_SAMPLE);

#define SLEEP_IN_NANOS (10 * 1000) /* Sample the job every 10 microseconds  */

/*
 * Checks that the two buffers are not overlap each other
 *
 * @dst_buffer [in]: Destination buffer
 * @src_buffer [in]: Source buffer
 * @length [in]: Length of both buffers
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
memory_ranges_overlap(const char *dst_buffer, const char *src_buffer, size_t length) {
    const char *dst_range_end = dst_buffer + length;
    const char *src_range_end = src_buffer + length;

    if (((dst_buffer >= src_buffer) && (dst_buffer < src_range_end)) ||
        ((src_buffer >= dst_buffer) && (src_buffer < dst_range_end))) {
        return DOCA_ERROR_INVALID_VALUE;
    }

    return DOCA_SUCCESS;
}

/*
 * Run DOCA DMA local copy sample
 *
 * @pcie_addr [in]: Device PCI address
 * @dst_buffer [in]: Destination buffer
 * @src_buffer [in]: Source buffer to copy
 * @length [in]: Buffer's size
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t
dma_local_copy(std::string &pci_addr, char *dst_buffer, char *src_buffer, size_t length) {
    program_core_objects state;
    doca_error_t result;
    uint32_t max_bufs = 3; /* Two buffers for source and destination */

    if (dst_buffer == nullptr || src_buffer == nullptr || length == 0) {
        DOCA_LOG_ERR("Invalid input values, addresses and sizes must not be 0");
        return DOCA_ERROR_INVALID_VALUE;
    }

    result = memory_ranges_overlap(dst_buffer, src_buffer, length);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Memory ranges must not overlap");
        return result;
    }

    state.dev = DOCA::open_doca_device_with_pci(pci_addr.c_str(), DOCA::dma_jobs_is_supported);

    auto *dma_ctx = new DOCA::dma(state.dev);

    state.ctx = new DOCA::ctx(dma_ctx);

    result = state.init_core_objects(max_bufs, dma_ctx);
    if (result != DOCA_SUCCESS) {
        exit(1);
    }
    state.dst_mmap->set_memrange_and_start(dst_buffer, length);
    state.src_mmap->set_memrange_and_start(src_buffer, length);

    /* Clear destination memory buffer */
    memset(dst_buffer, 0, length);



    /* Construct DOCA buffer for each address range */
    DOCA::buf *src_doca_buf1 = DOCA::buf::buf_inventory_buf_by_data(state.buf_inv, state.src_mmap, src_buffer, length / 2);
    DOCA::buf *src_doca_buf2 = DOCA::buf::buf_inventory_buf_by_data(state.buf_inv, state.src_mmap, src_buffer + length / 2, length / 2);
    src_doca_buf1->buf_append(src_doca_buf2);

    DOCA::buf *dst_doca_buf = DOCA::buf::buf_inventory_buf_by_addr(state.buf_inv, state.dst_mmap, dst_buffer, length);

    auto complete_callback = [](struct doca_dma_task_memcpy *useless_task, union doca_data task_user_data,
        union doca_data ctx_user_data) -> void {
            _unused(task_user_data);
            _unused(ctx_user_data);
            doca_task_free(doca_dma_task_memcpy_as_task(useless_task));
        };

    auto error_callback = [](struct doca_dma_task_memcpy *useless_task, union doca_data task_user_data,
        union doca_data ctx_user_data) -> void {
            _unused(useless_task);
            _unused(task_user_data);
            _unused(ctx_user_data);
            throw DOCA::DOCAException(DOCA_ERROR_BAD_STATE, __FILE__, __LINE__);
        };

    dma_ctx->set_task_conf(WORKQ_DEPTH, complete_callback, error_callback);
    state.pe->start_ctx();


    DOCA::dma_task_memcpy *dma_task = new DOCA::dma_task_memcpy(dma_ctx, src_doca_buf1, dst_doca_buf, { 0 });
    dma_task->set_src(src_doca_buf1);
    dma_task->set_dst(dst_doca_buf);

    result = state.pe->submit(dma_task);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to submit DMA job: %s", doca_error_get_descr(result));
        exit(-1);
    }
    timespec ts;
    /* Wait for job completion */
    while (state.pe->get_num_inflight() > 0) {
        state.pe->poll_progress();
        /* Wait for the job to complete */
        ts.tv_sec = 0;
        ts.tv_nsec = SLEEP_IN_NANOS;
        nanosleep(&ts, &ts);
    }

    for (size_t i = 0;i < length;i++) {
        if (dst_buffer[i] != src_buffer[i]) {
            DOCA_LOG_ERR("Failed to verify DMA job: %s", doca_error_get_descr(result));
            return DOCA_ERROR_INVALID_VALUE;
        }
    }

    DOCA_LOG_INFO("Success, memory copied and verified as correct");

    delete src_doca_buf1;
    // warning: don't delete at here because src_doca_buf2 is appended to src_doca_buf1
//    delete src_doca_buf2;
    delete dst_doca_buf;

    delete state.ctx;
    delete dma_ctx;
    state.clean_all();

    return DOCA_SUCCESS;
}
