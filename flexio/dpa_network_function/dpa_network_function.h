#include "wrapper_flexio.hpp"
#include "common/dpa_network_function_common.h"
#include "gflags_common.h"

extern "C"
{
    flexio_func_t dpa_network_function_device_event_handler;
    flexio_func_t dpa_network_function_device_init;
    extern struct flexio_app *dpa_network_function_device;
}

#define TARGET_MAC 0xa088c2320440
// #define TARGET_MAC 0x010101010101

DECLARE_string(device_name);

DECLARE_uint64(begin_thread);

class dpa_network_function_config {
public:
    FLEX::CQ *rq_cq;
    FLEX::CQ *sq_cq;
    FLEX::SQ *sq;
    FLEX::RQ *rq;

    FLEX::dr_flow_rule *rx_flow_rule;

    FLEX::dr_flow_rule *tx_flow_root_rule;
    FLEX::dr_flow_rule *tx_flow_rule;
};
