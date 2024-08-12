#include "server.h"

#define PHY_PORT_ID 0

void ping_handler(erpc::ReqHandle *req_handle, void *_context) {
    auto *ctx = static_cast<ServerContext *>(_context);
    ctx->stat_req_ping_tot++;
    auto *req_msgbuf = req_handle->get_req_msgbuf();
    DOCA::rt_assert(req_msgbuf->get_data_size() == sizeof(RPCMsgReq<PingRPCReq>), "data size not match");

    auto *req = reinterpret_cast<RPCMsgReq<PingRPCReq> *>(req_msgbuf->buf_);

    new (req_handle->pre_resp_msgbuf_.buf_) RPCMsgResp<PingRPCResp>(req->req_common.type, req->req_common.req_number, 0, { req->req_control.timestamp });
    ctx->rpc_->resize_msg_buffer(&req_handle->pre_resp_msgbuf_, sizeof(RPCMsgResp<PingRPCResp>));

    ctx->rpc_->enqueue_response(req_handle, &req_handle->pre_resp_msgbuf_);
}

void get_data_handler(erpc::ReqHandle *req_handle, void *_context) {
    auto *ctx = static_cast<ServerContext *>(_context);
    ctx->stat_req_get_data_tot++;
    auto *req_msgbuf = req_handle->get_req_msgbuf();
    DOCA::rt_assert(req_msgbuf->get_data_size() == sizeof(RPCMsgReq<GetDataReq>), "data size not match");

    auto *req = reinterpret_cast<RPCMsgReq<GetDataReq> *>(req_msgbuf->buf_);
    size_t user1_id = req->req_control.user1_id;
    size_t user2_id = req->req_control.user2_id;
    size_t user1_id_hash = hash_table_global[user1_id];
    size_t user2_id_hash = hash_table_global[user2_id];


    auto *resp = reinterpret_cast<RPCMsgResp<GetDataResp> *>(req_handle->pre_resp_msgbuf_.buf_);
    resp->resp_control.user1_id = user1_id_hash;
    resp->resp_control.user2_id = user2_id_hash;
    if (req->req_control.get_user1_checking) {
        resp->resp_control.user1_checking = checking_global[user1_id * NUM_SCALE].balance;
    }
    if (req->req_control.get_user1_saving) {
        resp->resp_control.user1_saving = savings_global[user1_id * NUM_SCALE].balance;
    }
    if (req->req_control.get_user2_checking) {
        resp->resp_control.user2_checking = checking_global[user2_id * NUM_SCALE].balance;
    }
    if (req->req_control.get_user2_saving) {
        resp->resp_control.user2_saving = savings_global[user2_id * NUM_SCALE].balance;
    }

    resp->resp_common.req_number = req->req_common.req_number;
    resp->resp_common.type = req->req_common.type;
    resp->resp_common.status = 0;
    ctx->rpc_->resize_msg_buffer(&req_handle->pre_resp_msgbuf_, sizeof(RPCMsgResp<GetDataResp>));

    ctx->rpc_->enqueue_response(req_handle, &req_handle->pre_resp_msgbuf_);
}


