#pragma once
#include "../small_bank_common.h"
#include "../xxhash64.h"



size_t hash_table_global[NUM_ACCOUNTS];

struct Savings {
    size_t custom_id;
    double balance;
    spinlock_rw_mutex mutex;
}savings_global[NUM_ENTRY];

struct Checking {
    size_t custom_id;
    double balance;
    spinlock_rw_mutex mutex;
}checking_global[NUM_ENTRY];


void init_hash_table() {
    for (size_t i = 0; i < NUM_ACCOUNTS; i++) {
        hash_table_global[i] = XXHash64::hash(&i, sizeof(i), 0) % (NUM_ENTRY);
    }
}

void init_saving() {
    for (size_t i = 0; i < NUM_ACCOUNTS; i++) {
        savings_global[i * NUM_SCALE].custom_id = i;
        savings_global[i * NUM_SCALE].balance = SIZE_MAX / 2;
    }
}

void init_checking() {
    for (size_t i = 0; i < NUM_ACCOUNTS; i++) {
        checking_global[i * NUM_SCALE].custom_id = i;
        checking_global[i * NUM_SCALE].balance = SIZE_MAX / 2;
    }
}

class ServerContext: public BasicContext {
public:
    explicit ServerContext(size_t sid): server_id_(sid) {
    }
    ~ServerContext()
        = default;
    size_t server_id_{};
    size_t stat_req_ping_tot{};
    size_t stat_req_get_data_tot{};
    size_t stat_req_get_lock_success_tot{};
    size_t stat_req_get_lock_fail_tot{};
    size_t stat_req_finish_tot{};

    void reset_stat() {
        stat_req_ping_tot = 0;
        stat_req_get_data_tot = 0;
        stat_req_get_lock_success_tot = 0;
        stat_req_get_lock_fail_tot = 0;
        stat_req_finish_tot = 0;
    }
};

class AppContext {
public:
    AppContext() {
        for (size_t i = 0;i < FLAGS_server_num; i++) {
            server_contexts_.push_back(new ServerContext(i));
        }
    }
    ~AppContext() {
        for (auto &ctx : server_contexts_) {
            delete ctx;
        }
    }
    std::vector<ServerContext *> server_contexts_;
};