#pragma once

#include "wrapper_buffer.h"
#include "wrapper_ctx.h"
#include "wrapper_device.h"
#include "wrapper_device_rep.h"
#include "wrapper_dma.h"
#include "wrapper_interface.h"
#include "wrapper_mmap.h"
#include "wrapper_workq.h"
#include "wrapper_comm_channel.h"
#include "wrapper_sync_event.h"
#include "common.hpp"
#include "bench.h"
#include "utils_common.h"

#include "gflags_common.h"


static const std::string server_name = "test_server";
static const std::string resp_success = "success";
static const uint16_t TIME_OUT = 18;

static const uint16_t MAX_MSG_SIZE = 1024;
static const uint16_t MAX_QUEUE_SIZE = 128;
static const uint16_t RTT_RESPONSE_SIZE = 8;

class channel_config {
public:
    std::string pci_address;
    std::string rep_pci_addrss;
    bool is_server = false;
    bool is_sender = false;
    bool is_read = false;
    uint32_t thread_count = 1;
    uint32_t payload = 64;
    uint32_t batch_size = 32;
    uint32_t mmap_length = 1024 * 1024 * 100;
    uint32_t life_time = 15;
};

class sync_event_config {
public:
    DOCA::sync_event *se;
    DOCA::ctx *se_ctx;
    DOCA::workq *se_workq;

    void sync_event_job_submit_sync(DOCA::to_base_job *job) const {
        doca_error_t result = se_workq->submit(job);
        if (result != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
        ::doca_event ev{};
        while ((result = se_workq->poll_completion(&ev)) == DOCA_ERROR_AGAIN) {
            /* Wait for the job to complete */
        }
        if (result != DOCA_SUCCESS) {
            throw DOCA::DOCAException(result, __FILE__, __LINE__);
        }
    }

    doca_error_t sync_event_job_submit_async(DOCA::to_base_job *job) const {
        return se_workq->submit(job);
    }

    ~sync_event_config() {
        delete se_workq;
        delete se_ctx;
        delete se;
    }
};

class dma_descriptor {
public:
    size_t local_offset;
    size_t local_payload;
    size_t remote_offset;
    size_t remote_payload;
};

void bootstrap_client(channel_config config);

void bootstrap_server(channel_config config);

// used for DMA and SYNC_EVENT
DOCA::sync_event *
handle_sync_event_import(DOCA::dev *dev, DOCA::comm_channel_ep *ep, DOCA::comm_channel_addr *peer_addr);

DOCA::sync_event *
handle_sync_event_export(DOCA::sync_event *se, DOCA::comm_channel_ep *ep,
                         DOCA::comm_channel_addr *peer_addr);


// used for DMA
void handle_dma_export(channel_config *config, sync_event_config *se_config, DOCA::bench_runner *runner,
                       DOCA::bench_stat *stat, DOCA::dev *dev,
                       DOCA::comm_channel_ep *ep,
                       DOCA::comm_channel_addr *peer_addr);

void
handle_dma_import(size_t thread_id, channel_config *config, sync_event_config *se_config, DOCA::bench_runner *runner,
                  DOCA::bench_stat *stat,
                  DOCA::dev *dev,
                  DOCA::comm_channel_ep *ep,
                  DOCA::comm_channel_addr *peer_addr);

// used for RTT
void
handle_rtt_sender(channel_config *config, DOCA::bench_runner *runner, DOCA::bench_stat *stat, DOCA::comm_channel_ep *ep,
                  DOCA::comm_channel_addr *peer_addr);

void handle_rtt_receiver(channel_config *config, DOCA::bench_runner *runner, DOCA::bench_stat *stat,
                         DOCA::comm_channel_ep *ep,
                         DOCA::comm_channel_addr *peer_addr);


// used for sync
void
handle_sync_sender(channel_config *config, DOCA::bench_runner *runner, DOCA::bench_stat *stat, DOCA::dev *dev,
                   DOCA::comm_channel_ep *ep,
                   DOCA::comm_channel_addr *peer_addr);

void handle_sync_receiver(channel_config *config, DOCA::bench_runner *runner, DOCA::bench_stat *stat, DOCA::dev *dev,
                          DOCA::comm_channel_ep *ep,
                          DOCA::comm_channel_addr *peer_addr);