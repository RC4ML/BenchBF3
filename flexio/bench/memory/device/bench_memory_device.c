#include "wrapper_flexio/wrapper_flexio_device.h"
#include "wrapper_flexio/common_cross.h"
#include "../bench_memory_cross.h"

__FLEXIO_ENTRY_POINT_START

void bench_mem_read(size_t *data, long num_ele){
	long loop_num = 0;
	long sum[EXPAND_SIZE];
	size_t start = __dpa_thread_cycles();
	while(loop_num<LOOPS){
		for(long i=0;i<num_ele;i+=STRIDE*EXPAND_SIZE){
			#if EXPAND_SIZE == 1
				sum[0] += data[i + 0 * STRIDE];
			#elif EXPAND_SIZE == 4
				sum[0] += data[i + 0 * STRIDE];
				sum[1] += data[i + 1 * STRIDE];
				sum[2] += data[i + 2 * STRIDE];
				sum[3] += data[i + 3 * STRIDE];
			#elif EXPAND_SIZE == 8
				sum[0] += data[i + 0 * STRIDE];
				sum[1] += data[i + 1 * STRIDE];
				sum[2] += data[i + 2 * STRIDE];
				sum[3] += data[i + 3 * STRIDE];
				sum[4] += data[i + 4 * STRIDE];
				sum[5] += data[i + 5 * STRIDE];
				sum[6] += data[i + 6 * STRIDE];
				sum[7] += data[i + 7 * STRIDE];
			#endif
		}
		loop_num++;
	}
	size_t end = __dpa_thread_cycles();
	size_t total_cycles = (end - start);
    size_t nano_seconds =  total_cycles * 10 / 18; // 1.8GHz
	for(int i=0;i<EXPAND_SIZE;i++){
		LOG_I("%ld\n", sum[i]);
	}
	size_t accessed_words = 1UL*LOOPS*num_ele/STRIDE;
	size_t time_ms = nano_seconds/1000/1000;
	size_t words_per_second = accessed_words*1000/time_ms;
	LOG_I("Stride: %d Bytes\n",STRIDE*8);
	LOG_I("Accsessed words(8B): %ld\n", accessed_words);
	LOG_I("Duration: %ld secs\n", time_ms/1000);
	LOG_I("Words per second: %ld K/s\n", words_per_second/1024);
}

__dpa_rpc__ uint64_t bench_func(uint64_t arg_daddr){
	struct MemoryArg *arg = (struct MemoryArg *)arg_daddr;
	int stream_id = arg->stream_id;
	LOG_F(stream_id, "Bench func, with stream id:%d\n", stream_id);

	uint64_t *host_buffer = (uint64_t *)get_host_buffer(arg->window_id, arg->mkey, arg->haddr);

	uint64_t* data = arg->data;
	for (int i = 0; i < 1; i++){
		LOG_I("Arm mem:%ld\n", host_buffer[i]);
	}
	for (int i = 0; i < 1; i++){
		LOG_I("Private mem:%ld\n", data[i]);
	}
	long num_ele = THREAD_MEM_SIZE/sizeof(size_t);
	
	#if MEMORY_TYPE == COPY_FROM_HOST
		LOG_I("Use private memory, copy_from_host\n");
		bench_mem_read(data, num_ele);	
	#elif MEMORY_TYPE ==  POSIX_MEM_ALIGN
		LOG_I("Use ARM memory posix_mem_align\n");
		bench_mem_read(host_buffer, num_ele);
	#endif
	return 0;
}

__FLEXIO_ENTRY_POINT_END