void get_lock_handler(erpc::ReqHandle *req_handle, void *_context) {
    auto *ctx = static_cast<ServerContext *>(_context);
    auto *req_msgbuf = req_handle->get_req_msgbuf();
    DOCA::rt_assert(req_msgbuf->get_data_size() == sizeof(RPCMsgReq<GetLockReq>), "data size not match");

    auto *req = reinterpret_cast<RPCMsgReq<GetLockReq> *>(req_msgbuf->buf_);
    size_t user1_id = req->req_control.user1_id;
    size_t user2_id = req->req_control.user2_id;
    size_t user1_id_hash = hash_table_global[user1_id];
    size_t user2_id_hash = hash_table_global[user2_id];

    auto *resp = reinterpret_cast<RPCMsgResp<GetLockResp> *>(req_handle->pre_resp_msgbuf_.buf_);
    resp->resp_control.user1_id = user1_id_hash;
    resp->resp_control.user2_id = user2_id_hash;

    LOCK_TYPE user1_checking_lock = LOCK_TYPE::NO_LOCK, user1_saving_lock = LOCK_TYPE::NO_LOCK,
        user2_checking_lock = LOCK_TYPE::NO_LOCK, user2_saving_lock = LOCK_TYPE::NO_LOCK;
    bool lock_failed = false;

    if (!lock_failed) {
        switch (req->req_control.lock_user1_checking) {
        case LOCK_TYPE::NO_LOCK:
            break;
        case LOCK_TYPE::READ_LOCK:
            if (checking_global[user1_id * NUM_SCALE].mutex.try_lock_read()) {
                user1_checking_lock = LOCK_TYPE::READ_LOCK;
            } else {
                lock_failed = true;
            }
            break;
        case LOCK_TYPE::WRITE_LOCK:
            if (checking_global[user1_id * NUM_SCALE].mutex.try_lock_write()) {
                user1_checking_lock = LOCK_TYPE::WRITE_LOCK;
            } else {
                lock_failed = true;
            }
            break;
        }
    }

    if (!lock_failed) {
        switch (req->req_control.lock_user1_saving) {
        case LOCK_TYPE::NO_LOCK:
            break;
        case LOCK_TYPE::READ_LOCK:
            if (savings_global[user1_id * NUM_SCALE].mutex.try_lock_read()) {
                user1_saving_lock = LOCK_TYPE::READ_LOCK;
            } else {
                lock_failed = true;
            }
            break;
        case LOCK_TYPE::WRITE_LOCK:
            if (savings_global[user1_id * NUM_SCALE].mutex.try_lock_write()) {
                user1_saving_lock = LOCK_TYPE::WRITE_LOCK;
            } else {
                lock_failed = true;
            }
            break;
        }
    }

    if (!lock_failed) {
        switch (req->req_control.lock_user2_checking) {
        case LOCK_TYPE::NO_LOCK:
            break;
        case LOCK_TYPE::READ_LOCK:
            if (checking_global[user2_id * NUM_SCALE].mutex.try_lock_read()) {
                user2_checking_lock = LOCK_TYPE::READ_LOCK;
            } else {
                lock_failed = true;
            }
            break;
        case LOCK_TYPE::WRITE_LOCK:
            if (checking_global[user2_id * NUM_SCALE].mutex.try_lock_write()) {
                user2_checking_lock = LOCK_TYPE::WRITE_LOCK;
            } else {
                lock_failed = true;
            }
            break;
        }
    }

    if (!lock_failed) {
        switch (req->req_control.lock_user2_saving) {
        case LOCK_TYPE::NO_LOCK:
            break;
        case LOCK_TYPE::READ_LOCK:
            if (savings_global[user2_id * NUM_SCALE].mutex.try_lock_read()) {
                user2_saving_lock = LOCK_TYPE::READ_LOCK;
            } else {
                lock_failed = true;
            }
            break;
        case LOCK_TYPE::WRITE_LOCK:
            if (savings_global[user2_id * NUM_SCALE].mutex.try_lock_write()) {
                user2_saving_lock = LOCK_TYPE::WRITE_LOCK;
            } else {
                lock_failed = true;
            }
            break;
        }
    }

    if (lock_failed) {
        if (user1_checking_lock != LOCK_TYPE::NO_LOCK) {
            if (user1_checking_lock == LOCK_TYPE::READ_LOCK) {
                checking_global[user1_id * NUM_SCALE].mutex.unlock_read();
            } else {
                checking_global[user1_id * NUM_SCALE].mutex.unlock_write();
            }
        }
        if (user1_saving_lock != LOCK_TYPE::NO_LOCK) {
            if (user1_saving_lock == LOCK_TYPE::READ_LOCK) {
                savings_global[user1_id * NUM_SCALE].mutex.unlock_read();
            } else {
                savings_global[user1_id * NUM_SCALE].mutex.unlock_write();
            }
        }
        if (user2_checking_lock != LOCK_TYPE::NO_LOCK) {
            if (user2_checking_lock == LOCK_TYPE::READ_LOCK) {
                checking_global[user2_id * NUM_SCALE].mutex.unlock_read();
            } else {
                checking_global[user2_id * NUM_SCALE].mutex.unlock_write();
            }
        }
        if (user2_saving_lock != LOCK_TYPE::NO_LOCK) {
            if (user2_saving_lock == LOCK_TYPE::READ_LOCK) {
                savings_global[user2_id * NUM_SCALE].mutex.unlock_read();
            } else {
                savings_global[user2_id * NUM_SCALE].mutex.unlock_write();
            }
        }
        ctx->stat_req_get_lock_fail_tot++;
    } else {
        ctx->stat_req_get_lock_success_tot++;
    }

    resp->resp_common.req_number = req->req_common.req_number;
    resp->resp_common.type = req->req_common.type;
    resp->resp_common.status = 0;

    resp->resp_control.success = !lock_failed;
    ctx->rpc_->resize_msg_buffer(&req_handle->pre_resp_msgbuf_, sizeof(RPCMsgResp<GetLockResp>));

    ctx->rpc_->enqueue_response(req_handle, &req_handle->pre_resp_msgbuf_);
}

