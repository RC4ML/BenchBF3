#include <stdlib.h>
#include <gflags/gflags.h>

#include <doca_log.h>
#include <infiniband/verbs.h>
#include <infiniband/mlx5_api.h>
#include <libflexio/flexio.h>
#include <numa.h>

#include "wrapper_flexio/wrapper_flexio.hpp"
#include "wrapper_flexio/common_cross.h"
#include "wrapper_flexio/common_host.hpp"

#include "common.hpp"
#include "mt_memory_cross.h"

extern "C"
{
    flexio_func_t bench_func;
    flexio_func_t get_dpa_mem_result;
    flexio_func_t get_host_mem_result;

}

extern struct flexio_app *mt_memory_device;

DOCA_LOG_REGISTER(MAIN);

int WORKER_BATCH_SIZE = 1;
int STRIDE = 1;
size_t LOOP = 0;
int ThreadsHost = 0;
int ThreadsPrivate = 0;

size_t MemSizePrivate = 0;
size_t MemSizeHost = 0;

flexio_uintptr_t prepare_private_memory(FLEX::Context *ctx, size_t size, int num) {
    if (num == 0) {
        return 0;
    }
    flexio_status ret;
    flexio_heap_mem_info info;
    ret = flexio_process_mem_info_get(ctx->get_process(), &info);
    Assert(ret == FLEXIO_STATUS_SUCCESS);
    Assert(size * num <= info.size);

    void *data;
    Assert(data = malloc(size * num));
    for (size_t i = 0; i < (size * num) / (sizeof(size_t)); i++) {
        ((uint64_t *)data)[i] = i + 10;
    }
    flexio_uintptr_t data_daddr;
    ret = flexio_copy_from_host(ctx->get_process(), data, size * num, &data_daddr);
    Assert(ret == FLEXIO_STATUS_SUCCESS);
    return data_daddr;
}

void *prepare_host_memory(FLEX::Context *ctx, size_t size, int num) {
    if (num == 0) {
        return NULL;
    }
    void *host_buffer;
    host_buffer = ctx->alloc_huge((size * num + 63) & (~63));
    Assert(host_buffer);
    ctx->reg_mr(host_buffer, (size * num + 63) & (~63));

    for (size_t i = 0; i < (size * num) / (sizeof(size_t)); i++) {
        ((uint64_t *)host_buffer)[i] = i + 1000;
    }
    return host_buffer;
}

