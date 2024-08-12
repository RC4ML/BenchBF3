#include "ec_bench.h"

DOCA_LOG_REGISTER(EC_BENCH);

DEFINE_uint64(blocks, 1, "Total number of blocks");

DEFINE_uint64(k, 12, "Data shards");

DEFINE_uint64(m, 4, "Parity shards");

doca_error_t print_dev_info(struct ::doca_devinfo *devinfo)
{
    _unused(devinfo);

    return DOCA_SUCCESS;
}

doca_error_t ec_bench_encoding(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat, void *args)
{
    _unused(thread_id);
    _unused(stat);
    auto *config = static_cast<ec_config *>(args);

    DOCA::dev *dev = DOCA::open_doca_device_with_pci(config->pci_address.c_str(), print_dev_info);

    auto *ctx = new DOCA::ctx(config->ec_obj.ec, std::vector<DOCA::dev *>{dev});

    auto *buf_inv = new DOCA::buf_inventory(config->batch_size * 2);

    auto *mmap = new DOCA::mmap(true);
    mmap->add_device(dev);
    mmap->set_memrange_and_start(config->data[0][0], config->data_size);

    auto *workq = new DOCA::workq(config->batch_size, config->ec_obj.ec);

    std::vector<DOCA::buf *> src_buf_vec(config->batch_size, nullptr);
    std::vector<DOCA::buf *> dst_buf_vec(config->batch_size, nullptr);

    DOCA::ec_job_create job_create(config->ec_obj.ec);
    job_create.set_matrix(config->ec_obj.encoding_matrix);
    auto enqueue_job = [&](size_t i)
    {
        size_t mod_i = i % config->batch_size;
        i = i % config->data.size();
        auto *src_buf = DOCA::buf::buf_inventory_buf_by_data(buf_inv, mmap, config->data[i][0],
                                                             config->each * config->k);
        auto dst_buf = DOCA::buf::buf_inventory_buf_by_addr(buf_inv, mmap, config->data[i][config->k],
                                                            config->each * config->m);

        src_buf_vec[mod_i] = src_buf;
        dst_buf_vec[mod_i] = dst_buf;

        job_create.set_src(src_buf);
        job_create.set_dst(dst_buf);

        doca_error_t result;
        if ((result = workq->submit(&job_create)) != DOCA_SUCCESS)
        {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
    };

    auto dequeue_job = [&](size_t i)
    {
        size_t mod_i = i % config->batch_size;
        doca_error_t result;
        ::doca_event event{};

        while ((result = workq->poll_completion(&event)) ==
               DOCA_ERROR_AGAIN)
        {
            /* Wait for the job to complete */
        }
        if (result != DOCA_SUCCESS)
        {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
        delete src_buf_vec[mod_i];
        delete dst_buf_vec[mod_i];
    };

    DOCA::Timer timer;
    DOCA::Timer total_timer;

    total_timer.tic();
    size_t total_index = 0;
    for (total_index = 0; total_index < config->batch_size; total_index++)
    {
        enqueue_job(total_index);
        timer.minus_now_time();
    }

    for (; runner->running(); total_index++)
    {
        dequeue_job(total_index - config->batch_size);
        timer.add_now_time();
        enqueue_job(total_index);
        timer.minus_now_time();
    }

    for (size_t i = 0; i < config->batch_size; i++, total_index++)
    {
        dequeue_job(total_index - config->batch_size);
        timer.add_now_time();
    }
    size_t total_time_us = total_timer.toc();

    double encMB = static_cast<double>(total_index) * config->each * (config->k + config->m) / (1 << 20);
    double speed = encMB * 1e6 / (total_time_us * 1.0);

    std::cout << DOCA::string_format("Encoded %.2lf MB in %.1lf seconds. Speed: %.2lf %s (%ld+%ld:%ld)\n", encMB,
                                     1.0 * total_time_us / 1e6, speed, "MB/s", config->k, config->m, config->each);
    std::cout << DOCA::string_format("encode %.2lf %.2lf\n", double(timer.get_now_timepoint()) / (total_index * 1000), speed);

    delete workq;
    delete buf_inv;
    delete ctx;
    delete mmap;
    delete dev;

    return DOCA_SUCCESS;
}

std::set<uint32_t> random_vec(const std::vector<uint32_t> &v, size_t a)
{
    static std::mt19937_64 rng{std::random_device{}()};
    std::set<uint32_t> result;
    while (result.size() < a)
    {
        int i = rng() % v.size();
        result.insert(v[i]);
    }
    return result;
}

doca_error_t ec_bench_decoding(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat, void *args)
{
    _unused(thread_id);
    _unused(stat);
    auto *config = static_cast<ec_config *>(args);

    DOCA::dev *dev = DOCA::open_doca_device_with_pci(config->pci_address.c_str(), print_dev_info);

    auto *ctx = new DOCA::ctx(config->ec_obj.ec, std::vector<DOCA::dev *>{dev});

    auto *buf_inv = new DOCA::buf_inventory(config->batch_size * 2);

    auto *mmap_src = new DOCA::mmap(true);
    mmap_src->add_device(dev);
    mmap_src->set_memrange_and_start(config->k_data[0][0], config->k_data_size);

    auto *mmap_dst = new DOCA::mmap(true);
    mmap_dst->add_device(dev);
    mmap_dst->set_memrange_and_start(config->m_data[0][0], config->m_data_size);

    auto *workq = new DOCA::workq(config->batch_size, config->ec_obj.ec);

    std::vector<DOCA::buf *> src_buf_vec(config->batch_size, nullptr);
    std::vector<DOCA::buf *> dst_buf_vec(config->batch_size, nullptr);
    std::vector<DOCA::matrix *> decoding_matrix(config->batch_size, nullptr);

    std::vector<uint32_t> random_pos;
    for (size_t i = 0; i < (config->k + config->m); i++)
    {
        random_pos.push_back(i);
    }

    DOCA::ec_job_recover job_recover(config->ec_obj.ec);

    auto enqueue_job = [&](size_t i)
    {
        size_t mod_i = i % config->batch_size;
        i = i % config->data.size();
        std::set<uint32_t> random_set = random_vec(random_pos, config->m);
        std::vector<uint32_t> random_set_vec{random_set.begin(), random_set.end()};
        size_t pos_i = 0, pos_j = 0;
        for (; pos_i < (config->k + config->m); pos_i++)
        {
            if (!random_set.count(pos_i))
            {
                memcpy(config->k_data[i][pos_j], config->data[i][pos_i], config->each);
                pos_j++;
                if (unlikely(pos_j == config->m))
                {
                    break;
                }
            }
        }
        config->ec_obj.decoding_matrix = config->ec_obj.encoding_matrix->create_recover_matrix(config->ec_obj.ec,
                                                                                               random_set_vec);
        job_recover.set_matrix(config->ec_obj.decoding_matrix);
        auto *src_buf = DOCA::buf::buf_inventory_buf_by_data(buf_inv, mmap_src, config->k_data[i][0],
                                                             config->each * config->k);
        auto dst_buf = DOCA::buf::buf_inventory_buf_by_addr(buf_inv, mmap_dst, config->m_data[i][0],
                                                            config->each * config->m);

        src_buf_vec[mod_i] = src_buf;
        dst_buf_vec[mod_i] = dst_buf;
        decoding_matrix[mod_i] = config->ec_obj.decoding_matrix;

        job_recover.set_src(src_buf);
        job_recover.set_dst(dst_buf);

        doca_error_t result;
        if ((result = workq->submit(&job_recover)) != DOCA_SUCCESS)
        {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
    };

    auto dequeue_job = [&](size_t i)
    {
        size_t mod_i = i % config->batch_size;
        doca_error_t result;
        ::doca_event event{};

        while ((result = workq->poll_completion(&event)) ==
               DOCA_ERROR_AGAIN)
        {
            /* Wait for the job to complete */
        }
        if (result != DOCA_SUCCESS)
        {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
        delete src_buf_vec[mod_i];
        delete dst_buf_vec[mod_i];
        delete decoding_matrix[mod_i];
    };

    DOCA::Timer timer;
    DOCA::Timer total_timer;

    total_timer.tic();
    size_t total_index = 0;
    for (total_index = 0; total_index < config->batch_size; total_index++)
    {
        enqueue_job(total_index);
        timer.minus_now_time();
    }

    for (; runner->running(); total_index++)
    {
        dequeue_job(total_index - config->batch_size);
        timer.add_now_time();
        enqueue_job(total_index);
        timer.minus_now_time();
    }

    for (size_t i = 0; i < config->batch_size; i++, total_index++)
    {
        dequeue_job(total_index - config->batch_size);
        timer.add_now_time();
    }
    size_t total_time_us = total_timer.toc();

    double encMB = static_cast<double>(total_index) * config->each * (config->k + config->m) / (1 << 20);
    double speed = encMB * 1e6 / (total_time_us * 1.0);

    std::cout << DOCA::string_format("Decoded %.2lf MB in %.1lf seconds. Speed: %.2lf %s (%ld+%ld:%ld)\n", encMB,
                                     1.0 * total_time_us / 1e6, speed, "MB/s", config->k, config->m, config->each);
    std::cout << DOCA::string_format("decode %.2lf %.2lf\n", double(timer.get_now_timepoint()) / (total_index * 1000), speed);

    delete workq;
    delete buf_inv;
    delete ctx;
    delete mmap_src;
    delete mmap_dst;
    delete dev;

    return DOCA_SUCCESS;
}