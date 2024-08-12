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
#include "bench_cross.h"
#include "hs_clock.h"

extern "C" {
	flexio_func_t bench_func;
}

extern struct flexio_app *bench_compute_device;

DOCA_LOG_REGISTER(MAIN);

int NUM_OF_THREADS = 1;
int WORKER_BATCH_SIZE = 1;

doca_error_t host_func() {
	flexio_status  ret;
	// uint64_t rpc_call_result;

	FLEX::Context ctx("mlx5_0");
	ctx.alloc_pd();
	// ctx.query_hca_caps();

	ctx.create_process(bench_compute_device);
	ctx.generate_flexio_uar();
	ctx.print_init("compute", NUM_OF_THREADS);
	// ctx.create_outbox();

	struct flexio_cmdq_attr cmdq_fattr;
	struct flexio_cmdq *cmd_q = NULL;
	cmdq_fattr.workers = NUM_OF_THREADS;
	cmdq_fattr.batch_size = WORKER_BATCH_SIZE;
	cmdq_fattr.state = FLEXIO_CMDQ_STATE_PENDING;

	ret = flexio_cmdq_create(ctx.get_process(), &cmdq_fattr, &cmd_q);
	Assert(ret == FLEXIO_STATUS_SUCCESS);

	for (int i = 0;i < NUM_OF_THREADS;i++) {
		ComputeArg arg;
		arg.stream_id = i + 1;
		flexio_uintptr_t arg_daddr = ctx.move_to_dev(arg);
		ret = flexio_cmdq_task_add(cmd_q, bench_func, arg_daddr);
		Assert(ret == FLEXIO_STATUS_SUCCESS);
	}
	LOG_I("Threads: %d\n", NUM_OF_THREADS);
	LOG_I("Add jobs done!\n");

	DOCA::Timer timer;
	timer.tic();
	flexio_cmdq_state_running(cmd_q);

	int useconds = 0;
	int usecond_sleep = 1000;
#define TIMEOUT_IN_SECONDS 20
	while (!flexio_cmdq_is_empty(cmd_q)) {
		usleep(usecond_sleep);
		useconds += usecond_sleep;
		Assert(useconds < TIMEOUT_IN_SECONDS * 1e6);
	}
	auto duration = timer.toc();
	LOG_I("Finished, total time: %ld us\n", duration);

	double throughput = 1.0 * DPA_DHRYSTONE_NUMBER_OF_RUNS * NUM_OF_THREADS / (1.0 * duration / 1000000) / 1e6;//DMIPS

	LOG_I("Throughput: %.2f DMIPS\n", throughput);
	sleep(1);//wait log

	// ret = flexio_process_call(ctx.get_process(), &bench_func, &rpc_call_result, 0x1234);
	// Assert(ret == FLEXIO_STATUS_SUCCESS);
	// LOG_I("RPC call result: %#lx\n", rpc_call_result);
	return DOCA_SUCCESS;
}

DEFINE_int32(num_threads, 1, "Threads number");

int main(int argc, char **argv) {
	gflags::ParseCommandLineFlags(&argc, &argv, true);
	NUM_OF_THREADS = FLAGS_num_threads;
	struct doca_log_backend *stdout_logger = nullptr;
	Assert(doca_log_backend_create_with_file(stdout, &stdout_logger) == DOCA_SUCCESS);
	doca_error_t result = host_func();
	Assert(result == DOCA_SUCCESS);

	return EXIT_SUCCESS;
}