#include "dpa_small_bank_client.h"

DOCA_LOG_REGISTER(DPA_SMALLBANK_CLIENT);

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

int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    std::string device_name = FLAGS_device_name;
    auto *global_ctx = new FLEX::Context(device_name);
    Assert(global_ctx);
    // useless at DOCA2.2.1
    // global_ctx->alloc_devx_uar(0x1);
    global_ctx->alloc_pd();
    global_ctx->create_process(dpa_small_bank_client_device);
    global_ctx->create_window();
    global_ctx->generate_flexio_uar();
    global_ctx->print_init("dpa_small_bank_client", 0);


    auto *global_rx_dr = new FLEX::DR(global_ctx, MLX5DV_DR_DOMAIN_TYPE_NIC_RX);
    auto *global_tx_dr = new FLEX::DR(global_ctx, MLX5DV_DR_DOMAIN_TYPE_FDB);
    FLEX::flow_matcher matcher{};

    matcher.set_dst_mac_mask();
    FLEX::dr_flow_table *global_rx_flow_table = global_rx_dr->create_flow_table(0, 0, &matcher);
    matcher.clear();
    matcher.set_src_mac_mask();
    FLEX::dr_flow_table *global_tx_flow_root_table = global_tx_dr->create_flow_table(0, 0, &matcher);
    FLEX::dr_flow_table *global_tx_flow_table = global_tx_dr->create_flow_table(1, 0, &matcher);
    matcher.clear();

    std::vector<dpa_small_bank_client_config> configs;
    uint64_t rpc_ret_val;
    for (size_t i = 0; i < FLAGS_g_thread_num; i++) {
        dpa_small_bank_client_config config{};
        global_ctx->create_event_handler(dpa_small_bank_client_device_event_handler);
        config.rq_cq = new FLEX::CQ(true, LOG_CQ_RING_DEPTH, global_ctx, i);
        config.sq_cq = new FLEX::CQ(false, LOG_CQ_RING_DEPTH, global_ctx, i);
        config.rq = new FLEX::RQ(LOG_RQ_RING_DEPTH, LOG_WQ_DATA_ENTRY_BSIZE, config.rq_cq->get_cq_num(), global_ctx, false);
        config.sq = new FLEX::SQ(LOG_SQ_RING_DEPTH, LOG_WQ_DATA_ENTRY_BSIZE, config.sq_cq->get_cq_num(), global_ctx, false);

        queue_config_data dev_data{ config.rq_cq->get_cq_transf(), config.rq->get_rq_transf(),
                                   config.sq_cq->get_cq_transf(),
                                   config.sq->get_sq_transf(), static_cast<uint32_t>(i),0,  global_ctx->get_window_id() };
        flexio_uintptr_t dev_config_data = global_ctx->move_to_dev(dev_data);

        Assert(flexio_process_call(global_ctx->get_process(), &dpa_small_bank_client_device_init, &rpc_ret_val,
            dev_config_data) == FLEXIO_STATUS_SUCCESS);
        config.rx_flow_rule = new FLEX::dr_flow_rule();
        config.rx_flow_rule->add_dest_devx_tir(config.rq->get_inner_ptr());
        matcher.set_dst_mac(TARGET_MAC + i);
        config.rx_flow_rule->create_dr_rule(global_rx_flow_table, &matcher);
        matcher.clear();
        config.tx_flow_rule = new FLEX::dr_flow_rule();
        config.tx_flow_root_rule = new FLEX::dr_flow_rule();
        config.tx_flow_root_rule->add_dest_table(global_tx_flow_table->dr_table);
        config.tx_flow_rule->add_dest_vport(global_tx_dr->get_inner_ptr(), 0xFFFF);
        matcher.set_src_mac(TARGET_MAC + i);
        config.tx_flow_root_rule->create_dr_rule(global_tx_flow_root_table, &matcher);

        config.tx_flow_rule->create_dr_rule(global_tx_flow_table, &matcher);
        matcher.clear();
        global_ctx->event_handler_run(i, i);
        configs.push_back(config);

    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    LOG_I("Small Bank Client Started\n");
    /* Add an additional new line for output readability */
    LOG_I("Press Ctrl+C to terminate\n");
    sleep(1);
    LOG_I("Prepare ping packet\n");
    Assert(flexio_process_call(global_ctx->get_process(), &dpa_small_bank_client_device_ping_packet, &rpc_ret_val, 0) ==
        FLEXIO_STATUS_SUCCESS);
    while (!force_quit)
        sleep(1);

    Assert(flexio_process_call(global_ctx->get_process(), &dpa_small_bank_client_device_stop, &rpc_ret_val, 0) ==
        FLEXIO_STATUS_SUCCESS);
    LOG_I("Stop RPC CALL Finish\n");

    // don't free at now
    return EXIT_SUCCESS;
}