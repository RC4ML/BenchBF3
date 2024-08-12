#include "channel_bench.h"
#include "hs_clock.h"

DOCA_LOG_REGISTER(CHANNEL_BENCH);

void handle_rtt_sender(channel_config *config, DOCA::bench_runner *runner, DOCA::bench_stat *stat, DOCA::comm_channel_ep *ep,
    DOCA::comm_channel_addr *peer_addr) {

    DOCA::rt_assert(config->batch_size <= MAX_QUEUE_SIZE, "batch size too large");
    char msg[MAX_MSG_SIZE];
    size_t msg_size;

    size_t now_msg_index = 0;
    std::vector<DOCA::Timer> timers;
    std::vector<long> time_vec;
    for (size_t i = 0; i < config->batch_size; i++) {
        timers.emplace_back();
        time_vec.push_back(0);
        timers[i].tic();
        ep->ep_sendto(msg, config->payload, peer_addr);
    }
    while (runner->running()) {
        try {
            msg_size = MAX_MSG_SIZE;
            ep->ep_recvfrom(msg, &msg_size, peer_addr);
            time_vec[now_msg_index % config->batch_size] += timers[now_msg_index % config->batch_size].toc();
            DOCA::rt_assert(msg_size == RTT_RESPONSE_SIZE, "[sender] Invalid message size");
            stat->finish_batch_op(1);
            timers[now_msg_index % config->batch_size].tic();
            ep->ep_sendto(msg, config->payload, peer_addr);

            now_msg_index++;
        }
        catch (const DOCA::DOCAException &e) {
            DOCA_LOG_ERR("Failed to send message: {%s}", e.what());
            break;
        }
    }
    DOCA_LOG_INFO("STOP!");
    long total_lat = 0;
    for (size_t i = 0; i < config->batch_size; i++) {
        total_lat += time_vec[i];
    }
    DOCA_LOG_INFO("latency %.3f", static_cast<double>(total_lat) / now_msg_index);
}

void handle_rtt_receiver(channel_config *config, DOCA::bench_runner *runner, DOCA::bench_stat *stat,
    DOCA::comm_channel_ep *ep,
    DOCA::comm_channel_addr *peer_addr) {
    _unused(stat);

    char msg[MAX_MSG_SIZE];
    size_t msg_size;
    while (runner->running()) {
        try {
            msg_size = MAX_MSG_SIZE;
            ep->ep_recvfrom(msg, &msg_size, peer_addr);
            DOCA::rt_assert(msg_size == config->payload, "[receiver] Invalid message size");
            ep->ep_sendto(msg, RTT_RESPONSE_SIZE, peer_addr);
            stat->finish_batch_op(1);
        }
        catch (const DOCA::DOCAException &e) {
            DOCA_LOG_ERR("Failed to send message: {%s}", e.what());
            break;
        }
    }
    DOCA_LOG_INFO("STOP!");
}

// create a new sync event and export to the peer
DOCA::sync_event *
handle_sync_event_export(DOCA::sync_event *se, DOCA::comm_channel_ep *ep,
    DOCA::comm_channel_addr *peer_addr) {
    std::string export_se = se->export_sync_event();
    ep->ep_sendto(export_se.c_str(), export_se.size(), peer_addr);
    char msg[MAX_MSG_SIZE];
    size_t msg_len = MAX_MSG_SIZE;
    ep->ep_recvfrom(msg, &msg_len, peer_addr);
    DOCA::rt_assert(std::string(msg, msg_len) == resp_success, "recv sync event response failed");

    return se;
}

DOCA::sync_event *
handle_sync_event_import(DOCA::dev *dev, DOCA::comm_channel_ep *ep, DOCA::comm_channel_addr *peer_addr) {
    char msg[MAX_MSG_SIZE];
    size_t msg_len = MAX_MSG_SIZE;
    ep->ep_recvfrom(msg, &msg_len, peer_addr);
    ep->ep_sendto(resp_success.c_str(), resp_success.size(), peer_addr);

    std::string str = std::string(msg, msg_len);
    return DOCA::sync_event::new_from_exported_dpu(str, dev);
}

