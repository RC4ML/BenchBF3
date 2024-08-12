#include "dma_copy_bench.h"

DEFINE_uint32(bind_offset, 0, "bind to offset core and thread ID");

DEFINE_uint32(bind_numa, 0, "bind to numa node");

bool stop_flag = false;

void ctrl_c_handler(int) { stop_flag = true; }