#pragma once

#include "gflags_common.h"
#include "wrapper_flexio.hpp"
#include "common/dpa_small_bank_common.h"

DEFINE_uint64(concurrency, 0, "Concurrency for each request, 1 means sync methods, >1 means async methods");

DEFINE_string(device_name, "", "device name for select ib device");

extern "C"
{
    flexio_func_t dpa_small_bank_client_device_init;
    flexio_func_t dpa_small_bank_client_device_ping_packet;
    flexio_func_t dpa_small_bank_client_device_stop;
    flexio_func_t dpa_small_bank_client_device_event_handler;
    extern struct flexio_app *dpa_small_bank_client_device;
}

#define TARGET_MAC 0x020101010101

class dpa_small_bank_client_config {
public:
    FLEX::CQ *rq_cq;
    FLEX::CQ *sq_cq;
    FLEX::SQ *sq;
    FLEX::RQ *rq;

    FLEX::dr_flow_rule *rx_flow_rule;

    FLEX::dr_flow_rule *tx_flow_root_rule;
    FLEX::dr_flow_rule *tx_flow_rule;
};
