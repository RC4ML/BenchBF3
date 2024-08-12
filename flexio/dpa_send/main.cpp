#include "dpa_send.h"

DOCA_LOG_REGISTER(dpa_send);

static bool force_quit;

static void
signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        /* Add additional new lines for output readability */
        DOCA_LOG_INFO(" ");
        DOCA_LOG_INFO("Signal %d received, preparing to exit", signum);
        DOCA_LOG_INFO(" ");
        force_quit = true;
    }
}

bool rq_buffer_on_host = true;
bool sq_buffer_on_host = true;
// 正常情况下不应该这么使用，这么使用会导致收到其他的包覆盖了要发送的包，或者存在其他影响
// 但是这里我们设置了奇怪的源MAC地址，所以不会收到其他包，因此才可以使用tx_shared_rx_buffer
bool tx_shared_rx_buffer = true;

// 是否使用nic mode，使用nic mode时候config.tx_flow_root_rule->create_dr_rule这行代码会segment fault，原因未知，暂时注释掉 不影响测试
bool nic_mode = false;

int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    dpa_send_config config;
    config.device_name = FLAGS_device_name;
    config.ctx = new FLEX::Context(config.device_name);
    Assert(config.ctx);
    // useless at DOCA2.2.1
    // config.ctx->alloc_devx_uar(0x1);
    config.ctx->alloc_pd();

    config.ctx->create_process(dpa_send_device);
    config.ctx->create_window();

    config.ctx->generate_flexio_uar();
    config.ctx->print_init("dpa_send", 0);
    config.ctx->create_event_handler(dpa_send_device_event_handler);

    config.rq_cq = new FLEX::CQ(true, LOG_CQ_RING_DEPTH, config.ctx);
    config.sq_cq = new FLEX::CQ(false, LOG_CQ_RING_DEPTH, config.ctx);

    config.rq = new FLEX::RQ(LOG_RQ_RING_DEPTH, LOG_WQ_DATA_ENTRY_BSIZE, config.rq_cq->get_cq_num(), config.ctx, rq_buffer_on_host);
    // use same tx/rx buffer
    if (tx_shared_rx_buffer) {
        config.sq = new FLEX::SQ(LOG_SQ_RING_DEPTH, LOG_WQ_DATA_ENTRY_BSIZE, config.sq_cq->get_cq_num(), config.ctx, config.rq);
    } else {
        config.sq = new FLEX::SQ(LOG_SQ_RING_DEPTH, LOG_WQ_DATA_ENTRY_BSIZE, config.sq_cq->get_cq_num(), config.ctx, sq_buffer_on_host);
    }

    queue_config_data dev_data{ config.rq_cq->get_cq_transf(), config.rq->get_rq_transf(),
                               config.sq_cq->get_cq_transf(),
                               config.sq->get_sq_transf(), 0,0,  config.ctx->get_window_id() };

    flexio_uintptr_t dev_config_data = config.ctx->move_to_dev(dev_data);

    uint64_t rpc_ret_val;

    Assert(flexio_process_call(config.ctx->get_process(), &dpa_send_device_init, &rpc_ret_val, dev_config_data) ==
        FLEXIO_STATUS_SUCCESS);

    config.rx_dr = new FLEX::DR(config.ctx, MLX5DV_DR_DOMAIN_TYPE_NIC_RX);
    FLEX::flow_matcher matcher{};
    matcher.set_dst_mac_mask();
    config.rx_flow_table = config.rx_dr->create_flow_table(0, 0, &matcher);
    config.rx_flow_rule = new FLEX::dr_flow_rule();
    config.rx_flow_rule->add_dest_devx_tir(config.rq->get_inner_ptr());
    matcher.set_dst_mac(addr_to_num(SRC_ADDR));
    config.rx_flow_rule->create_dr_rule(config.rx_flow_table, &matcher);

    matcher.clear();
    config.tx_dr = new FLEX::DR(config.ctx, MLX5DV_DR_DOMAIN_TYPE_FDB);
    matcher.set_src_mac_mask();
    config.tx_flow_root_table = config.tx_dr->create_flow_table(0, 0, &matcher);
    config.tx_flow_table = config.tx_dr->create_flow_table(1, 0, &matcher);
    config.tx_flow_rule = new FLEX::dr_flow_rule();
    config.tx_flow_root_rule = new FLEX::dr_flow_rule();
    config.tx_flow_root_rule->add_dest_table(config.tx_flow_table->dr_table);
    config.tx_flow_rule->add_dest_vport(config.tx_dr->get_inner_ptr(), 0xFFFF);

    matcher.set_src_mac(addr_to_num(SRC_ADDR));
    if (!nic_mode) {
        config.tx_flow_root_rule->create_dr_rule(config.tx_flow_root_table, &matcher);
    } else {
        (void)config.tx_flow_root_rule;
    }
    config.tx_flow_rule->create_dr_rule(config.tx_flow_table, &matcher);

    config.ctx->event_handler_run(0);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    DOCA_LOG_INFO("L2 reflector Started");
    /* Add an additional new line for output readability */
    DOCA_LOG_INFO("Press Ctrl+C to terminate");
    sleep(2);
    DOCA_LOG_INFO("Sending first packet");
    Assert(flexio_process_call(config.ctx->get_process(), &dpa_send_deivce_first_packet, &rpc_ret_val, 0) ==
        FLEXIO_STATUS_SUCCESS);

    while (!force_quit)
        sleep(1);

    // don't free at now
    return EXIT_SUCCESS;
}