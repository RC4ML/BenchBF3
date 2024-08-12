#include "dpa_refactor.h"

DOCA_LOG_REGISTER(DPA_REFACTOR);

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

    dpa_refactor_config config;
    config.device_name = FLAGS_device_name;
    config.ctx = new FLEX::Context(config.device_name);
    Assert(config.ctx);
    // useless at DOCA2.2.1
    // config.ctx->alloc_devx_uar(0x1);
    config.ctx->alloc_pd();

    config.ctx->create_process(dpa_refactor_device);
    config.ctx->generate_flexio_uar();
    config.ctx->print_init("dpa_refactor", 0);

    config.ctx->create_window();

    config.ctx->create_event_handler(dpa_refactor_device_event_handler);

    config.rq_cq = new FLEX::CQ(true, LOG_CQ_RING_DEPTH, config.ctx, 0);
    config.sq_cq = new FLEX::CQ(false, LOG_CQ_RING_DEPTH, config.ctx, 0);
    config.rq = new FLEX::RQ(LOG_RQ_RING_DEPTH, LOG_WQ_DATA_ENTRY_BSIZE, config.rq_cq->get_cq_num(), config.ctx, true);
    // use same tx/rx buffer
    config.sq = new FLEX::SQ(13, 12, config.sq_cq->get_cq_num(), config.ctx);

    flexio_uintptr_t new_dpa_buffer;
    Assert(flexio_buf_dev_alloc(config.ctx->get_process(), LOG2VALUE(LOG_RQ_RING_DEPTH + LOG_WQ_DATA_ENTRY_BSIZE),
        &new_dpa_buffer) == FLEXIO_STATUS_SUCCESS);

    flexio_mkey *tmp_key = config.ctx->create_dpa_mkey(new_dpa_buffer, LOG_RQ_RING_DEPTH + LOG_WQ_DATA_ENTRY_BSIZE, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);

    queue_config_data dev_data{ config.rq_cq->get_cq_transf(), config.rq->get_rq_transf(), config.sq_cq->get_cq_transf(),
                               config.sq->get_sq_transf(), 0, new_dpa_buffer, flexio_mkey_get_id(tmp_key) };

    flexio_uintptr_t dev_config_data = config.ctx->move_to_dev(dev_data);

    uint64_t rpc_ret_val;

    Assert(flexio_process_call(config.ctx->get_process(), &dpa_refactor_device_init, &rpc_ret_val, dev_config_data) ==
        FLEXIO_STATUS_SUCCESS);

    config.rx_dr = new FLEX::DR(config.ctx, MLX5DV_DR_DOMAIN_TYPE_NIC_RX);
    FLEX::flow_matcher matcher{};
    matcher.set_dst_mac_mask();
    config.rx_flow_table = config.rx_dr->create_flow_table(0, 0, &matcher);
    config.rx_flow_rule = new FLEX::dr_flow_rule();
    config.rx_flow_rule->add_dest_devx_tir(config.rq->get_inner_ptr());
    matcher.set_dst_mac(TARGET_MAC);
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

    matcher.set_src_mac(TARGET_MAC);

    config.tx_flow_root_rule->create_dr_rule(config.tx_flow_root_table, &matcher);

    config.tx_flow_rule->create_dr_rule(config.tx_flow_table, &matcher);

    config.ctx->event_handler_run(0);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    DOCA_LOG_INFO("L2 reflector Started");
    /* Add an additional new line for output readability */
    DOCA_LOG_INFO("Press Ctrl+C to terminate");

    struct element {
        struct element *next;
        size_t padding[1023];
    };
    size_t total_size = 16384l * 1024 * 1024;
    size_t num_elem = total_size / sizeof(element);
    void *tmp_addr = malloc(total_size);
    struct element *data = static_cast<element *>(tmp_addr);

    for (size_t i = 0; i < num_elem; i++) {
        data[i].next = &data[(i + 1) % num_elem];
        for (int j = 0; j < 1023; j++) {
            data[i].padding[j] = 0;
        }
    }
    size_t count = 0;
    struct timespec begin, end;

    long loop_total = 100000000;

    while (!force_quit) {
        ether_hdr *hdr = reinterpret_cast<ether_hdr *>(config.rq->get_rq_transf().wqd_daddr);
        if (addr_to_num(hdr->dst_addr) != 0) {

            char *begin_addr = reinterpret_cast<char *>(hdr);
            // for (size_t i = 0; i < 1; i++)
            // {
            //     char *packet = begin_addr + i * 1024;
            //     printf("%ld ", i);
            //     for (int i = 0; i < 14; i++)
            //     {
            //         printf("%02X ", static_cast<unsigned char>(packet[i]));
            //     }
            //     printf("\n");
            // }
            // sleep(1);
            // printf("\n\n");
            long loop_num = 0;
            loop_total = 2048;

            clock_gettime(CLOCK_MONOTONIC, &begin);

            while (loop_num < loop_total) {
                count += *begin_addr;
                begin_addr += LOG2VALUE(LOG_WQ_DATA_ENTRY_BSIZE) * 4;
                loop_num += 4;
            }
            clock_gettime(CLOCK_MONOTONIC, &end);

            printf("%.2lf %ld %p\n", ((end.tv_sec - begin.tv_sec) * 1e9 + end.tv_nsec - begin.tv_nsec) / 512, count, static_cast<void *>(begin_addr));
            sleep(2);
        } else {
            struct element *p = data;

            long loop_num = 0;
            clock_gettime(CLOCK_MONOTONIC, &begin);

            while (loop_num < loop_total) {
                p = p->next;
                loop_num++;
            }
            clock_gettime(CLOCK_MONOTONIC, &end);

            count = loop_num;
            printf("%.2lf %ld %p\n", ((end.tv_sec - begin.tv_sec) * 1e9 + end.tv_nsec - begin.tv_nsec) / count, count, static_cast<void *>(p));
            sleep(5);
        }
    }
    // sleep(1);

    // don't free at now
    return EXIT_SUCCESS;
}