#pragma once

#include "wrapper_buffer.h"
#include "wrapper_ctx.h"
#include "wrapper_device.h"
#include "wrapper_rdma.h"
#include "wrapper_mmap.h"
#include "wrapper_pe.h"
#include "common.hpp"
#include "utils_common.h"
#include "udp_client.h"
#include "udp_server.h"
#include <gflags/gflags.h>

DECLARE_uint32(gid_index);

DECLARE_uint32(bind_offset);

DECLARE_uint32(bind_numa);

class rdma_config {
public:
    std::string network_addrss;            /* Network address, format "ip:port" */
    std::string ibdev_name;         /* IB device name */
    bool is_sender = false;                         /* Sender mode */
    uint32_t thread_count = 1;                  /* Number of threads */
    uint32_t payload = 64;                      /* Payload size */
    uint32_t batch_size = 32;                   /* Batch size */
    uint32_t mmap_length = 1024 * 1024 * 10;    /* Mmap length */
    uint32_t life_time = 15;                    /* live time in seconds */
    uint32_t gid_index = 3;                     /* GID index (default is 3) */
    uint32_t bind_offset = 0;                   /* Bind offset */
    uint32_t bind_numa = 0;                     /* Bind numa node */
    DOCA::dev *dev;
};

void bootstrap_sender(rdma_config &config);

void bootstrap_receiver(rdma_config &config);

std::string receive_and_send_rdma_param(uint64_t thread_id, std::string ip_and_port, std::string param);