void send_dma_reqs(DOCA::bench_runner *runner, DOCA::bench_stat *stat, channel_config *config,
    sync_event_config *se_config, DOCA::comm_channel_ep *ep,
    DOCA::comm_channel_addr *peer_addr) {
    size_t local_buf_len = config->batch_size * config->payload;
    size_t remote_buf_len = config->mmap_length;

    if (config->is_read) {
        std::swap(local_buf_len, remote_buf_len);
    }
    std::random_device seed;
    std::mt19937_64 engine(seed());
    std::uniform_int_distribution<> distrib(0, INT32_MAX);

    dma_descriptor descriptor{};
    descriptor.local_payload = config->payload;
    descriptor.remote_payload = config->payload;

    size_t start = 0;

    for (size_t i = 0; i < config->batch_size; i++) {
        size_t rand_offset = distrib(engine);
        rand_offset = DOCA::round_up(rand_offset, CACHE_LINE_SZ) % (config->mmap_length - config->payload);
        if (config->is_read) {
            descriptor.local_offset = rand_offset;
            descriptor.remote_offset = start;
        } else {
            descriptor.local_offset = start;
            descriptor.remote_offset = rand_offset;
        }

        ep->ep_sendto(&descriptor, sizeof(descriptor), peer_addr);
        stat->start_one_op();

        start += config->payload;
    }
    start = 0;
    size_t wait_index = 0;
    size_t send_num = config->batch_size;
    size_t inflight_num = config->batch_size;
    DOCA::sync_event_job_get se_get_job(se_config->se);
    se_get_job.set_fetched(&wait_index);
    char tmp_msg[MAX_MSG_SIZE];
    size_t tmp_msg_len = MAX_MSG_SIZE;

    try {
        while (runner->running()) {
            se_config->sync_event_job_submit_sync(&se_get_job);
            size_t need_to_send = wait_index + config->batch_size - send_num;
            stat->finish_batch_op(need_to_send);
            inflight_num -= need_to_send;

            for (size_t i = 0; i < need_to_send; i++) {
                size_t rand_offset = distrib(engine);
                rand_offset = DOCA::round_up(rand_offset, CACHE_LINE_SZ) % (config->mmap_length - config->payload);
                if (config->is_read) {
                    descriptor.local_offset = rand_offset;
                    descriptor.remote_offset = start;
                } else {
                    descriptor.local_offset = start;
                    descriptor.remote_offset = rand_offset;
                }
                if (unlikely(ep->ep_sendto(&descriptor, sizeof(descriptor), peer_addr) == false)) {
                    goto end_send;
                }
                stat->start_one_op();
                inflight_num++;
                send_num++;

                start += config->payload;
                if (unlikely(send_num % config->batch_size == 0)) {
                    start = 0;
                }
            }
            //            DOCA_LOG_INFO("index %ld", wait_index);
        }
    }
    catch (const DOCA::DOCAException &e) {
        DOCA_LOG_INFO("%s", e.what());
    }

    // don't forget to record the time
    stat->finish_batch_op(inflight_num);
    std::cout
        << DOCA::string_format("latency %.4f\n",
            double(stat->timer.get_now_timepoint()) / (1000 * stat->num_ops_finished));

end_send:

    // the end
    ep->ep_sendto(resp_success.c_str(), resp_success.size(), peer_addr);

    tmp_msg_len = MAX_MSG_SIZE;
    ep->ep_recvfrom(tmp_msg, &tmp_msg_len, peer_addr);
    DOCA::rt_assert(std::string(tmp_msg, tmp_msg_len) == resp_success, "recv sync event response failed");
}

