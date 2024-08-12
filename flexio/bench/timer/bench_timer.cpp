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

extern "C" {
	flexio_func_t timer_benchmark;
}

extern struct flexio_app *bench_timer_device;

DOCA_LOG_REGISTER(MAIN);

DEFINE_int32(times, 1, "Total times of calling sleep_(1) function");

int main(int argc, char **argv) {
	gflags::ParseCommandLineFlags(&argc, &argv, true);
	struct doca_log_backend *stdout_logger = nullptr;
	Assert(doca_log_backend_create_with_file(stdout, &stdout_logger) == DOCA_SUCCESS);

	flexio_status  ret;
	uint64_t rpc_call_result;
	int total_times = FLAGS_times;

	FLEX::Context ctx("mlx5_0");
	ctx.alloc_pd();
	ctx.create_process(bench_timer_device);
	ctx.generate_flexio_uar();
	ctx.print_init("", 0);
	ret = flexio_process_call(ctx.get_process(), &timer_benchmark, &rpc_call_result, total_times);
	Assert(ret == FLEXIO_STATUS_SUCCESS);
	LOG_I("Total cycles: %lu\n", rpc_call_result);

	return EXIT_SUCCESS;
}