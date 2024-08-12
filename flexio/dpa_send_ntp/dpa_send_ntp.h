#include "wrapper_flexio.hpp"
#include "common/dpa_send_ntp_common.h"
#include "gflags_common.h"

extern "C"
{
    flexio_func_t dpa_send_ntp_device_event_handler;
    flexio_func_t dpa_send_ntp_device_init;
    flexio_func_t dpa_send_ntp_deivce_first_packet;
    extern struct flexio_app *dpa_send_ntp_device;
}

DECLARE_string(device_name);

class dpa_send_ntp_config {
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
