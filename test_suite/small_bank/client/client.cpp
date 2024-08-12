#include "client.h"

#define PHY_PORT_ID 0


void connect_sessions(ClientContext *c) {

    // connect to backward server
    c->server_session_num_ = c->rpc_->create_session(server_addr, c->client_id_);
    DOCA::rt_assert(c->server_session_num_ >= 0, "Failed to create session");

    while (c->num_sm_resps_ != 1) {
        c->rpc_->run_event_loop(kAppEvLoopMs);
        if (unlikely(ctrl_c_pressed == 1)) {
            printf("Ctrl-C pressed. Exiting\n");
            return;
        }
    }
}


void callback_ping(void *_context, void *_tag) {
    _unused(_tag);
    auto *ctx = static_cast<ClientContext *>(_context);

    auto *resp = reinterpret_cast<RPCMsgResp<PingRPCResp>*>(ctx->ping_resp_msgbuf.buf_);

    // 如果返回值不为0，则认为后续不会有响应，直接将请求号和错误码放入队列
    // 如果返回值为0，则认为后续将有响应，不car
    DOCA::rt_assert(resp->resp_common.status == 0, "Ping callback failed");

    // tell leader thread to begin
    ctx->queue_store->push(REQ_MSG{ 0, RPC_TYPE::RPC_PING });
}

void handler_ping(ClientContext *ctx, REQ_MSG req_msg) {
    new (ctx->ping_msgbuf.buf_) RPCMsgReq<PingRPCReq>(RPC_TYPE::RPC_PING, req_msg.req_id, { 0 });
    ctx->rpc_->enqueue_request(ctx->server_session_num_, static_cast<uint8_t>(RPC_TYPE::RPC_PING),
        &ctx->ping_msgbuf, &ctx->ping_resp_msgbuf,
        callback_ping, nullptr);
}

void callback_get_data(void *_context, void *_tag) {
    auto req_id_ptr = reinterpret_cast<std::uintptr_t>(_tag);
    uint32_t req_id = req_id_ptr;
    auto *ctx = static_cast<ClientContext *>(_context);

    auto *resp = reinterpret_cast<RPCMsgResp<GetDataResp> *>(ctx->resp_msgbuf[req_id % kAppMaxBuffer].buf_);

    DOCA::rt_assert(resp->resp_common.status == 0, "[get_data]resp status error");
    DOCA::rt_assert(resp->resp_common.req_number == req_id, "[get_data]resp req_number error");
    DOCA::rt_assert(resp->resp_common.type == RPC_TYPE::RPC_GET_DATA, "[get_data]resp type error");
    // TODO

    Transition &transition = ctx->transitions[req_id % kAppMaxBuffer];
    transition.user1_checking_balance = resp->resp_control.user1_checking;
    transition.user1_saving_balance = resp->resp_control.user1_saving;
    transition.user2_checking_balance = resp->resp_control.user2_checking;
    transition.user2_saving_balance = resp->resp_control.user2_saving;

    ctx->queue_store->push(REQ_MSG{ req_id, RPC_TYPE::RPC_GET_LOCK });
    return;
}

