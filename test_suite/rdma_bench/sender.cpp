#include "rdma_bench.h"
#include "bench.h"
#include "simple_reporter.h"
#include "reporter.h"

DOCA_LOG_REGISTER(SENDER);


class rdma_task_send_config {
public:
    DOCA::mmap *local_mmap;
    DOCA::buf_inventory *inv;
    DOCA::rdma *rdma;
    DOCA::ctx *ctx;
    DOCA::pe *pe;

    std::vector<DOCA::rdma_task_send *> task_buffers;
    std::vector<DOCA::buf *>task_local_buf;

    size_t now_index{};
    size_t finish_index{};

    size_t batch_size;
    size_t payload;
    char *local_buffer;

    DOCA::bench_stat *stat;
    DOCA::bench_runner *runner;
};

void do_rdma_send(size_t thread_id, std::string network_addr, rdma_task_send_config &task_send_config) {
    size_t batch_size = task_send_config.batch_size;
    task_send_config.task_buffers.resize(batch_size);
    task_send_config.task_local_buf.resize(batch_size);

    auto complete_callback = [](struct doca_rdma_task_send *useless_task, union doca_data task_user_data,
        union doca_data ctx_user_data) -> void {
            _unused(useless_task);
            auto *worker_config = reinterpret_cast<rdma_task_send_config *>(ctx_user_data.ptr);
            worker_config->stat->finish_one_op();

            size_t task_index = task_user_data.u64;
            delete worker_config->task_local_buf[task_index];
            delete worker_config->task_buffers[task_index];
            worker_config->task_buffers[task_index] = nullptr;
            worker_config->finish_index++;
        };

    auto error_callback = [](struct doca_rdma_task_send *useless_task, union doca_data task_user_data,
        union doca_data ctx_user_data) -> void {
            _unused(useless_task);
            auto *worker_config = reinterpret_cast<rdma_task_send_config *>(ctx_user_data.ptr);

            size_t task_index = task_user_data.u64;
            printf("task %ld error %s\n", task_index, doca_error_get_descr(doca_task_get_status(worker_config->task_buffers[task_index]->to_base())));
            exit(1);
        };
    // tmp setup //
    doca_rdma_set_send_queue_size(task_send_config.rdma->get_inner_ptr(), 512);
    doca_rdma_set_recv_queue_size(task_send_config.rdma->get_inner_ptr(), 512);
    // tmp setup //

    task_send_config.rdma->set_task_send_conf(batch_size, complete_callback, error_callback);
    task_send_config.pe->start_ctx();

    std::string remote_rdma_param = receive_and_send_rdma_param(thread_id, network_addr, task_send_config.rdma->export_rdma());
    task_send_config.rdma->connect_rdma(remote_rdma_param);
    // move to running state
    task_send_config.pe->poll_progress();
    printf("connect success\n");
    sleep(2);

    while (task_send_config.runner->running()) {
        while (task_send_config.now_index - task_send_config.finish_index < batch_size) {
            size_t index = task_send_config.now_index % batch_size;
            if (task_send_config.task_buffers[index] != nullptr) {
                continue;
            }
            task_send_config.now_index++;
            task_send_config.task_local_buf[index] = DOCA::buf::buf_inventory_buf_by_data(task_send_config.inv
                , task_send_config.local_mmap, task_send_config.local_buffer + index * task_send_config.payload, task_send_config.payload);
            doca_data index_data = { .u64 = index };
            task_send_config.task_buffers[index] = new DOCA::rdma_task_send(task_send_config.rdma, task_send_config.task_local_buf[index], index_data);
            if (task_send_config.pe->submit(task_send_config.task_buffers[index]) != DOCA_SUCCESS) {
                printf("error %ld %ld %ld\n", index, task_send_config.now_index, task_send_config.finish_index);
                exit(1);
            }
        }
        task_send_config.pe->poll_progress();
    }

    while (task_send_config.now_index != task_send_config.finish_index) {
        task_send_config.pe->poll_progress();
    }
}

void perform_sender_routine(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat, void *args) {
    // wait for bind to numa node
    sleep(2);
    auto *config = static_cast<rdma_config *>(args);
    char *local_buffer = reinterpret_cast<char *>(malloc(config->batch_size * config->payload));

    rdma_task_send_config task_send_config;
    DOCA::dev *dev = config->dev;

    task_send_config.local_mmap = new DOCA::mmap(true);
    task_send_config.local_mmap->add_device(dev);
    task_send_config.local_mmap->set_memrange_and_start(local_buffer, config->batch_size * config->payload);

    task_send_config.rdma = new DOCA::rdma(dev, config->gid_index);

    task_send_config.ctx = new DOCA::ctx(task_send_config.rdma);
    task_send_config.pe = new DOCA::pe();
    // don't forget to start ctx with pe->start_ctx
    task_send_config.pe->connect_ctx(task_send_config.ctx);

    task_send_config.inv = new DOCA::buf_inventory(1024);

    task_send_config.batch_size = config->batch_size;
    task_send_config.payload = config->payload;
    task_send_config.local_buffer = local_buffer;
    task_send_config.runner = runner;
    task_send_config.stat = stat;

    task_send_config.ctx->set_data({ &task_send_config });
    do_rdma_send(thread_id, config->network_addrss, task_send_config);

    delete task_send_config.inv;
    delete task_send_config.ctx;
    delete task_send_config.rdma;
    delete task_send_config.pe;
    delete task_send_config.local_mmap;

    free(local_buffer);
}

void bootstrap_sender(rdma_config &config) {
    auto runner = DOCA::bench_runner(config.thread_count, config.bind_offset, config.bind_numa);
    config.dev = DOCA::open_doca_device_with_ibdev_name(config.ibdev_name.c_str(), DOCA::rdma_send_job_is_supported);
    runner.run(perform_sender_routine, static_cast<void *>(&config));

    auto reporter = DOCA::simple_reporter();

    for (size_t i = 0; i < config.life_time; i++) {
        sleep(1);
        std::cout << runner.report(&reporter).to_string_simple(config.payload) << std::endl;
    }
    runner.stop();
}