void handle_dma_export(channel_config *config, sync_event_config *se_config, DOCA::bench_runner *runner,
    DOCA::bench_stat *stat, DOCA::dev *dev,
    DOCA::comm_channel_ep *ep,
    DOCA::comm_channel_addr *peer_addr) {
    _unused(stat);

    char *src_buffer = reinterpret_cast<char *>(malloc(config->mmap_length));

    auto *src_mmap = new DOCA::mmap(true);
    src_mmap->add_device(dev);
    src_mmap->set_permissions(DOCA_ACCESS_FLAG_PCI_READ_WRITE);
    src_mmap->set_memrange_and_start(src_buffer, config->mmap_length);

    doca_conn_info conn_info{};
    conn_info.exports = src_mmap->export_dpus();
    conn_info.buffers.emplace_back(std::bit_cast<uint64_t>(src_buffer), config->mmap_length);

    std::string str = doca_conn_info::to_protobuf(conn_info).SerializeAsString();
    ep->ep_sendto(str.c_str(), str.size(), peer_addr);
    char msg[MAX_MSG_SIZE];
    size_t msg_len = MAX_MSG_SIZE;
    ep->ep_recvfrom(msg, &msg_len, peer_addr);
    DOCA::rt_assert(std::string(msg, msg_len) == resp_success, "recv failed");

    send_dma_reqs(runner, stat, config, se_config, ep, peer_addr);

    free(src_buffer);
    delete src_mmap;
}

doca_conn_info recv_config(DOCA::comm_channel_ep *ep, DOCA::comm_channel_addr *peer_addr) {
    char msg[MAX_MSG_SIZE];
    size_t msg_len = MAX_MSG_SIZE;
    ep->ep_recvfrom(msg, &msg_len, peer_addr);
    dma_connect::doca_conn_info info;
    DOCA::rt_assert(info.ParseFromArray(msg, static_cast<int>(msg_len)), "parse failed");

    ep->ep_sendto(resp_success.c_str(), resp_success.size(), peer_addr);

    return doca_conn_info::from_protobuf(info);
}

void receive_dma_reqs(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat, channel_config *config,
    sync_event_config *se_config,
    DOCA::workq *workq, DOCA::dma *dma, DOCA::buf *local_doca_buf, DOCA::buf *remote_doca_buf,
    DOCA::comm_channel_ep *ep,
    DOCA::comm_channel_addr *peer_addr) {
    _unused(thread_id);
    _unused(runner);
    _unused(stat);

    size_t local_buf_len = local_doca_buf->raw_size;
    size_t remote_buf_len = remote_doca_buf->raw_size;

    if (config->is_read) {
        std::swap(local_buf_len, remote_buf_len);
        std::swap(local_doca_buf, remote_doca_buf);
    }
    DOCA::dma_job_memcpy dma_job(dma);

    dma_job.set_src(local_doca_buf);
    dma_job.set_dst(remote_doca_buf);

    char msg[MAX_MSG_SIZE];
    size_t msg_len = MAX_MSG_SIZE;
    for (size_t i = 0; i < config->batch_size; i++) {
        msg_len = MAX_MSG_SIZE;
        ep->ep_recvfrom(msg, &msg_len, peer_addr);
        DOCA::rt_assert(msg_len == sizeof(dma_descriptor), "dma descriptor recv failed");
        auto *descriptor = reinterpret_cast<dma_descriptor *>(msg);
        dma_job.get_src()->set_data(descriptor->local_offset, descriptor->local_payload);

        // dma_job.get_dst()->set_data(descriptor->remote_offset, descriptor->remote_payload);
        // doca 2.2.0 udpate dst_buf_data_len need to be zero
        dma_job.get_dst()->set_data(descriptor->remote_offset, 0);
        workq->submit(&dma_job);
    }

    doca_error_t result;
    ::doca_event event{};

    // DOCA::sync_event_job_update_set se_set_job(se_config->se);
    // se_set_job.set_value(0);

    size_t se_add_fetched = 0;
    DOCA::sync_event_job_update_add se_add_job(se_config->se);
    se_add_job.set_value_and_fetched(1, &se_add_fetched);

    size_t wait_index = 0;

    size_t se_workq_num = 0;

    size_t inflight_num = config->batch_size;
    while (true) {

        if ((result = workq->poll_completion(&event)) == DOCA_SUCCESS) {
            inflight_num--;
            wait_index++;
            // se_add_job.set_value(wait_index);
            result = se_config->sync_event_job_submit_async(&se_add_job);
            if (result != DOCA_SUCCESS) {
                throw DOCA::DOCAException(result, __FILE__, __LINE__);
            }
            se_workq_num++;
        } else if (likely(result == DOCA_ERROR_AGAIN)) {
            // do nothing
        } else {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }

        //        DOCA_LOG_INFO("index %ld", wait_index);

        msg_len = MAX_MSG_SIZE;
        result = ep->ep_recvfrom_async(msg, &msg_len, peer_addr);

        if (unlikely(result == DOCA_SUCCESS)) {
            if (unlikely(msg_len != sizeof(dma_descriptor))) {
                if (std::string(msg, msg_len) == resp_success) {
                    DOCA_LOG_INFO("recv success, exit");
                    break;
                } else {
                    DOCA_LOG_ERR("recv failed, exit");
                    throw DOCA::DOCAException(result, __FILE__, __LINE__);
                }
            }

            auto *descriptor = reinterpret_cast<dma_descriptor *>(msg);
            dma_job.get_src()->set_data(descriptor->local_offset, descriptor->local_payload);
            // dma_job.get_dst()->set_data(descriptor->remote_offset, descriptor->remote_payload);
            // doca 2.2.0 udpate dst_buf_data_len need to be zero
            dma_job.get_dst()->set_data(descriptor->remote_offset, 0);
            workq->submit(&dma_job);
            inflight_num++;

            while ((result = se_config->se_workq->poll_completion(&event)) != DOCA_SUCCESS) {
            }

            DOCA::rt_assert(result == DOCA_SUCCESS, "poll_completion assert error");
            se_workq_num--;
        } else if (likely(result == DOCA_ERROR_AGAIN)) {
            // do nothing
        } else {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
    }
    while (inflight_num) {
        while ((result = workq->poll_completion(&event)) ==
            DOCA_ERROR_AGAIN) {
            /* Wait for the job to complete */
        }
        if (unlikely(result != DOCA_SUCCESS)) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }

        inflight_num--;
    }
    while (se_workq_num) {
        while ((result = se_config->se_workq->poll_completion(&event)) ==
            DOCA_ERROR_AGAIN) {
            /* Wait for the job to complete */
        }
        if (unlikely(result != DOCA_SUCCESS)) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
        se_workq_num--;
    }
    ep->ep_sendto(resp_success.c_str(), resp_success.size(), peer_addr);
}

