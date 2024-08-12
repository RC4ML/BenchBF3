#pragma once

#include "wrapper_buffer.h"
#include "wrapper_ctx.h"
#include "wrapper_device.h"
#include "wrapper_mmap.h"
#include "wrapper_workq.h"
#include "wrapper_erasure_coding.h"
#include "hs_clock.h"
#include "bench.h"
#include "simple_reporter.h"
#include <gflags/gflags.h>

DECLARE_uint64(blocks);

DECLARE_uint64(k);

DECLARE_uint64(m);

const size_t PADDING = 64;

class ec_object {
public:
    DOCA::ec *ec;
    DOCA::matrix *encoding_matrix;
    DOCA::matrix *decoding_matrix;

};

class ec_config {
public:
    size_t size;
    size_t blocks;
    size_t each;
    size_t k;
    size_t m;
    std::string pci_address;
    size_t batch_size;
    ec_object ec_obj;

    std::vector<std::vector<char *>> data;
    size_t data_size;

    std::vector<std::vector<char *>> k_data;
    size_t k_data_size;

    std::vector<std::vector<char *>> m_data;
    size_t m_data_size;
};

doca_error_t ec_bench_encoding(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat, void *args);

doca_error_t ec_bench_decoding(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat, void *args);
