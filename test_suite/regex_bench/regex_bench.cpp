#include "regex_bench.h"

DOCA_LOG_REGISTER(REGEX_BENCH);

DEFINE_string(input_path, "", "test input file path");

DEFINE_string(rule_path, "", "Compiled/Uncompiled hardware rule path(compiled by rxpc)");

doca_error_t print_dev_info(struct ::doca_devinfo *devinfo) {
    _unused(devinfo);
    // fuck nvidia
    // still use bf2 driver and error: "Regex not supported on the device"
    // wait nvidia's bf2 driver fix this question or release bf3 driver

//    size_t size;
//
//    doca_error_t result = doca_regex_get_maximum_job_size(devinfo, &size);
//    if (result != DOCA_SUCCESS) {
//        throw DOCA::DOCAException(result, __FILE__, __LINE__);
//    }
//    DOCA_LOG_INFO("regex_get_maximum_job_size: %ld", size);
//    result = doca_regex_get_maximum_non_huge_job_size(devinfo, &size);
//    if (result != DOCA_SUCCESS) {
//        throw DOCA::DOCAException(result, __FILE__, __LINE__);
//    }
//    DOCA_LOG_INFO("regex_get_maximum_non_huge_job_size: %ld", size);

    return DOCA_SUCCESS;
}

doca_error_t regex_bench(regex_config &config) {
    const int mempool_size = 16;


    DOCA::dev *dev = DOCA::open_doca_device_with_pci(config.pci_address.c_str(), print_dev_info);
    auto *regex = new DOCA::regex();
    regex->set_memory_pool_size(mempool_size);
    regex->set_compiled_rules(config.rule_str);

    auto *ctx = new DOCA::ctx(regex, std::vector<DOCA::dev *>{dev});

    auto *buf_inv = new DOCA::buf_inventory(config.batch_size);

    auto *mmap = new DOCA::mmap(true);
    mmap->add_device(dev);
    mmap->set_memrange_and_start(&config.input_str[0], config.input_str.size());


    auto *results = static_cast<doca_regex_search_result *>(calloc(config.batch_size,
                                                                   sizeof(struct doca_regex_search_result)));

    auto *workq = new DOCA::workq(config.batch_size, regex);

    DOCA::regex_job_search job_request(regex);
    job_request.set_allow_batch(true);

    size_t now_index = 0;
    size_t now_chunk = config.batch_size;
    size_t remain_length = config.input_str.size();
    DOCA::rt_assert(remain_length >= now_chunk, "length lower than chunk!");

    DOCA::Timer timer, total_bw_timer;
    ::doca_event event{};
    doca_error_t result;
    size_t counter1 = 0, counter2 = 0, counter3 = 0;
    size_t latency_1 = 0, latency_2 = 0, latency_3 = 0, total_time_us = 0;

    std::vector<DOCA::buf *> bufs_vec;

    for (size_t i = 0; i < config.batch_size; i++) {
        int length = (remain_length - 1) / now_chunk + 1;
        bufs_vec.push_back(
                DOCA::buf::buf_inventory_buf_by_data(buf_inv, mmap, &config.input_str[0] + now_index, length));
        remain_length -= length;
        now_chunk--;
        now_index += length;
    }


    job_request.set_rule_group_id({1});

    total_bw_timer.tic();
    timer.tic();
    for (size_t i = 0; i < config.batch_size; i++) {
        job_request.set_result_ptr(results + i);
        job_request.set_buf(bufs_vec[i]);
        if ((result = workq->submit(&job_request)) != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
    }


    for (size_t i = 0; i < config.batch_size; i++) {
        while ((result = workq->poll_completion(&event)) ==
               DOCA_ERROR_AGAIN) {
            /* Wait for the job to complete */
        }
        auto *result_ptr = static_cast<doca_regex_search_result *>(event.result.ptr);
        counter1 += result_ptr->detected_matches;
    }
    latency_1 += timer.toc_ns();

    job_request.set_rule_group_id({2});

    timer.tic();
    for (size_t i = 0; i < config.batch_size; i++) {
        job_request.set_result_ptr(results + i);
        job_request.set_buf(bufs_vec[i]);
        if ((result = workq->submit(&job_request)) != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
    }


    for (size_t i = 0; i < config.batch_size; i++) {
        while ((result = workq->poll_completion(&event)) ==
               DOCA_ERROR_AGAIN) {
            /* Wait for the job to complete */
        }
        auto *result_ptr = static_cast<doca_regex_search_result *>(event.result.ptr);
        counter2 += result_ptr->detected_matches;
    }
    latency_2 += timer.toc_ns();

    job_request.set_rule_group_id({3});

    timer.tic();
    for (size_t i = 0; i < config.batch_size; i++) {
        job_request.set_result_ptr(results + i);
        job_request.set_buf(bufs_vec[i]);
        if ((result = workq->submit(&job_request)) != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
    }


    for (size_t i = 0; i < config.batch_size; i++) {
        while ((result = workq->poll_completion(&event)) ==
               DOCA_ERROR_AGAIN) {
            /* Wait for the job to complete */
        }
        auto *result_ptr = static_cast<doca_regex_search_result *>(event.result.ptr);
        counter3 += result_ptr->detected_matches;
    }
    latency_3 += timer.toc_ns();
    total_time_us = total_bw_timer.toc();

    std::cout << "counter1: " << counter1 << std::endl;
    std::cout << "latency1: " << latency_1 / 1000 << "us" << std::endl;
    std::cout << "counter2: " << counter2 << std::endl;
    std::cout << "latency2: " << latency_2 / 1000 << "us" << std::endl;
    std::cout << "counter3: " << counter3 << std::endl;
    std::cout << "latency3: " << latency_3 / 1000 << "us" << std::endl;

    std::cout << "bandwidth:" << config.input_str.size() * 3 / total_time_us << "MB/s" << std::endl;
    for (auto &m: bufs_vec) {
        delete m;
    }

    free(results);
    delete workq;
    delete buf_inv;
    delete ctx;
    delete mmap;
    delete dev;
    delete regex;

    return DOCA_SUCCESS;

}