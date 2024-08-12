#pragma once

#include "wrapper_buffer.h"
#include "wrapper_ctx.h"
#include "wrapper_device.h"
#include "wrapper_mmap.h"
#include "wrapper_workq.h"
#include "wrapper_compress.h"
#include "hs_clock.h"
#include <gflags/gflags.h>


DECLARE_string(input_path);


class compress_config {
public:
    std::string pci_address;
    std::string input_str;
    size_t batch_size;
};


doca_error_t compress_bench(compress_config &config, bool is_lz4);