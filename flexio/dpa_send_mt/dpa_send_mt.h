#include "wrapper_flexio.hpp"
#include "common/dpa_send_common.h"
#include "gflags_common.h"

extern "C"
{
    flexio_func_t dpa_send_mt_device_event_handler;
    flexio_func_t dpa_send_mt_device_init;
    flexio_func_t dpa_send_mt_deivce_first_packet;
    flexio_func_t dpa_send_mt_stop;
    extern struct flexio_app *dpa_send_mt_device;
}

#define TARGET_MAC 0x020101010101

DECLARE_string(device_name);

DECLARE_uint64(begin_thread);

class dpa_send_mt_config {
public:
    FLEX::CQ *rq_cq;
    FLEX::CQ *sq_cq;
    FLEX::SQ *sq;
    FLEX::RQ *rq;

    FLEX::dr_flow_rule *rx_flow_rule;

    FLEX::dr_flow_rule *tx_flow_root_rule;
    FLEX::dr_flow_rule *tx_flow_rule;
};