void handler_get_data(ClientContext *ctx, REQ_MSG req_msg) {
    erpc::MsgBuffer &req_msgbuf = ctx->req_msgbuf[req_msg.req_id % kAppMaxBuffer];
    erpc::MsgBuffer &resp_msgbuf = ctx->resp_msgbuf[req_msg.req_id % kAppMaxBuffer];
    Transition &transition = ctx->transitions[req_msg.req_id % kAppMaxBuffer];
    transition = Transition(req_msg.req_id);


    auto *req = reinterpret_cast<RPCMsgReq<GetDataReq> *>(req_msgbuf.buf_);
    req->req_common.req_number = req_msg.req_id;
    req->req_common.type = RPC_TYPE::RPC_GET_DATA;

    req->req_control.user1_id = transition.user1_id;
    req->req_control.user2_id = transition.user2_id;

    req->req_control.get_user1_checking = transition.lock_user1_checking == LOCK_TYPE::NO_LOCK ? 0 : 1;
    req->req_control.get_user1_saving = transition.lock_user1_saving == LOCK_TYPE::NO_LOCK ? 0 : 1;
    req->req_control.get_user2_checking = transition.lock_user2_checking == LOCK_TYPE::NO_LOCK ? 0 : 1;
    req->req_control.get_user2_saving = transition.lock_user2_saving == LOCK_TYPE::NO_LOCK ? 0 : 1;


    ctx->rpc_->resize_msg_buffer(&req_msgbuf, sizeof(RPCMsgReq<GetDataReq>));

    timers[ctx->client_id_][req_msg.req_id % kAppMaxBuffer].tic();
    ctx->rpc_->enqueue_request(ctx->server_session_num_, static_cast<uint8_t>(RPC_TYPE::RPC_GET_DATA),
        &req_msgbuf, &resp_msgbuf,
        callback_get_data, reinterpret_cast<void *>(req_msg.req_id));
}

void callback_get_lock(void *_context, void *_tag) {
    auto req_id_ptr = reinterpret_cast<std::uintptr_t>(_tag);
    uint32_t req_id = req_id_ptr;
    auto *ctx = static_cast<ClientContext *>(_context);

    auto *resp = reinterpret_cast<RPCMsgResp<GetLockResp> *>(ctx->resp_msgbuf[req_id % kAppMaxBuffer].buf_);
    DOCA::rt_assert(resp->resp_common.status == 0, "[get_lock]resp status error");
    DOCA::rt_assert(resp->resp_common.req_number == req_id, "[get_lock]resp req_number error");
    DOCA::rt_assert(resp->resp_common.type == RPC_TYPE::RPC_GET_LOCK, "[get_lock]resp type error");

    if (resp->resp_control.success) {
        ctx->queue_store->push(REQ_MSG{ req_id, RPC_TYPE::RPC_FINISH });
    } else {
        // TODO存疑，是使用ctx->req_id_++ 还是 req_id + FLAGS_concurrency
        ctx->req_id_++;
        ctx->queue_store->push(REQ_MSG{ req_id + static_cast<uint32_t>(FLAGS_concurrency) , RPC_TYPE::RPC_GET_DATA });
    }

}

void handler_get_lock(ClientContext *ctx, REQ_MSG req_msg) {
    erpc::MsgBuffer &req_msgbuf = ctx->req_msgbuf[req_msg.req_id % kAppMaxBuffer];
    erpc::MsgBuffer &resp_msgbuf = ctx->resp_msgbuf[req_msg.req_id % kAppMaxBuffer];

    Transition &transition = ctx->transitions[req_msg.req_id % kAppMaxBuffer];

    auto *req = reinterpret_cast<RPCMsgReq<GetLockReq> *>(req_msgbuf.buf_);
    req->req_common.req_number = req_msg.req_id;
    req->req_common.type = RPC_TYPE::RPC_GET_LOCK;

    req->req_control.user1_id = transition.user1_id;
    req->req_control.user2_id = transition.user2_id;

    req->req_control.lock_user1_checking = transition.lock_user1_checking;
    req->req_control.lock_user1_saving = transition.lock_user1_saving;
    req->req_control.lock_user2_checking = transition.lock_user2_checking;
    req->req_control.lock_user2_saving = transition.lock_user2_saving;

    ctx->rpc_->resize_msg_buffer(&req_msgbuf, sizeof(RPCMsgReq<GetLockReq>));

    ctx->rpc_->enqueue_request(ctx->server_session_num_, static_cast<uint8_t>(RPC_TYPE::RPC_GET_LOCK),
        &req_msgbuf, &resp_msgbuf,
        callback_get_lock, reinterpret_cast<void *>(req_msg.req_id));
}

