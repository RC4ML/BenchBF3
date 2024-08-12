#include "wrapper_flexio_device.h"

int get_double(int a) {
	return a * TEST_DOUBLE;
}



__dpa_rpc__ uint64_t timer_benchmark(int times) {
	LOG_I("Timer benchmark\n");
	uint64_t t0, t1;
	uint64_t total_cycles = 0;
	int overflow_times = 0;
	for (int i = 0;i < times;i++) {
		t0 = __dpa_thread_cycles();
		sleep_(1);
		t1 = __dpa_thread_cycles();
		if (t0 < t1) {
			total_cycles += (t1 - t0);
		} else {
			total_cycles += ((1UL << 32) - 1 + t1 - t0);
			overflow_times += 1;
		}
	}
	LOG_I("Total times: %d\n", times);
	LOG_I("Overflow times: %d\n", overflow_times);
	LOG_I("Timer benchmark: %lu cycles\n", total_cycles);
	return total_cycles;
}

flexio_uintptr_t get_host_buffer(uint32_t window_id, uint32_t mkey, void *haddr) {
	struct flexio_dev_thread_ctx *dtctx;
	flexio_uintptr_t host_buffer;
	flexio_dev_status_t ret;
	Assert(flexio_dev_get_thread_ctx(&dtctx) >= 0);

	ret = flexio_dev_window_config(dtctx, (uint16_t)window_id, mkey);
	Assert(ret == FLEXIO_DEV_STATUS_SUCCESS);

	ret = flexio_dev_window_ptr_acquire(dtctx, (uint64_t)haddr, &host_buffer);
	Assert(ret == FLEXIO_DEV_STATUS_SUCCESS);
	return host_buffer;
}

flexio_uintptr_t get_host_buffer_with_dtctx(struct flexio_dev_thread_ctx *dtctx, uint32_t window_id, uint32_t mkey, void *haddr) {
	(void)haddr;

	flexio_uintptr_t host_buffer = 0;
	flexio_dev_status_t ret;

	ret = flexio_dev_window_config(dtctx, (uint16_t)window_id, mkey);
	Assert(ret == FLEXIO_DEV_STATUS_SUCCESS);

	ret = flexio_dev_window_ptr_acquire(dtctx, (uint64_t)haddr, &host_buffer);
	Assert(ret == FLEXIO_DEV_STATUS_SUCCESS);
	return host_buffer;
}

