#include "wrapper_flexio/wrapper_flexio_device.h"
#include "wrapper_flexio/common_cross.h"
#include "../dhrystone/dhrystone.h"
#include "../bench_cross.h"

__FLEXIO_ENTRY_POINT_START
__dpa_rpc__ uint64_t bench_func(uint64_t arg_daddr){
	struct ComputeArg* arg = (struct ComputeArg*)arg_daddr;
	int stream_id = arg->stream_id;
	LOG_F(stream_id, "Bench func, with stream id:%d\n", stream_id);
	dhrystone_bench(stream_id);
	// sleep_(1);
	return 0;
}
__FLEXIO_ENTRY_POINT_END
