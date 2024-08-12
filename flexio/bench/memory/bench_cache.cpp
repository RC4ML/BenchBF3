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
#include "bench_cache_cross.h"

extern "C"
{
    flexio_func_t bench_func;
    flexio_func_t average_cache_bench;
    flexio_func_t single_cache_bench;
}

extern struct flexio_app *bench_cache_device;

DOCA_LOG_REGISTER(MAIN);

int NUM_OF_THREADS = 1;
int WORKER_BATCH_SIZE = 500;

void host_func() {

    flexio_status ret;
    int return_value;

    FLEX::Context ctx("mlx5_bond_0");
    ctx.alloc_pd();

    ctx.create_process(bench_cache_device);
    ctx.generate_flexio_uar();
    ctx.print_init("compute", NUM_OF_THREADS);

    ctx.create_window();

    void *host_buffer;
    return_value = posix_memalign(&host_buffer, 64, memsize);
    Assert(return_value == 0);
    ibv_mr *mr = ctx.reg_mr(host_buffer, memsize);

    for (size_t i = 0; i < memsize / (sizeof(size_t)); i++) {
        ((uint64_t *)host_buffer)[i] = i + 1000;
    }

    FLEX::CommandQ cmdq(ctx.get_process(), NUM_OF_THREADS, WORKER_BATCH_SIZE);

    // alloc memory
    struct element *data;
    void *sizes;
    void *times, *bws;
    size_t *cycles;

    size_t aid_size = timeslots * sizeof(long);

    Assert(sizes = malloc(aid_size));
    Assert(times = malloc(aid_size));
    Assert(bws = malloc(aid_size));
    Assert(data = (struct element *)malloc(memsize));
    Assert(cycles = (size_t *)malloc(LOOP_TOTAL * sizeof(size_t)));

    memset(sizes, 0x00, aid_size);
    memset(times, 0x00, aid_size);
    memset(bws, 0x00, aid_size);
    memset(data, 0x00, memsize);
    memset(cycles, 0x00, LOOP_TOTAL * sizeof(size_t));

    LOG_I("memsize: %ld\n", memsize);
    LOG_I("timeslots: %ld\n", timeslots);

    flexio_uintptr_t sizes_daddr;
    ret = flexio_copy_from_host(ctx.get_process(), sizes, aid_size, &sizes_daddr);
    Assert(ret == FLEXIO_STATUS_SUCCESS);

    flexio_uintptr_t times_daddr;
    ret = flexio_copy_from_host(ctx.get_process(), times, aid_size, &times_daddr);
    Assert(ret == FLEXIO_STATUS_SUCCESS);

    flexio_uintptr_t bws_daddr;
    ret = flexio_copy_from_host(ctx.get_process(), bws, aid_size, &bws_daddr);
    Assert(ret == FLEXIO_STATUS_SUCCESS);

    flexio_uintptr_t data_daddr;
    ret = flexio_copy_from_host(ctx.get_process(), data, memsize, &data_daddr);
    Assert(ret == FLEXIO_STATUS_SUCCESS);

    flexio_uintptr_t cycles_daddr;
    ret = flexio_copy_from_host(ctx.get_process(), cycles, LOOP_TOTAL * sizeof(size_t), &cycles_daddr);
    Assert(ret == FLEXIO_STATUS_SUCCESS);

    MemoryArg arg;
    arg.stream_id = 1;
    arg.window_id = ctx.get_window_id();
    arg.mkey = mr->lkey;
    arg.haddr = host_buffer;

    arg.sizes = sizes_daddr;
    arg.times = times_daddr;
    arg.bws = bws_daddr;
    arg.data = data_daddr;
    arg.cycles = cycles_daddr;

    for (int i = 0; i < timeslots; i++) {
        arg.index = i;
        flexio_uintptr_t arg_daddr = ctx.move_to_dev(arg);
        cmdq.add_task(average_cache_bench, arg_daddr);
    }
    LOG_I("Threads: %d\n", NUM_OF_THREADS);
    LOG_I("Add jobs done!\n");
    LOG_I("Use memory:%s\n", MemTypeStr[MEMORY_TYPE]);

    cmdq.run();

    auto duration = cmdq.wait_run(300); // 100 seconds
    LOG_I("Finished, total time: %ld us\n", duration);
    sleep(1); // wait log
}

int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    struct doca_log_backend *stdout_logger = nullptr;
    Assert(doca_log_backend_create_with_file(stdout, &stdout_logger) == DOCA_SUCCESS);
    host_func();
    return EXIT_SUCCESS;
}