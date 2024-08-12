#include "rdma_bench.h"
#include "bench.h"
#include "simple_reporter.h"
#include "reporter.h"

DOCA_LOG_REGISTER(RECEIVER);

class timestamps_acc_number {
public:
    uint32_t timestamp;
    uint32_t acc_number;
    bool operator < (const timestamps_acc_number &tmp) const {
        return (timestamp < tmp.timestamp);
    }
    bool operator > (const timestamps_acc_number &tmp) const {
        return (timestamp > tmp.timestamp);
    }
    bool operator == (const timestamps_acc_number &tmp) const {
        return timestamp == tmp.timestamp && acc_number == tmp.acc_number;
    }
};

class rdma_task_receive_config {
public:
    DOCA::mmap *local_mmap;
    DOCA::buf_inventory *inv;
    DOCA::rdma *rdma;
    DOCA::ctx *ctx;
    DOCA::pe *pe;

    std::vector<DOCA::rdma_task_receive *> task_buffers;
    std::vector<DOCA::buf *>task_local_buf;

    size_t batch_size;
    size_t payload;
    char *local_buffer;

    DOCA::bench_stat *stat;
    DOCA::bench_runner *runner;

};

void do_rdma_receive(size_t thread_id, std::string network_addr, rdma_task_receive_config &task_receive_config) {
    size_t batch_size = task_receive_config.batch_size;
    task_receive_config.task_buffers.resize(batch_size);
    task_receive_config.task_local_buf.resize(batch_size);

    auto complete_callback = [](struct doca_rdma_task_receive *useless_task,
        union doca_data task_user_data, union doca_data ctx_user_data) -> void {
            _unused(useless_task);
            auto *worker_config = reinterpret_cast<rdma_task_receive_config *>(ctx_user_data.ptr);
            // worker_config->stat->finish_one_op();

            size_t task_index = task_user_data.u64;
            // uint32_t *data = reinterpret_cast<uint32_t *>(worker_config->task_local_buf[task_index]->get_data());
            // size_t length = worker_config->task_local_buf[task_index]->get_length();

            worker_config->task_local_buf[task_index]->set_data(0ul, 0ul);

            // reuse this task
            worker_config->pe->submit(worker_config->task_buffers[task_index]);
        };

    auto error_callback = [](struct doca_rdma_task_receive *useless_task,
        union doca_data task_user_data, union doca_data ctx_user_data) -> void {
            _unused(useless_task);
            auto *worker_config = reinterpret_cast<rdma_task_receive_config *>(ctx_user_data.ptr);

            size_t task_index = task_user_data.u64;
            printf("task %ld error %s\n", task_index, doca_error_get_descr(doca_task_get_status(worker_config->task_buffers[task_index]->to_base())));
            exit(1);
        };

    // tmp setup //
    doca_rdma_set_send_queue_size(task_receive_config.rdma->get_inner_ptr(), 512);
    doca_rdma_set_recv_queue_size(task_receive_config.rdma->get_inner_ptr(), 512);
    // tmp setup //
    task_receive_config.rdma->set_task_receive_conf(batch_size, complete_callback, error_callback);
    task_receive_config.pe->start_ctx();

    std::string remote_rdma_param = receive_and_send_rdma_param(thread_id, network_addr, task_receive_config.rdma->export_rdma());
    task_receive_config.rdma->connect_rdma(remote_rdma_param);
    // move to running state
    task_receive_config.pe->poll_progress();
    printf("connect success\n");

    for (size_t i = 0;i < batch_size;i++) {
        task_receive_config.task_local_buf[i] = DOCA::buf::buf_inventory_buf_by_addr(task_receive_config.inv,
            task_receive_config.local_mmap, task_receive_config.local_buffer + i * task_receive_config.payload, task_receive_config.payload);
        doca_data index_data = { .u64 = i };
        task_receive_config.task_buffers[i] = new DOCA::rdma_task_receive(task_receive_config.rdma, task_receive_config.task_local_buf[i], index_data);
        if (task_receive_config.pe->submit(task_receive_config.task_buffers[i]) != DOCA_SUCCESS) {
            printf("error %ld\n", i);
            exit(1);
        }
    }

    while (task_receive_config.runner->running()) {
        task_receive_config.pe->poll_progress();
    }
    for (size_t i = 0;i < batch_size;i++) {
        delete task_receive_config.task_local_buf[i];
        delete task_receive_config.task_buffers[i];
    }
}



void perform_receiver_routine(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat, void *args) {
    // wait for bind to numa node
    sleep(2);
    auto *config = static_cast<rdma_config *>(args);
    char *local_buffer = reinterpret_cast<char *>(malloc(config->batch_size * config->payload));

    rdma_task_receive_config task_receive_config;
    DOCA::dev *dev = config->dev;

    task_receive_config.local_mmap = new DOCA::mmap(true);
    task_receive_config.local_mmap->add_device(dev);
    task_receive_config.local_mmap->set_memrange_and_start(local_buffer, config->batch_size * config->payload);

    task_receive_config.rdma = new DOCA::rdma(dev, config->gid_index);

    task_receive_config.ctx = new DOCA::ctx(task_receive_config.rdma);
    task_receive_config.pe = new DOCA::pe();
    // don't forget to start ctx with pe->start_ctx
    task_receive_config.pe->connect_ctx(task_receive_config.ctx);

    task_receive_config.inv = new DOCA::buf_inventory(1024);

    task_receive_config.batch_size = config->batch_size;
    task_receive_config.payload = config->payload;
    task_receive_config.local_buffer = local_buffer;
    task_receive_config.runner = runner;
    task_receive_config.stat = stat;

    task_receive_config.ctx->set_data({ &task_receive_config });
    do_rdma_receive(thread_id, config->network_addrss, task_receive_config);

    // still have some bug on delete object, so we don't delete these.

    // delete task_receive_config.inv;
    // delete task_receive_config.ctx;
    // delete ctx convert state to stopping state

    // doca_pe_progress() will cause all tasks to be flushed, and finally transition state to idle

    // while (task_receive_config.pe->poll_progress() != 1) {}
    // delete task_receive_config.rdma;
    // delete task_receive_config.pe;
    // delete task_receive_config.local_mmap;

    free(local_buffer);
    printf("thread %ld finish\n", thread_id);
}

void bootstrap_receiver(rdma_config &config) {
    auto runner = DOCA::bench_runner(config.thread_count, config.bind_offset, config.bind_numa);
    config.dev = DOCA::open_doca_device_with_ibdev_name(config.ibdev_name.c_str(), DOCA::rdma_send_job_is_supported);
    runner.run(perform_receiver_routine, &config);

    auto reporter = DOCA::simple_reporter();

    for (size_t i = 0; i < config.life_time; i++) {
        sleep(1);
        std::cout << runner.report(&reporter).to_string_simple(config.payload) << std::endl;
    }
    runner.stop();
    printf("stopped all threads, Bye!\n");
}