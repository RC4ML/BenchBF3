#pragma once
#include"../small_bank_common.h"

hdr_histogram *latency_total_;
// hardcode tmp
std::string server_addr = "172.25.2.24:31851";
std::vector<std::vector<DOCA::Timer>> timers(kAppMaxThreads, std::vector<DOCA::Timer>(kAppMaxBuffer));

struct REQ_MSG {
    uint32_t req_id;
    RPC_TYPE req_type;
};

static_assert(sizeof(REQ_MSG) == 8, "REQ_MSG size is not 8");

TRANSMIT_TYPE generateRandomType() {
    // 设置随机数引擎
    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::discrete_distribution<> dist{
        15, 15, 15, 25, 15, 15
    };

    int index = dist(gen);

    return static_cast<TRANSMIT_TYPE>(index);
}

size_t generateRandomNumber() {
    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_int_distribution<> dist(0, NUM_ACCOUNTS - 1);

    return dist(gen);
}

class Transition {
public:
    Transition() = default;
    Transition(uint32_t req_id):req_id_(req_id) {
        user1_id = user2_id = 0;
        lock_user1_checking = lock_user1_saving =
            lock_user2_checking = lock_user2_saving = LOCK_TYPE::NO_LOCK;
        changed_user1_checking = changed_user1_saving =
            changed_user2_checking = changed_user2_saving = 0;


        transmit_type = generateRandomType();
        switch (transmit_type) {
        case TRANSMIT_TYPE::Amalgamate:
            user1_id = generateRandomNumber();
            user2_id = generateRandomNumber();
            while (user2_id == user1_id) {
                user2_id = generateRandomNumber();
            }
            lock_user1_checking = LOCK_TYPE::WRITE_LOCK;
            lock_user2_saving = LOCK_TYPE::WRITE_LOCK;
            changed_user1_checking = 1;
            changed_user2_saving = 1;
            break;
        case TRANSMIT_TYPE::Balance:
            user1_id = generateRandomNumber();
            user2_id = user1_id;
            lock_user1_checking = LOCK_TYPE::READ_LOCK;
            lock_user1_saving = LOCK_TYPE::READ_LOCK;
            break;

        case TRANSMIT_TYPE::DepositChecking:
            user1_id = generateRandomNumber();
            user2_id = user1_id;
            lock_user1_checking = LOCK_TYPE::WRITE_LOCK;
            changed_user1_checking = 1;
            break;

        case TRANSMIT_TYPE::SendPayment:
            user1_id = generateRandomNumber();
            user2_id = generateRandomNumber();
            while (user2_id == user1_id) {
                user2_id = generateRandomNumber();
            }
            lock_user1_checking = LOCK_TYPE::WRITE_LOCK;
            lock_user2_checking = LOCK_TYPE::WRITE_LOCK;
            changed_user1_checking = 1;
            changed_user2_checking = 1;
            break;
        case TRANSMIT_TYPE::TransactSavings:
            user1_id = generateRandomNumber();
            user2_id = user1_id;
            lock_user1_saving = LOCK_TYPE::WRITE_LOCK;
            changed_user1_saving = 1;
            break;
        case TRANSMIT_TYPE::WriteCheck:
            user1_id = generateRandomNumber();
            user2_id = user1_id;
            lock_user1_checking = LOCK_TYPE::WRITE_LOCK;
            lock_user1_saving = LOCK_TYPE::READ_LOCK;
            changed_user1_checking = 1;
            break;
        default:
            printf("error\n");
            exit(1);
        }
    }
    uint32_t req_id_;
    TRANSMIT_TYPE transmit_type;
    RPC_TYPE now_type;
    size_t user1_id;
    size_t user2_id;
    LOCK_TYPE lock_user1_checking{ LOCK_TYPE::NO_LOCK };
    LOCK_TYPE lock_user1_saving{ LOCK_TYPE::NO_LOCK };
    LOCK_TYPE lock_user2_checking{ LOCK_TYPE::NO_LOCK };
    LOCK_TYPE lock_user2_saving{ LOCK_TYPE::NO_LOCK };
    uint8_t changed_user1_checking{ 0 };
    uint8_t changed_user1_saving{ 0 };
    uint8_t changed_user2_checking{ 0 };
    uint8_t changed_user2_saving{ 0 };
    double amount = 0.001;
    double user1_checking_balance;
    double user1_saving_balance;
    double user2_checking_balance;
    double user2_saving_balance;
};

class ClientContext: public BasicContext {
public:
    ClientContext(size_t client_id): client_id_(client_id) {
        queue_store = new atomic_queue::AtomicQueueB2<REQ_MSG, std::allocator<REQ_MSG>, true, false, true>(kAppMaxBuffer);
    }

    ~ClientContext() {
        delete queue_store;
    }
    size_t client_id_;
    erpc::MsgBuffer req_msgbuf[kAppMaxBuffer];
    erpc::MsgBuffer resp_msgbuf[kAppMaxBuffer];
    Transition transitions[kAppMaxBuffer];
    erpc::MsgBuffer ping_msgbuf;
    erpc::MsgBuffer ping_resp_msgbuf;

    int server_session_num_;
    atomic_queue::AtomicQueueB2<REQ_MSG, std::allocator<REQ_MSG>, true, false, true> *queue_store;
    uint32_t req_id_ = 1;
};

class AppContext {
public:
    AppContext() {
        for (size_t i = 0; i < FLAGS_client_num; i++) {
            client_contexts_.push_back(new ClientContext(i));
        }
    }
    ~AppContext() {
        for (auto &ctx : client_contexts_) {
            delete ctx;
        }
    }
    std::vector<ClientContext *> client_contexts_;
};