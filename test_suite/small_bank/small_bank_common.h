#pragma once

#include <gflags/gflags.h>
#include <hs_clock.h>
#include <hdr/hdr_histogram.h>
#include "common.hpp"
#include "spinlock_mutex.h"
#include "atomic_queue/atomic_queue.h"
#include "small_bank_rpc_type.h"
#include "numautil.h"
#include "rpc.h"


DEFINE_string(server_addr, "127.0.0.1:1234", "Server(self) address, format ip:port");
// this node
DEFINE_uint64(client_num, 1, "Client(forward usage) thread num, must >0 and <DPDK_QUEUE_NUM");
// this node
DEFINE_uint64(server_num, 1, "Server(self) thread num, must >0 and <DPDK_QUEUE_NUM");

DEFINE_uint64(timeout_second, UINT64_MAX, "Timeout second for each request, default(UINT64_MAX) means no timeout");

DEFINE_string(latency_file, "latency.txt", "Latency file name");
DEFINE_string(bandwidth_file, "bandwidth.txt", "Bandwidth file name");
DEFINE_uint64(concurrency, 0, "Concurrency for each request, 1 means sync methods, >1 means async methods");

static constexpr size_t kAppMaxConcurrency = 256;       // Outstanding reqs per thread
static constexpr size_t kAppMaxThreads = 16;                // Outstanding rpcs per thread

static constexpr size_t kAppMaxBuffer = kAppMaxConcurrency * 10;
static constexpr size_t kAppEvLoopMs = 1000; // Duration of event loop

const size_t NUM_ENTRY = 4000000 + 1;
const size_t NUM_ACCOUNTS = 1000000;
const size_t NUM_SCALE = NUM_ENTRY / NUM_ACCOUNTS;

volatile sig_atomic_t ctrl_c_pressed = 0;
void ctrl_c_handler(int) { ctrl_c_pressed = 1; }

class BasicContext {
public:
    erpc::Rpc<erpc::CTransport> *rpc_;
    std::vector<int> session_num_vec_;
    size_t num_sm_resps_ = 0; // Number of SM responses
};

void basic_sm_handler_client(int session_num, int remote_session_num, erpc::SmEventType sm_event_type,
    erpc::SmErrType sm_err_type, void *_context) {
    _unused(remote_session_num);
    printf("client sm_handler receive: session_num:%d\n", session_num);
    auto *c = static_cast<BasicContext *>(_context);
    c->num_sm_resps_++;
    DOCA::rt_assert(
        sm_err_type == erpc::SmErrType::kNoError,
        "SM response with error " + erpc::sm_err_type_str(sm_err_type));

    if (!(sm_event_type == erpc::SmEventType::kConnected ||
        sm_event_type == erpc::SmEventType::kDisconnected)) {
        throw std::runtime_error("Received unexpected SM event.");
    }

    // The callback gives us the eRPC session number - get the index in vector
//    size_t session_idx = c->session_num_vec_.size();
//    for (size_t i = 0; i < c->session_num_vec_.size(); i++)
//    {
//        if (c->session_num_vec_[i] == session_num)
//            session_idx = i;
//    }
//    DOCA::rt_assert(session_idx < c->session_num_vec_.size(),
//                    "SM callback for invalid session number.");
}

void basic_sm_handler_server(int session_num, int remote_session_num, erpc::SmEventType sm_event_type,
    erpc::SmErrType sm_err_type, void *_context) {
    _unused(remote_session_num);

    auto *c = static_cast<BasicContext *>(_context);
    c->num_sm_resps_++;

    DOCA::rt_assert(
        sm_err_type == erpc::SmErrType::kNoError,
        "SM response with error " + erpc::sm_err_type_str(sm_err_type));

    if (!(sm_event_type == erpc::SmEventType::kConnected ||
        sm_event_type == erpc::SmEventType::kDisconnected)) {
        throw std::runtime_error("Received unexpected SM event.");
    }

    c->session_num_vec_.push_back(session_num);
    printf("Server id %" PRIu8 ": Got session %d\n", c->rpc_->get_rpc_id(), session_num);
}

size_t get_bind_core(size_t numa) {
    static size_t numa0_core = 0;
    static size_t numa1_core = 0;
    static spinlock_mutex lock;
    size_t res;
    lock.lock();
    assert(numa == 0 || numa == 1);
    if (numa == 0) {
        assert(numa0_core <= DOCA::num_lcores_per_numa_node());
        res = numa0_core++;
    } else {
        DOCA::rt_assert(numa1_core <= DOCA::num_lcores_per_numa_node());
        res = numa1_core++;
    }
    lock.unlock();
    return res;
}

bool write_latency_and_reset(const std::string &filename, hdr_histogram *hdr) {

    FILE *fp = fopen(filename.c_str(), "w");
    if (fp == nullptr) {
        return false;
    }
    hdr_percentiles_print(hdr, fp, 5, 10, CLASSIC);
    fclose(fp);
    hdr_reset(hdr);
    return true;
}