void host_func() {
    FLEX::Context ctx("mlx5_bond_0");
    ctx.alloc_pd();

    ctx.create_process(mt_memory_device);
    ctx.create_window();

    ctx.generate_flexio_uar();
    ctx.print_init("mt_memory", 0);

    FLEX::CommandQ cmdq(ctx.get_process(), ThreadsHost + ThreadsPrivate, WORKER_BATCH_SIZE);

    while (MemSizeHost * ThreadsHost > 16UL * 1024 * 1024 * 1024) {
        MemSizeHost /= 2;
    }
    while (MemSizePrivate * ThreadsPrivate > 512 * 1024 * 1024) {
        MemSizePrivate /= 2;
    }

    void *host_mem = prepare_host_memory(&ctx, MemSizeHost, ThreadsHost);
    flexio_uintptr_t private_mem = prepare_private_memory(&ctx, MemSizePrivate, ThreadsPrivate);

    MemoryArg arg;
    arg.stride = STRIDE;
    arg.loop = LOOP;
    if (ThreadsHost != 0) {
        arg.window_id = ctx.get_window_id();
        arg.mkey = ctx.get_mr(0)->lkey;
    }

    for (int i = 0; i < ThreadsHost; i++) {
        arg.stream_id = i;
        arg.mem_type = HOST_MEM;
        arg.size = MemSizeHost;
        arg.haddr = (void *)(((size_t)host_mem) + MemSizeHost * i);
        flexio_uintptr_t arg_daddr = ctx.move_to_dev(arg);
        cmdq.add_task(bench_func, arg_daddr);
    }
    for (int i = 0; i < ThreadsPrivate; i++) {
        arg.stream_id = ThreadsHost + i;
        arg.mem_type = PRIVATE_MEM;
        arg.size = MemSizePrivate;
        arg.ddata = private_mem + MemSizePrivate * i;
        flexio_uintptr_t arg_daddr = ctx.move_to_dev(arg);
        cmdq.add_task(bench_func, arg_daddr);
    }
    LOG_I("Stride: %d = %d Bytes\n", STRIDE, STRIDE * 8);

    cmdq.run();
    auto time_us = cmdq.wait_run(300); // 100 seconds

    LOG_I("Finished, total time: %ld ms\n", time_us / 1000);
    if (ThreadsHost != 0) {
        size_t host_accessed_words = 1UL * LOOP * MemSizeHost / sizeof(size_t) / STRIDE;
        // size_t host_words_per_second = host_accessed_words * ThreadsHost * 1000 * 1000 / time_us;
        LOG_I("Host Threads: %d\n", ThreadsHost);
        LOG_I("Host Mem Size: %ld MB\n", MemSizeHost / 1024 / 1024);
        LOG_I("Host accsessed words(8B): %ld\n", host_accessed_words);
        // LOG_I("Host throughput: %.4lf GB/s\n", 1.0 * host_words_per_second * 8 / 1024 / 1024 / 1000);
        uint64_t rpc_ret_val;
        Assert(flexio_process_call(ctx.get_process(), &get_host_mem_result, &rpc_ret_val, ThreadsPrivate + ThreadsHost) ==
            FLEXIO_STATUS_SUCCESS);
        LOG_I("Host throughput: %.4lf GB/s\n", 1.0 * rpc_ret_val / 1024 / 1024 / 1000);
    }
    if (ThreadsPrivate != 0) {
        size_t private_accessed_words = 1UL * LOOP * MemSizePrivate / sizeof(size_t) / STRIDE;
        // size_t private_words_per_second = private_accessed_words * ThreadsPrivate * 1000 * 1000 / time_us;
        LOG_I("Private Threads: %d\n", ThreadsPrivate);
        LOG_I("Private Mem Size: %ld MB\n", MemSizePrivate / 1024 / 1024);
        LOG_I("Private accsessed words(8B): %ld\n", private_accessed_words);
        // LOG_I("Private throughput: %.4lf GB/s\n", 1.0 * private_words_per_second * 8 / 1024 / 1024 / 1000);
        uint64_t rpc_ret_val;
        Assert(flexio_process_call(ctx.get_process(), &get_dpa_mem_result, &rpc_ret_val, ThreadsPrivate + ThreadsHost) ==
            FLEXIO_STATUS_SUCCESS);
        LOG_I("Private throughput: %.4lf GB/s\n", 1.0 * rpc_ret_val / 1024 / 1024 / 1000);
    }
    ctx.flush();
    sleep(1); // wait log

}

DEFINE_int32(stride, 1, "Stride (words, 8B)");
DEFINE_int32(threads_private, 0, "private mem threads");
DEFINE_int32(threads_host, 0, "host mem threads");
DEFINE_int64(mem_host, 256UL * 1024 * 4, "host mem size");
DEFINE_int64(mem_private, 1024UL * 1024 * 4, "private mem size");
DEFINE_int64(loop, 1, "Loops");

int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    ThreadsHost = FLAGS_threads_host;
    ThreadsPrivate = FLAGS_threads_private;
    Assert(ThreadsHost + ThreadsPrivate > 0);
    STRIDE = FLAGS_stride;
    LOOP = FLAGS_loop;
    MemSizeHost = FLAGS_mem_host;
    MemSizePrivate = FLAGS_mem_private;
    struct doca_log_backend *stdout_logger = nullptr;
    Assert(doca_log_backend_create_with_file(stdout, &stdout_logger) == DOCA_SUCCESS);
    host_func();
    return EXIT_SUCCESS;
}