void callback_finish(void *_context, void *_tag) {
    auto req_id_ptr = reinterpret_cast<std::uintptr_t>(_tag);
    uint32_t req_id = req_id_ptr;
    auto *ctx = static_cast<ClientContext *>(_context);

    auto *resp = reinterpret_cast<RPCMsgResp<FinishResp> *>(ctx->resp_msgbuf[req_id % kAppMaxBuffer].buf_);
    DOCA::rt_assert(resp->resp_common.status == 0, "[finish]resp status error");
    DOCA::rt_assert(resp->resp_common.req_number == req_id, "[finish]resp req_number error");
    DOCA::rt_assert(resp->resp_common.type == RPC_TYPE::RPC_FINISH, " [finish]resp type error");

    DOCA::rt_assert(resp->resp_control.success == true, "[finish] resp failed");
    hdr_record_value_atomic(latency_total_, static_cast<int64_t>(timers[ctx->client_id_][req_id % kAppMaxBuffer].toc()) * 10);

    ctx->req_id_++;
    ctx->queue_store->push(REQ_MSG{ req_id + static_cast<uint32_t>(FLAGS_concurrency) , RPC_TYPE::RPC_GET_DATA });

}

void handler_finish(ClientContext *ctx, REQ_MSG req_msg) {
    erpc::MsgBuffer &req_msgbuf = ctx->req_msgbuf[req_msg.req_id % kAppMaxBuffer];
    erpc::MsgBuffer &resp_msgbuf = ctx->resp_msgbuf[req_msg.req_id % kAppMaxBuffer];

    Transition &transition = ctx->transitions[req_msg.req_id % kAppMaxBuffer];
    auto *req = reinterpret_cast<RPCMsgReq<FinishReq> *>(req_msgbuf.buf_);
    req->req_common.req_number = req_msg.req_id;
    req->req_common.type = RPC_TYPE::RPC_FINISH;

    req->req_control.user1_id = transition.user1_id;
    req->req_control.user2_id = transition.user2_id;
    req->req_control.unlock_user1_checking = transition.lock_user1_checking;
    req->req_control.unlock_user1_saving = transition.lock_user1_saving;
    req->req_control.unlock_user2_checking = transition.lock_user2_checking;
    req->req_control.unlock_user2_saving = transition.lock_user2_saving;
    req->req_control.changed_user1_checking = transition.changed_user1_checking;
    req->req_control.changed_user1_saving = transition.changed_user1_saving;
    req->req_control.changed_user2_checking = transition.changed_user2_checking;
    req->req_control.changed_user2_saving = transition.changed_user2_saving;

    switch (transition.transmit_type) {
    case TRANSMIT_TYPE::Amalgamate:
        req->req_control.user2_saving = transition.user1_checking_balance + transition.user2_saving_balance;
        req->req_control.user1_checking = 0;
        break;
    case TRANSMIT_TYPE::Balance:
        break;
    case TRANSMIT_TYPE::DepositChecking:
        req->req_control.user1_checking = transition.user1_checking_balance + transition.amount;
        break;
    case TRANSMIT_TYPE::SendPayment:
        req->req_control.user1_checking = transition.user1_checking_balance - transition.amount;
        req->req_control.user2_checking = transition.user2_checking_balance + transition.amount;
        break;
    case TRANSMIT_TYPE::TransactSavings:
        req->req_control.user1_saving = transition.user1_saving_balance - transition.amount;
        break;
    case TRANSMIT_TYPE::WriteCheck:
        req->req_control.user1_checking = transition.user1_checking_balance - transition.amount;
        break;
    }

    ctx->rpc_->resize_msg_buffer(&req_msgbuf, sizeof(RPCMsgReq<FinishReq>));

    ctx->rpc_->enqueue_request(ctx->server_session_num_, static_cast<uint8_t>(RPC_TYPE::RPC_FINISH),
        &req_msgbuf, &resp_msgbuf,
        callback_finish, reinterpret_cast<void *>(req_msg.req_id));
}

