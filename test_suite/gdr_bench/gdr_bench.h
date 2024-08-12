#pragma once

#include "wrapper_buffer.h"
#include "wrapper_ctx.h"
#include "wrapper_device.h"
#include "wrapper_device_rep.h"
#include "wrapper_dma.h"
#include "wrapper_mmap.h"
#include "wrapper_pe.h"
#include "common.hpp"
#include "utils_common.h"

class gdr_config {
public:
    std::string network_addrss;            /* Network address, format "ip:port" */
    std::string pci_address;         /* PCI device address */
    bool is_read = false;                         /* Read mode */
    bool is_export = false;                         /* Export mode */
    uint32_t thread_count = 1;                  /* Number of threads */
    uint32_t payload = 64;                      /* Payload size */
    uint32_t batch_size = 32;                   /* Batch size */
    uint32_t mmap_length = 1024 * 1024 * 16;    /* Mmap length */
    uint32_t life_time = 15;                    /* live time in seconds */

    DOCA::dev *dev;
    std::string remote_mmap_export_string;
    void *remote_mmap_addr;
    size_t remote_mmap_size;
};

static const uint8_t kMagic = 123;

void bootstrap_exporter(gdr_config &config);

void bootstrap_importer(gdr_config &config);