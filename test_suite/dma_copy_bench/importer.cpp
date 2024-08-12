#include "dma_copy_bench.h"
#include "bench.h"
#include "simple_reporter.h"
#include "udp_server.h"

DOCA_LOG_REGISTER(IMPORTER);

doca_conn_info recv_config(std::string ip_and_port) {
    DOCA::UDPServer<dma_connect::doca_conn_info> server(std::move(ip_and_port));
    dma_connect::doca_conn_info info;
    server.recv_blocking(info);
    return doca_conn_info::from_protobuf(info);
}

class task_config {
public:
    DOCA::bench_stat *stat;
    class dma_config *dma_config;
    DOCA::pe *pe;
    DOCA::dma *dma;
    DOCA::mmap *local_mmap;
    DOCA::mmap *remote_mmap;
    char *local_mmap_addr;
    char *remote_mmap_addr;
    DOCA::buf_inventory *inv;
    size_t now_index;
    size_t finish_index;

    // used for storage of dma tasks
    std::vector<DOCA::dma_task_memcpy *> task_buffers;
    std::vector<DOCA::buf *>task_src_buf;
    std::vector<DOCA::buf *>task_dst_buf;
};

void post_dma_reqs(uint64_t thread_id, DOCA::bench_runner *runner, task_config *task_config) {
    _unused(thread_id);

    static std::random_device seed;
    static std::mt19937_64 engine(seed());
    static std::uniform_int_distribution<> distrib(0, INT32_MAX);

    uint32_t batch_size = task_config->dma_config->batch_size;
    size_t mod_num = task_config->dma_config->mmap_length - task_config->dma_config->payload;
    (void)mod_num;

    task_config->task_buffers.resize(batch_size);
    task_config->task_src_buf.resize(batch_size);
    task_config->task_dst_buf.resize(batch_size);


    auto complete_callback = [](struct doca_dma_task_memcpy *useless_task, union doca_data task_user_data,
        union doca_data ctx_user_data) -> void {
            _unused(useless_task);

            auto *task_config = static_cast<class task_config *>(ctx_user_data.ptr);
            size_t task_index = task_user_data.u64;
            task_config->stat->finish_one_op_without_time();
            if (stop_flag) {
                return;
            }
            task_config->task_dst_buf[task_index]->set_data(task_index * task_config->dma_config->payload, 0ul);
            task_config->task_src_buf[task_index]->set_data(task_index * task_config->dma_config->payload, task_config->dma_config->payload);
            task_config->pe->submit(task_config->task_buffers[task_index]);
        };
    auto error_callback = [](struct doca_dma_task_memcpy *useless_task, union doca_data task_user_data,
        union doca_data ctx_user_data) -> void {
            _unused(useless_task);

            auto *task_config = static_cast<class task_config *>(ctx_user_data.ptr);
            size_t task_index = task_user_data.u64;

            throw DOCA::DOCAException(doca_task_get_status(task_config->task_buffers[task_index]->to_base()), __FILE__, __LINE__);
        };
    task_config->dma->set_task_conf(batch_size, complete_callback, error_callback);
    // start ctx here
    task_config->pe->start_ctx();

    DOCA::Timer now_timer;
    now_timer.tic();

    for (size_t index = 0;index < batch_size;index++) {
        size_t remote_offset = index * task_config->dma_config->payload;
        size_t local_offset = index * task_config->dma_config->payload;
        if (task_config->dma_config->is_read) {
            task_config->task_src_buf[index] = DOCA::buf::buf_inventory_buf_by_data(task_config->inv, task_config->remote_mmap, task_config->remote_mmap_addr, task_config->dma_config->payload * batch_size);
            task_config->task_src_buf[index]->set_data(remote_offset, task_config->dma_config->payload);
            task_config->task_dst_buf[index] = DOCA::buf::buf_inventory_buf_by_addr(task_config->inv, task_config->local_mmap, task_config->local_mmap_addr, task_config->dma_config->payload * batch_size);
            task_config->task_dst_buf[index]->set_data(local_offset, 0);
        } else {
            task_config->task_src_buf[index] = DOCA::buf::buf_inventory_buf_by_data(task_config->inv, task_config->local_mmap, task_config->local_mmap_addr, task_config->dma_config->payload * batch_size);
            task_config->task_src_buf[index]->set_data(local_offset, task_config->dma_config->payload);
            task_config->task_dst_buf[index] = DOCA::buf::buf_inventory_buf_by_addr(task_config->inv, task_config->remote_mmap, task_config->remote_mmap_addr, task_config->dma_config->payload * batch_size);
            task_config->task_dst_buf[index]->set_data(remote_offset, 0);
        }
        doca_data index_data = { .u64 = index };
        task_config->task_buffers[index] = new DOCA::dma_task_memcpy(task_config->dma,
            task_config->task_src_buf[index], task_config->task_dst_buf[index], index_data);
        if (task_config->pe->submit(task_config->task_buffers[index]) != DOCA_SUCCESS) {
            printf("error %ld %ld %ld\n", index, task_config->now_index, task_config->finish_index);
            exit(1);
        };
    }
    while (runner->running()) {
        task_config->pe->poll_progress();
    }
    // dry the pe
    while (task_config->pe->get_num_inflight() > 0) {
        task_config->pe->poll_progress();
        usleep(1);
    }

    for (size_t index = 0;index < batch_size;index++) {
        delete task_config->task_buffers[index];
        delete task_config->task_src_buf[index];
        delete task_config->task_dst_buf[index];
    }

    printf("Thread %ld: %lf Mops\n", thread_id, task_config->stat->num_ops_finished * 1.0 / now_timer.toc());
}

