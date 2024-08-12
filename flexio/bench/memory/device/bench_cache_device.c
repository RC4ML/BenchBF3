#include "wrapper_flexio/wrapper_flexio_device.h"
#include "wrapper_flexio/common_cross.h"
#include "../bench_cache_cross.h"

__FLEXIO_ENTRY_POINT_START

__dpa_rpc__ uint64_t bench_func(uint64_t arg_daddr) {
	uint64_t *host_buffer;

	struct MemoryArg *arg = (struct MemoryArg *)arg_daddr;
	int stream_id = arg->stream_id;
	LOG_F(stream_id, "Bench func, with stream id:%d\n", stream_id);

	host_buffer = (uint64_t *)get_host_buffer(arg->window_id, arg->mkey, arg->haddr);

	for (int i = 0; i < 8; i++) {
		LOG_I("%ld\n", host_buffer[i]);
	}
	return 0;
}

__dpa_rpc__ uint64_t average_cache_bench(uint64_t arg_daddr) {
	struct MemoryArg *arg = (struct MemoryArg *)arg_daddr;
	long *sizes = arg->sizes;
	long *bws = arg->bws;
	long *times = arg->times;
	long index = arg->index;
	initialize_sizes(sizes);
	long num_elem, k, tcount, tnanosec;
	long count, nanosec;
	uint64_t result = 0;
	num_elem = sizes[index] / (long)sizeof(struct element);
	tcount = 0, tnanosec = 0;
#if MEMORY_TYPE == COPY_FROM_HOST
	struct element *actual_buf = arg->data;
#elif MEMORY_TYPE ==  POSIX_MEM_ALIGN
	struct element *actual_buf = (struct element *)get_host_buffer(arg->window_id, arg->mkey, arg->haddr);
#endif
	LOG_F(1, "Actual buf:%ld\n", ((uint64_t *)actual_buf)[0]);
	LOG_F(1, "Magic\n"); //do not remove this line! Otherwise the latency when size==768B would be higer than expected.
	for (k = 0; k < repeat_count; k++) {
		init_pchase(actual_buf, num_elem);
		result += benchmark_cache_ronly_p(actual_buf, &nanosec, &count);
		tcount += count;
		tnanosec += nanosec;
	}
	compute_stat(index, times, bws, sizes, tcount, tnanosec);
	LOG_F(1, "%lx\n", result);
	return 0;
}

__dpa_rpc__ uint64_t single_cache_bench(uint64_t arg_daddr) {
	struct MemoryArg *arg = (struct MemoryArg *)arg_daddr;
	long *sizes = arg->sizes;
	struct element *data = arg->data;
	size_t *cycles = arg->cycles;
	long index = arg->index;
	initialize_sizes(sizes);

	long cur_size = sizes[index];
	long num_elem = cur_size / (long)sizeof(struct element);
	if (cur_size <= 1536) {
		LOG_F(1, "WorkingSetSize: %ldB, Stride: %ldB\n", cur_size, (PADDING_SIZE + 1) * 8);
	} else if (cur_size <= 1536 * 1024) {
		LOG_F(1, "WorkingSetSize: %ldKB, Stride: %ldB\n", cur_size / 1024, (PADDING_SIZE + 1) * 8);
	} else {
		LOG_F(1, "WorkingSetSize: %ldMB, Stride: %ldB\n", cur_size / 1024 / 1024, (PADDING_SIZE + 1) * 8);
	}
	init_pchase(data, num_elem);
	long loop_num = 0;
	struct element *p = data;
	while (loop_num < LOOP_TOTAL) {
		size_t t1 = __dpa_thread_cycles();
		p = p->next;
		size_t t2 = __dpa_thread_cycles();
		if (t1 < t2) {
			cycles[loop_num] += (t2 - t1);
		} else {
			cycles[loop_num] += ((1UL << 32) - 1 + t2 - t1);
		}
		loop_num++;
	}
	statistics(cycles);
	LOG_I("%p\n", (void *)p);
	return 0;
}
__FLEXIO_ENTRY_POINT_END
