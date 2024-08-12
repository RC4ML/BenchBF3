#pragma once
#include <cstdint>
#include <cstddef>

enum class RPC_TYPE: uint8_t {
    RPC_PING = 0,
    RPC_GET_DATA,
    RPC_GET_LOCK,
    RPC_FINISH,
};

enum class LOCK_TYPE: uint8_t {
    NO_LOCK = 0,
    READ_LOCK,
    WRITE_LOCK,
};

enum class TRANSMIT_TYPE: uint8_t {
    Amalgamate = 0,
    Balance,
    DepositChecking,
    SendPayment,
    TransactSavings,
    WriteCheck,
};

class CommonReq {
public:
    RPC_TYPE type;
    // for debug
    uint32_t req_number;
} __attribute__((packed));

class CommonResp {
public:
    RPC_TYPE type;
    // for debug
    uint32_t req_number;
    int status;
} __attribute__((packed));

template<class T>
class RPCMsgReq {
public:
    CommonReq req_common;
    T req_control;
    explicit RPCMsgReq(RPC_TYPE t, uint32_t num, T req): req_common{ t, num }, req_control(req) {}
}__attribute__((packed));

template<class T>
class RPCMsgResp {
public:
    CommonResp resp_common;
    T resp_control;
    explicit RPCMsgResp(RPC_TYPE t, uint32_t num, int status, T resp): resp_common{ t, num, status }, resp_control(resp) {}
}__attribute__((packed));

class PingRPCReq {
public:
    size_t timestamp;
};

class PingRPCResp {
public:
    size_t timestamp;
};

class GetDataReq {
public:
    size_t user1_id;
    size_t user2_id;
    uint8_t get_user1_checking;
    uint8_t get_user1_saving;
    uint8_t get_user2_checking;
    uint8_t get_user2_saving;
};

class GetDataResp {
public:
    size_t user1_id;
    size_t user2_id;
    double user1_checking;
    double user1_saving;
    double user2_checking;
    double user2_saving;
};

class GetLockReq {
public:
    size_t user1_id;
    size_t user2_id;
    LOCK_TYPE lock_user1_checking;
    LOCK_TYPE lock_user1_saving;
    LOCK_TYPE lock_user2_checking;
    LOCK_TYPE lock_user2_saving;
};

class GetLockResp {
public:
    size_t user1_id;
    size_t user2_id;
    uint8_t success;
};

class FinishReq {
public:
    size_t user1_id;
    size_t user2_id;
    double user1_checking;
    double user1_saving;
    double user2_checking;
    double user2_saving;
    uint8_t changed_user1_checking;
    uint8_t changed_user1_saving;
    uint8_t changed_user2_checking;
    uint8_t changed_user2_saving;
    LOCK_TYPE unlock_user1_checking;
    LOCK_TYPE unlock_user1_saving;
    LOCK_TYPE unlock_user2_checking;
    LOCK_TYPE unlock_user2_saving;
};

class FinishResp {
public:
    size_t user1_id;
    size_t user2_id;
    uint8_t success;
};