void finish_handler(erpc::ReqHandle *req_handle, void *_context) {
    auto *ctx = static_cast<ServerContext *>(_context);
    ctx->stat_req_finish_tot++;
    auto *req_msgbuf = req_handle->get_req_msgbuf();
    DOCA::rt_assert(req_msgbuf->get_data_size() == sizeof(RPCMsgReq<FinishReq>), "data size not match");

    auto *req = reinterpret_cast<RPCMsgReq<FinishReq> *>(req_msgbuf->buf_);
    size_t user1_id = req->req_control.user1_id;
    size_t user2_id = req->req_control.user2_id;
    size_t user1_id_hash = hash_table_global[user1_id];
    size_t user2_id_hash = hash_table_global[user2_id];

    auto *resp = reinterpret_cast<RPCMsgResp<FinishResp> *>(req_handle->pre_resp_msgbuf_.buf_);
    resp->resp_control.user1_id = user1_id_hash;
    resp->resp_control.user2_id = user2_id_hash;

    if (req->req_control.changed_user1_checking) {
        checking_global[user1_id * NUM_SCALE].balance = req->req_control.user1_checking;
    }
    if (req->req_control.changed_user1_saving) {
        savings_global[user1_id * NUM_SCALE].balance = req->req_control.user1_saving;
    }
    if (req->req_control.changed_user2_checking) {
        checking_global[user2_id * NUM_SCALE].balance = req->req_control.user2_checking;
    }
    if (req->req_control.changed_user2_saving) {
        savings_global[user2_id * NUM_SCALE].balance = req->req_control.user2_saving;
    }

    if (req->req_control.unlock_user1_checking != LOCK_TYPE::NO_LOCK) {
        if (req->req_control.unlock_user1_checking == LOCK_TYPE::READ_LOCK) {
            checking_global[user1_id * NUM_SCALE].mutex.unlock_read();
        } else {
            checking_global[user1_id * NUM_SCALE].mutex.unlock_write();
        }
    }
    if (req->req_control.unlock_user1_saving != LOCK_TYPE::NO_LOCK) {
        if (req->req_control.unlock_user1_saving == LOCK_TYPE::READ_LOCK) {
            savings_global[user1_id * NUM_SCALE].mutex.unlock_read();
        } else {
            savings_global[user1_id * NUM_SCALE].mutex.unlock_write();
        }
    }
    if (req->req_control.unlock_user2_checking != LOCK_TYPE::NO_LOCK) {
        if (req->req_control.unlock_user2_checking == LOCK_TYPE::READ_LOCK) {
            checking_global[user2_id * NUM_SCALE].mutex.unlock_read();
        } else {
            checking_global[user2_id * NUM_SCALE].mutex.unlock_write();
        }
    }
    if (req->req_control.unlock_user2_saving != LOCK_TYPE::NO_LOCK) {
        if (req->req_control.unlock_user2_saving == LOCK_TYPE::READ_LOCK) {
            savings_global[user2_id * NUM_SCALE].mutex.unlock_read();
        } else {
            savings_global[user2_id * NUM_SCALE].mutex.unlock_write();
        }
    }

    resp->resp_common.req_number = req->req_common.req_number;
    resp->resp_common.type = req->req_common.type;
    resp->resp_common.status = 0;
    resp->resp_control.success = true;

    ctx->rpc_->resize_msg_buffer(&req_handle->pre_resp_msgbuf_, sizeof(RPCMsgResp<FinishResp>));

    ctx->rpc_->enqueue_response(req_handle, &req_handle->pre_resp_msgbuf_);
}

