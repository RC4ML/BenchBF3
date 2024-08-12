#include "wrapper_flexio/wrapper_flexio_device.h"
#include "wrapper_flexio/common_cross.h"
#include "../mt_memory_cross.h"

__FLEXIO_ENTRY_POINT_START

uint64_t dpa_mem_result[256];
uint64_t host_mem_result[256];

uint64_t bench_mem_read(size_t *data, long num_ele, int stream_id, int stride, size_t loop) {
	size_t loop_num = 0;
	long sum[EXPAND_SIZE];
	size_t start = __dpa_thread_cycles();
	while (loop_num < loop) {
		for (long i = 0; i < num_ele; i += stride * EXPAND_SIZE) {
#if EXPAND_SIZE == 1
			sum[0] += data[i + 0 * stride];
#elif EXPAND_SIZE == 4
			sum[0] += data[i + 0 * stride];
			sum[1] += data[i + 1 * stride];
			sum[2] += data[i + 2 * stride];
			sum[3] += data[i + 3 * stride];
#elif EXPAND_SIZE == 8
			sum[0] += data[i + 0 * stride];
			sum[1] += data[i + 1 * stride];
			sum[2] += data[i + 2 * stride];
			sum[3] += data[i + 3 * stride];
			sum[4] += data[i + 4 * stride];
			sum[5] += data[i + 5 * stride];
			sum[6] += data[i + 6 * stride];
			sum[7] += data[i + 7 * stride];
#endif
		}
		loop_num++;
	}
	size_t end = __dpa_thread_cycles();
	size_t total_cycles = (end - start);
	for (int i = 0; i < EXPAND_SIZE; i++) {
		flexio_dev_msg(stream_id, FLEXIO_MSG_DEV_NO_PRINT, "%ld\n", sum[i]);
	}
	return loop * num_ele * sizeof(size_t) * 1800000000UL / stride / total_cycles;
}

uint64_t bench_mem_write(size_t *data, long num_ele, int stream_id, int stride, size_t loop) {
	size_t loop_num = 0;
	(void)stream_id;
	size_t start = __dpa_thread_cycles();
	while (loop_num < loop) {
		for (long i = 0; i < num_ele; i += stride * EXPAND_SIZE) {
#if EXPAND_SIZE == 1
			data[i + 0 * stride] = loop_num;
#elif EXPAND_SIZE == 4
			data[i + 0 * stride] = i;
			data[i + 1 * stride] = i;
			data[i + 2 * stride] = i;
			data[i + 3 * stride] = i;
#elif EXPAND_SIZE == 8
			data[i + 0 * stride] = i;
			data[i + 1 * stride] = i;
			data[i + 2 * stride] = i;
			data[i + 3 * stride] = i;
			data[i + 4 * stride] = i;
			data[i + 5 * stride] = i;
			data[i + 6 * stride] = i;
			data[i + 7 * stride] = i;
#endif
		}
		loop_num++;
	}
	size_t end = __dpa_thread_cycles();
	size_t total_cycles = (end - start);

	return loop * num_ele * sizeof(size_t) * 1800000000UL / stride / total_cycles;
}

uint64_t bench_mem_memcpy(size_t *data, long num_ele, int stream_id, int stride, size_t loop) {
	size_t loop_num = 0;
	(void)stream_id;
	num_ele /= 2;
	size_t *dst = data + num_ele;
	data[0] = 0;
	size_t start = __dpa_thread_cycles();
	while (loop_num < loop) {
		for (long i = 0; i < num_ele; i += stride * EXPAND_SIZE) {
			dst[i] = data[i];
		}
		// memcpy(dst + data[0], data, num_ele * sizeof(size_t));
		loop_num++;
	}
	size_t end = __dpa_thread_cycles();
	size_t total_cycles = (end - start);
	return (loop * num_ele) * sizeof(size_t) * 1800000000UL / stride / total_cycles;
}


__dpa_rpc__ uint64_t bench_func(uint64_t arg_daddr) {
	struct MemoryArg *arg = (struct MemoryArg *)arg_daddr;
	int stream_id = arg->stream_id;
	LOG_F(stream_id, "Bench func, with stream id:%d\n", stream_id);

	uint64_t *data = NULL;
	if (arg->mem_type == HOST_MEM) {
		data = (uint64_t *)get_host_buffer(arg->window_id, arg->mkey, arg->haddr);
		LOG_F(stream_id, "Use host memory\n");
		for (int i = 0; i < 2; i++) {
			LOG_F(stream_id, "Host mem:%ld\n", data[i]);
		}
	} else if (arg->mem_type == PRIVATE_MEM) {
		data = arg->ddata;
		LOG_F(stream_id, "Use private memory\n");
		for (int i = 0; i < 2; i++) {
			LOG_F(stream_id, "Private mem:%ld\n", data[i]);
		}
	}

	long num_ele = arg->size / sizeof(size_t);
	if (arg->mem_type == HOST_MEM) {
		host_mem_result[stream_id] = bench_mem_write(data, num_ele, stream_id, arg->stride, arg->loop);
	} else {
		dpa_mem_result[stream_id] = bench_mem_write(data, num_ele, stream_id, arg->stride, arg->loop);
	}
	// bench_mem_write(data, num_ele, stream_id, arg->stride, arg->loop);
	// bench_mem_memcpy(data, num_ele, stream_id, arg->stride, arg->loop);

	return 0;
}

__dpa_rpc__ uint64_t get_dpa_mem_result(uint64_t total_threads) {
	uint64_t result = 0;
	for (size_t i = 0;i < total_threads;i++) {
		result += dpa_mem_result[i];
	}
	return result;
}

__dpa_rpc__ uint64_t get_host_mem_result(uint64_t total_threads) {
	uint64_t result = 0;
	for (size_t i = 0;i < total_threads;i++) {
		result += host_mem_result[i];
	}
	return result;
}

__FLEXIO_ENTRY_POINT_END