void handle_dma_import(size_t thread_id, channel_config *config, sync_event_config *se_config, DOCA::bench_runner *runner,
    DOCA::bench_stat *stat,
    DOCA::dev *dev,
    DOCA::comm_channel_ep *ep,
    DOCA::comm_channel_addr *peer_addr) {

    doca_conn_info info = recv_config(ep, peer_addr);
    std::string thread_export = info.exports[thread_id % info.exports.size()];
    std::pair<uint64_t, size_t> thread_buffer = info.buffers[thread_id % info.buffers.size()];

    char *local_buffer = reinterpret_cast<char *>(malloc(config->batch_size * config->payload));
    auto *local_mmap = new DOCA::mmap(true);
    local_mmap->add_device(dev);
    local_mmap->set_memrange_and_start(local_buffer, config->batch_size * config->payload);

    auto *remote_mmap = DOCA::mmap::new_from_exported_dpu(thread_export, dev);

    auto *dma = new DOCA::dma();
    auto *ctx = new DOCA::ctx(dma, std::vector<DOCA::dev *>{dev});

    auto *workq = new DOCA::workq(MAX_QUEUE_SIZE, dma);
    auto *inv = new DOCA::buf_inventory(1024);

    auto *remote_doca_buf = DOCA::buf::buf_inventory_buf_by_addr(inv, remote_mmap,
        reinterpret_cast<void *>(thread_buffer.first),
        thread_buffer.second);
    auto *local_doca_buf = DOCA::buf::buf_inventory_buf_by_addr(inv, local_mmap, reinterpret_cast<void *>(local_buffer),
        config->batch_size * config->payload);

    receive_dma_reqs(thread_id, runner, stat, config, se_config, workq, dma, local_doca_buf, remote_doca_buf, ep,
        peer_addr);

    delete local_doca_buf;
    delete remote_doca_buf;
    delete workq;
    delete inv;
    delete ctx;
    delete local_mmap;
    delete remote_mmap;
    delete dma;
    free(local_buffer);
}
