#include "wrapper_flexio.hpp"
#include "common/dpa_refactor_common.h"
#include "gflags_common.h"

extern "C"
{
    flexio_func_t dpa_refactor_mt_device_event_handler;
    flexio_func_t dpa_refactor_mt_device_init;
    extern struct flexio_app *dpa_refactor_mt_device;
}

// port bf_1 bond_0 mac
// #define TARGET_MAC 0xa088c2320440

// port host_1 ens21f0np0 mac
// #define TARGET_MAC 0xa088c2320430

// test for dst mac
#define TARGET_MAC 0x010101010101

DECLARE_string(device_name);

DECLARE_uint64(begin_thread);

class dpa_refactor_mt_config {
public:
    FLEX::CQ *rq_cq;
    FLEX::CQ *sq_cq;
    FLEX::SQ *sq;
    FLEX::RQ *rq;

    FLEX::dr_flow_rule *rx_flow_rule;

    FLEX::dr_flow_rule *tx_flow_root_rule;
    FLEX::dr_flow_rule *tx_flow_rule;
};
