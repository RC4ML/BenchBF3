#include "dpa_network_function.h"

DEFINE_string(device_name, "", "device name for select ib device");

DEFINE_uint64(begin_thread, 0, "begin thread number, which means use DPA core [begin_thread, begin_thread + g_thread_num]");