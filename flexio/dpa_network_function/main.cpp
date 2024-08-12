#include "dpa_network_function.h"

DOCA_LOG_REGISTER(DPA_NETWORK_FUNCTION);

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
bool tx_shared_rx_buffer = true;

// 是否使用nic mode，使用nic mode时候config.tx_flow_root_rule->create_dr_rule这行代码会segment fault，原因未知，暂时注释掉 不影响测试
bool nic_mode = false;

int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    std::string device_name = FLAGS_device_name;
    auto *global_ctx = new FLEX::Context(device_name);
    Assert(global_ctx);
    // useless at DOCA2.2.1
    // global_ctx->alloc_devx_uar(0x1);
    global_ctx->alloc_pd();
    global_ctx->create_process(dpa_network_function_device);
    global_ctx->create_window();
    global_ctx->generate_flexio_uar();
    global_ctx->print_init("dpa_network_function", 0);

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

    std::vector<dpa_network_function_config> configs;
    for (size_t i = 0; i < FLAGS_g_thread_num; i++) {
        dpa_network_function_config config{};
        global_ctx->create_event_handler(dpa_network_function_device_event_handler);
        config.rq_cq = new FLEX::CQ(true, LOG_CQ_RING_DEPTH, global_ctx, i);
        config.sq_cq = new FLEX::CQ(false, LOG_CQ_RING_DEPTH, global_ctx, i);

        config.rq = new FLEX::RQ(LOG_RQ_RING_DEPTH, LOG_WQ_DATA_ENTRY_BSIZE, config.rq_cq->get_cq_num(), global_ctx, rq_buffer_on_host);

        if (tx_shared_rx_buffer) {
            config.sq = new FLEX::SQ(LOG_SQ_RING_DEPTH, LOG_WQ_DATA_ENTRY_BSIZE, config.sq_cq->get_cq_num(), global_ctx, config.rq);
        } else {
            config.sq = new FLEX::SQ(LOG_SQ_RING_DEPTH, LOG_WQ_DATA_ENTRY_BSIZE, config.sq_cq->get_cq_num(), global_ctx);
        }

        queue_config_data dev_data{ config.rq_cq->get_cq_transf(), config.rq->get_rq_transf(),
                                   config.sq_cq->get_cq_transf(),
                                   config.sq->get_sq_transf(), static_cast<uint32_t>(i + FLAGS_begin_thread), 0, global_ctx->get_window_id() };
        flexio_uintptr_t dev_config_data = global_ctx->move_to_dev(dev_data);
        uint64_t rpc_ret_val;

        Assert(flexio_process_call(global_ctx->get_process(), &dpa_network_function_device_init, &rpc_ret_val,
            dev_config_data) == FLEXIO_STATUS_SUCCESS);
        config.rx_flow_rule = new FLEX::dr_flow_rule();
        config.rx_flow_rule->add_dest_devx_tir(config.rq->get_inner_ptr());
        matcher.set_dst_mac(TARGET_MAC + i + FLAGS_begin_thread);
        config.rx_flow_rule->create_dr_rule(global_rx_flow_table, &matcher);
        matcher.clear();

        config.tx_flow_rule = new FLEX::dr_flow_rule();
        config.tx_flow_root_rule = new FLEX::dr_flow_rule();
        config.tx_flow_root_rule->add_dest_table(global_tx_flow_table->dr_table);
        config.tx_flow_rule->add_dest_vport(global_tx_dr->get_inner_ptr(), 0xFFFF);
        matcher.set_src_mac(TARGET_MAC + i + FLAGS_begin_thread);
        if (!nic_mode) {
            config.tx_flow_root_rule->create_dr_rule(global_tx_flow_root_table, &matcher);
        } else {
            (void)global_tx_flow_root_table;
        }
        config.tx_flow_rule->create_dr_rule(global_tx_flow_table, &matcher);
        matcher.clear();

        global_ctx->event_handler_run(i + FLAGS_begin_thread, i);
        configs.push_back(config);
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    DOCA_LOG_INFO("MT DPA NETWORK FUNCTION Started");
    /* Add an additional new line for output readability */
    DOCA_LOG_INFO("Press Ctrl+C to terminate");
    while (!force_quit)
        sleep(1);

    // don't free at now
    return EXIT_SUCCESS;
}