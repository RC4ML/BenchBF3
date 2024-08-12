#include <stdlib.h>
#include <gflags/gflags.h>

#include <doca_log.h>
#include <infiniband/verbs.h>
#include <infiniband/mlx5_api.h>
#include <libflexio/flexio.h>

#include "wrapper_flexio/wrapper_flexio.hpp"
#include "wrapper_flexio/common_cross.h"
#include "wrapper_flexio/common_host.hpp"

#include "common.hpp"
#include "bench_memory_cross.h"

extern "C"
{
    flexio_func_t bench_func;
}

extern struct flexio_app *bench_memory_device;

DOCA_LOG_REGISTER(MAIN);

int NUM_OF_THREADS = 1;
int WORKER_BATCH_SIZE = 1;

flexio_uintptr_t get_data_buffer(FLEX::Context *ctx, size_t size) {
    flexio_status ret;
    void *data;
    Assert(data = malloc(size));

    for (size_t i = 0; i < size / (sizeof(size_t)); i++) {
        ((uint64_t *)data)[i] = i + 10;
    }

    flexio_uintptr_t data_daddr;
    ret = flexio_copy_from_host(ctx->get_process(), data, size, &data_daddr);
    Assert(ret == FLEXIO_STATUS_SUCCESS);

    return data_daddr;
}

void host_func() {
    int return_value;
    FLEX::Context ctx("mlx5_0");
    ctx.alloc_pd();

    ctx.create_process(bench_memory_device);
    ctx.generate_flexio_uar();
    ctx.print_init("compute", NUM_OF_THREADS);

    ctx.create_window();
    void *host_buffer;
    return_value = posix_memalign(&host_buffer, 64, THREAD_MEM_SIZE);
    Assert(return_value == 0);
    ibv_mr *mr = ctx.reg_mr(host_buffer, THREAD_MEM_SIZE);

    for (size_t i = 0; i < THREAD_MEM_SIZE / (sizeof(size_t)); i++) {
        ((uint64_t *)host_buffer)[i] = i + 1000;
    }

    FLEX::CommandQ cmdq(ctx.get_process(), NUM_OF_THREADS, WORKER_BATCH_SIZE);

    MemoryArg arg;

    for (int i = 0; i < NUM_OF_THREADS; i++) {
        arg.stream_id = i + 1;
        arg.window_id = ctx.get_window_id();
        arg.mkey = mr->lkey;
        arg.haddr = host_buffer;
        arg.data = get_data_buffer(&ctx, THREAD_MEM_SIZE);
        flexio_uintptr_t arg_daddr = ctx.move_to_dev(arg);
        cmdq.add_task(bench_func, arg_daddr);
    }
    LOG_I("Threads: %d\n", NUM_OF_THREADS);
    LOG_I("Add jobs done!\n");

    cmdq.run();
    auto duration = cmdq.wait_run(300); // 100 seconds
    LOG_I("Finished, total time: %ld us\n", duration);
    ctx.flush();
    sleep(1); // wait log
}

int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    struct doca_log_backend *stdout_logger = nullptr;
    Assert(doca_log_backend_create_with_file(stdout, &stdout_logger) == DOCA_SUCCESS);
    host_func();
    return EXIT_SUCCESS;
}