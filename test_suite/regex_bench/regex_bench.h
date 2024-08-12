#pragma once

#include "wrapper_buffer.h"
#include "wrapper_ctx.h"
#include "wrapper_device.h"
#include "wrapper_mmap.h"
#include "wrapper_workq.h"
#include "wrapper_regex.h"
#include "hs_clock.h"
#include <gflags/gflags.h>


DECLARE_string(input_path);

DECLARE_string(rule_path);


class regex_config {
public:
    std::string pci_address;
    std::string input_str;
    std::string rule_str;
    size_t batch_size;
};

doca_error_t regex_bench(regex_config &config);