void client_thread_func(size_t thread_id, ClientContext *ctx, erpc::Nexus *nexus) {
    ctx->client_id_ = thread_id;
    uint8_t rpc_id = thread_id;

    erpc::Rpc<erpc::CTransport> rpc(nexus, static_cast<void *>(ctx),
        rpc_id,
        basic_sm_handler_client, PHY_PORT_ID);
    rpc.retry_connect_on_invalid_rpc_id_ = true;
    ctx->rpc_ = &rpc;
    for (size_t i = 0; i < kAppMaxBuffer; i++) {
        ctx->req_msgbuf[i] = rpc.alloc_msg_buffer_or_die(256);
        ctx->resp_msgbuf[i] = rpc.alloc_msg_buffer_or_die(256);
    }
    ctx->ping_msgbuf = rpc.alloc_msg_buffer_or_die(sizeof(RPCMsgReq<PingRPCReq>));
    ctx->ping_resp_msgbuf = rpc.alloc_msg_buffer_or_die(sizeof(RPCMsgResp<PingRPCResp>));
    connect_sessions(ctx);

    using FUNC_HANDLER = std::function<void(ClientContext *, REQ_MSG)>;
    std::map<RPC_TYPE, FUNC_HANDLER > handlers{
            {RPC_TYPE::RPC_PING, handler_ping},
            {RPC_TYPE::RPC_GET_DATA, handler_get_data},
            {RPC_TYPE::RPC_GET_LOCK, handler_get_lock},
            {RPC_TYPE::RPC_FINISH, handler_finish},
    };

    while (true) {
        unsigned size = ctx->queue_store->was_size();
        for (unsigned i = 0; i < size; i++) {
            REQ_MSG req_msg = ctx->queue_store->pop();

            handlers[req_msg.req_type](ctx, req_msg);
        }
        ctx->rpc_->run_event_loop_once();
        if (unlikely(ctrl_c_pressed)) {
            printf("Thread %zu: Ctrl-C pressed. Exiting\n", thread_id);
            break;
        }
    }
}

void leader_thread_func() {
    erpc::Nexus nexus(FLAGS_server_addr, 0, 0);

    std::vector<std::thread> clients;
    auto *context = new AppContext();
    clients.emplace_back(client_thread_func, 0, context->client_contexts_[0], &nexus);
    sleep(2);
    DOCA::bind_to_core(clients[0], 0, get_bind_core(0));

    for (size_t i = 1; i < FLAGS_client_num; i++) {
        clients.emplace_back(client_thread_func, i, context->client_contexts_[i], &nexus);
        DOCA::bind_to_core(clients[i], 0, get_bind_core(0));
    }

    sleep(2);

    for (size_t i = 0; i < FLAGS_client_num; i++) {
        context->client_contexts_[i]->queue_store->push(REQ_MSG{ 0, RPC_TYPE::RPC_PING });
    }
    for (size_t i = 0; i < FLAGS_client_num; i++) {
        context->client_contexts_[i]->queue_store->pop();
    }

    printf("ping ready, begin to generate param\n");

    for (size_t i = 0; i < FLAGS_client_num; i++) {
        size_t tmp = FLAGS_concurrency;
        while (tmp--) {
            context->client_contexts_[i]->queue_store->push(REQ_MSG{ context->client_contexts_[i]->req_id_++, RPC_TYPE::RPC_GET_DATA });
        }
    }
    if (FLAGS_timeout_second != UINT64_MAX) {
        sleep(FLAGS_timeout_second);
        ctrl_c_pressed = true;
    }

    for (size_t i = 0; i < FLAGS_client_num; i++) {
        clients[i].join();
    }
    write_latency_and_reset(FLAGS_latency_file, latency_total_);
}

int main(int argc, char **argv) {
    signal(SIGINT, ctrl_c_handler);
    signal(SIGTERM, ctrl_c_handler);

    gflags::ParseCommandLineFlags(&argc, &argv, true);
    int ret = hdr_init(1, 1000 * 1000 * 10, 3,
        &latency_total_);
    assert(ret == 0);

    std::thread leader_thread(leader_thread_func);
    DOCA::bind_to_core(leader_thread, 1, get_bind_core(1));

    leader_thread.join();
    hdr_close(latency_total_);
}