#include "wrapper_flexio.hpp"
#include "common/dpa_refactor_random_access_common.h"
#include "gflags_common.h"

extern "C"
{
    flexio_func_t dpa_refactor_random_access_device_event_handler;
    flexio_func_t dpa_refactor_random_access_device_init;
    extern struct flexio_app *dpa_refactor_random_access_device;
}

// port bf_1 bond_0 mac
// #define SELF_MAC 0xf6721aedb72c
#define SELF_MAC 0x010101010101

DECLARE_string(device_name);

class dpa_refactor_random_access_config {
public:
    std::string device_name;
    FLEX::Context *ctx;

    FLEX::CQ *rq_cq;
    FLEX::CQ *sq_cq;
    FLEX::SQ *sq;
    FLEX::RQ *rq;

    FLEX::DR *rx_dr;
    FLEX::DR *tx_dr;

    FLEX::dr_flow_table *rx_flow_table;
    FLEX::dr_flow_rule *rx_flow_rule;

    FLEX::dr_flow_table *tx_flow_root_table;
    FLEX::dr_flow_rule *tx_flow_root_rule;
    FLEX::dr_flow_table *tx_flow_table;
    FLEX::dr_flow_rule *tx_flow_rule;
};