void server_thread_func(size_t thread_id, ServerContext *ctx, erpc::Nexus *nexus) {
    ctx->server_id_ = thread_id;
    uint8_t rpc_id = thread_id;

    erpc::Rpc<erpc::CTransport> rpc(nexus, static_cast<void *>(ctx),
        rpc_id,
        basic_sm_handler_server, PHY_PORT_ID);
    rpc.retry_connect_on_invalid_rpc_id_ = true;
    ctx->rpc_ = &rpc;

    while (true) {
        ctx->reset_stat();
        erpc::ChronoTimer start;
        start.reset();
        rpc.run_event_loop(kAppEvLoopMs);
        const double seconds = start.get_sec();
        printf("thread %zu: ping_req : %.2f, get_data: %.2f, get_lock: %.2f/%.2f, finish: %.2f\n", thread_id,
            ctx->stat_req_ping_tot / seconds, ctx->stat_req_get_data_tot / seconds, ctx->stat_req_get_lock_success_tot / seconds,
            ctx->stat_req_get_lock_fail_tot / seconds, ctx->stat_req_finish_tot / seconds);
        ctx->rpc_->reset_dpath_stats();
        // more handler
        if (ctrl_c_pressed == 1) {
            break;
        }
    }
}

void leader_thread_func() {
    erpc::Nexus nexus(FLAGS_server_addr, 0, 0);

    nexus.register_req_func(static_cast<uint8_t>(RPC_TYPE::RPC_PING), ping_handler);
    nexus.register_req_func(static_cast<uint8_t>(RPC_TYPE::RPC_GET_DATA), get_data_handler);
    nexus.register_req_func(static_cast<uint8_t>(RPC_TYPE::RPC_GET_LOCK), get_lock_handler);
    nexus.register_req_func(static_cast<uint8_t>(RPC_TYPE::RPC_FINISH), finish_handler);

    std::vector<std::thread> servers;
    auto *context = new AppContext();
    servers.emplace_back(server_thread_func, 0, context->server_contexts_[0], &nexus);
    sleep(2);
    DOCA::bind_to_core(servers[0], 0, get_bind_core(0));

    for (size_t i = 1; i < FLAGS_server_num; i++) {
        servers.emplace_back(server_thread_func, i, context->server_contexts_[i], &nexus);
        DOCA::bind_to_core(servers[i], 0, get_bind_core(0));
    }

    sleep(2);

    if (FLAGS_timeout_second != UINT64_MAX) {
        sleep(FLAGS_timeout_second);
        ctrl_c_pressed = true;
    }

    for (size_t i = 0; i < FLAGS_server_num; i++) {
        servers[i].join();
    }
}


int main(int argc, char **argv) {
    signal(SIGINT, ctrl_c_handler);
    signal(SIGTERM, ctrl_c_handler);

    gflags::ParseCommandLineFlags(&argc, &argv, true);
    init_hash_table();
    init_saving();
    init_checking();
    std::thread leader_thread(leader_thread_func);
    DOCA::bind_to_core(leader_thread, 1, get_bind_core(1));
    leader_thread.join();
}