void perform_importer_routine(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat,
    void *args) {
    sleep(2);
    auto *config = static_cast<dma_config *>(args);

    char *local_buffer = reinterpret_cast<char *>(malloc(config->batch_size * config->payload));


    auto *local_mmap = new DOCA::mmap(true);
    local_mmap->add_device(config->dev);
    local_mmap->set_memrange_and_start(local_buffer, config->batch_size * config->payload);

    auto *dma = new DOCA::dma(config->dev);
    auto *ctx = new DOCA::ctx(dma);

    auto *pe = new DOCA::pe();
    // don't forget to start ctx with pe->start_ctx
    pe->connect_ctx(ctx);

    auto *remote_mmap = DOCA::mmap::new_from_exported_dpu(config->remote_mmap_export_string, config->dev);

    auto *inv = new DOCA::buf_inventory(config->batch_size * 2);

    task_config task_config = { stat, config, pe, dma, local_mmap, remote_mmap, local_buffer, reinterpret_cast<char *>(config->remote_mmap_addr), inv, 0, 0,{}, {}, {} };
    ctx->set_data({ &task_config });

    post_dma_reqs(thread_id, runner, &task_config);

    delete inv;
    delete ctx;
    delete dma;
    delete pe;
    delete local_mmap;
    delete remote_mmap;

    free(local_buffer);
}

void bootstrap_importer(dma_config &config) {
    doca_conn_info info = recv_config(config.network_addrss);
    config.remote_mmap_export_string = info.exports[0];
    std::pair<uint64_t, size_t> addr_len_buffer = info.buffers[0];
    printf("thread main, check export size %ld, remote addr %ld len %ld\n", config.remote_mmap_export_string.size(),
        addr_len_buffer.first, addr_len_buffer.second);

    config.remote_mmap_addr = reinterpret_cast<void *>(addr_len_buffer.first);
    config.remote_mmap_size = addr_len_buffer.second;

    config.dev = DOCA::open_doca_device_with_pci(config.pci_address.c_str(), DOCA::dma_jobs_is_supported);

    auto runner = DOCA::bench_runner(config.thread_count, config.bind_offset, config.bind_numa);

    runner.run(perform_importer_routine, &config);

    auto reporter = DOCA::simple_reporter();

    for (size_t i = 0; i < config.life_time && stop_flag == false; i++) {
        sleep(1);
        std::cout << runner.report(&reporter).to_string_simple(config.payload) << std::endl;
    }
    stop_flag = true;
    runner.stop();

    delete config.dev;
}