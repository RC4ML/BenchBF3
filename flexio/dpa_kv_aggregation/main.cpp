#include "dpa_kv_aggregation.h"

DOCA_LOG_REGISTER(DPA_KV_AGGREGATION);

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

bool rq_buffer_on_host = false;
bool kv_on_host = false;

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
    global_ctx->create_process(dpa_kv_aggregation_device);
    global_ctx->create_window();
    global_ctx->generate_flexio_uar();
    global_ctx->print_init("dpa_kv_aggregation", 0);

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

    flexio_uintptr_t global_address;
    uint32_t global_address_mkey;
    if (!kv_on_host) {
        Assert(flexio_buf_dev_alloc(global_ctx->get_process(), sizeof(uint32_t) * entry_size,
            &global_address) == FLEXIO_STATUS_SUCCESS);
        global_address_mkey = 0;
    } else {
        void *tmp_addr = global_ctx->alloc_huge((sizeof(uint32_t) * entry_size + 63) & (~63));
        Assert(tmp_addr);
        auto *global_address_mr = global_ctx->reg_mr(tmp_addr, (sizeof(uint32_t) * entry_size + 63) & (~63), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC);
        global_address = reinterpret_cast<flexio_uintptr_t>(tmp_addr);
        global_address_mkey = global_address_mr->lkey;
    }
    std::vector<dpa_kv_aggregation_config> configs;
    for (size_t i = 0; i < FLAGS_g_thread_num; i++) {
        dpa_kv_aggregation_config config{};
        global_ctx->create_event_handler(dpa_kv_aggregation_device_event_handler);
        config.rq_cq = new FLEX::CQ(true, LOG_CQ_RING_DEPTH, global_ctx, i);
        config.sq_cq = new FLEX::CQ(false, LOG_CQ_RING_DEPTH, global_ctx, i);

        config.rq = new FLEX::RQ(LOG_RQ_RING_DEPTH, LOG_WQ_DATA_ENTRY_BSIZE, config.rq_cq->get_cq_num(), global_ctx, rq_buffer_on_host);

        //borrow this flag !!!
        config.sq = new FLEX::SQ(LOG_SQ_RING_DEPTH, LOG_WQ_DATA_ENTRY_BSIZE, config.sq_cq->get_cq_num(), global_ctx, kv_on_host);
        app_transfer_wq sq_transf = config.sq->get_sq_transf();
        sq_transf.wqd_mkey_id = global_address_mkey;
        queue_config_data dev_data{ config.rq_cq->get_cq_transf(), config.rq->get_rq_transf(),
                                   config.sq_cq->get_cq_transf(),
                                   sq_transf, static_cast<uint32_t>(i + FLAGS_begin_thread), global_address, global_ctx->get_window_id() };
        flexio_uintptr_t dev_config_data = global_ctx->move_to_dev(dev_data);
        uint64_t rpc_ret_val;

        Assert(flexio_process_call(global_ctx->get_process(), &dpa_kv_aggregation_device_init, &rpc_ret_val,
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

    DOCA_LOG_INFO("MT DPA KV AGGREGATION Started");
    /* Add an additional new line for output readability */
    DOCA_LOG_INFO("Press Ctrl+C to terminate");
    uint64_t dpa_exec_time = 0;
    while (!force_quit) {
        sleep(1);
        Assert(flexio_process_call(global_ctx->get_process(), &dpa_kv_aggregation_device_stop_time, &dpa_exec_time,
            0) == FLEXIO_STATUS_SUCCESS);
        if (dpa_exec_time != 0) {
            printf("Mops: %.2lf\n", STOP_NUMBER * MAX_ELEMENT_NUM * FLAGS_g_thread_num * 1800.0 / (dpa_exec_time * 1.0));
            break;
        }
    }

    // don't free at now
    return EXIT_SUCCESS;
}