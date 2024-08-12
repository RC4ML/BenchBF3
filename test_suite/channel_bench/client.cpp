#include "channel_bench.h"
#include "bench.h"
#include "simple_reporter.h"

DOCA_LOG_REGISTER(CLIENT)

void ping_pong(DOCA::comm_channel_ep *ep, DOCA::comm_channel_addr *peer_addr) {
    std::string msg = "ping";
    ep->ep_sendto(msg.c_str(), msg.size(), peer_addr);
    DOCA_LOG_INFO("send %s", msg.c_str());

    char buf[MAX_MSG_SIZE];
    size_t msg_len = MAX_MSG_SIZE;
    ep->ep_recvfrom(buf, &msg_len, peer_addr);
    DOCA_LOG_INFO("recv %s", buf);

    if (std::string(buf) != "pong") {
        DOCA_LOG_ERR("ping pong failed");
        exit(1);
    }
}

void perform_client_routine(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat, void *args) {
    _unused(runner);
    _unused(stat);

    auto *config = static_cast<channel_config *>(args);

    DOCA::dev *dev = DOCA::open_doca_device_with_pci(config->pci_address.c_str(), nullptr);

    auto *ep = new DOCA::comm_channel_ep(dev, nullptr, server_name + std::to_string(thread_id));

    std::thread ep_stop_thread = std::thread([&]() {
        size_t tmp = TIME_OUT;
        while (tmp--) {
            sleep(1);
        }
        ep->stop_recv_and_send();
    });

    std::vector<uint16_t> property = ep->get_property();
    DOCA_LOG_INFO("[GET] max_msg_size %u, send_queue %u, recv_queue %u", property[0], property[1], property[2]);

    property[0] = MAX_MSG_SIZE;
    property[1] = MAX_QUEUE_SIZE;
    property[2] = MAX_QUEUE_SIZE;

    ep->set_property(property);
    DOCA_LOG_INFO("[SET] max_msg_size %u, send_queue %u, recv_queue %u", property[0], property[1], property[2]);

    DOCA::comm_channel_addr *peer_addr = ep->ep_connect();
    ping_pong(ep, peer_addr);
#if defined(RTT)
    if (config->is_sender) {
        handle_rtt_sender(config, runner, stat, ep, peer_addr);
    } else {
        handle_rtt_receiver(config, runner, stat, ep, peer_addr);
    }
#elif defined(DMA)
    auto *se_config = new sync_event_config();
    se_config->se = new DOCA::sync_event(config->is_sender, dev);
    se_config->se_ctx = new DOCA::ctx(se_config->se, {});
    se_config->se_workq = new DOCA::workq(config->batch_size, se_config->se);
// warning:: must create se_ctx and se_workq before calling handle_sync_event_export
    handle_sync_event_export(se_config->se, ep, peer_addr);

    if (config->is_sender) {
        handle_dma_export(config, se_config, runner, stat, dev, ep, peer_addr);
    } else {
        handle_dma_import(thread_id, config, se_config, runner, stat, dev, ep, peer_addr);
    }
    delete se_config;
#elif defined(SYNC)
    if (config->is_sender) {
        handle_sync_sender(config, runner, stat, dev, ep, peer_addr);
    } else {
        handle_sync_receiver(config, runner, stat, dev, ep, peer_addr);
    }
#endif
    ep_stop_thread.join();
    delete peer_addr;
    delete ep;
    delete dev;
}

void bootstrap_client(channel_config config) {
    auto runner = DOCA::bench_runner(config.thread_count);

    runner.run(perform_client_routine, static_cast<void *>(&config));

    auto reporter = DOCA::simple_reporter();

    for (size_t i = 0; i < config.life_time; i++) {
        sleep(1);
        std::cout << runner.report(&reporter).to_string_simple(config.payload) << std::endl;
    }

    runner.stop();
}