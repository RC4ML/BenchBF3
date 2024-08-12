#pragma once

#include "common_cross.h"

#define LOG_SQ_RING_DEPTH 7 /* 2^7 entries, max is 2^15 */
#define LOG_RQ_RING_DEPTH 7 /* 2^7 entries, max is 2^15 */
#define LOG_CQ_RING_DEPTH 7 /* 2^7 entries, max is 2^15 */

#define LOG_WQ_DATA_ENTRY_BSIZE 10 /* WQ buffer logarithmic size */

#define NUM_ENTRY  1000000
#define NUM_ACCOUNTS  1000000
#define NUM_SCALE 1

#define MAX_CONCURRENCY 128

__attribute__((unused)) static struct ether_addr CLIENT_ADDR = { {0x02, 0x01, 0x01, 0x01, 0x01, 0x01} };

// DPA (fake) mac addr
__attribute__((unused)) static struct ether_addr SERVER_ADDR = { {0x01, 0x01, 0x01, 0x01, 0x01, 0x01} };


#define RPC_PING 0
#define RPC_GET_DATA 1
#define RPC_GET_LOCK 2
#define RPC_FINISH 3


#define NO_LOCK 0
#define READ_LOCK 1
#define WRITE_LOCK 2


#define Amalgamate 0
#define Balance 1
#define DepositChecking 2
#define SendPayment 3
#define TransactSavings 4
#define WriteCheck 5

struct PingRPCReq {
    uint8_t type;
    uint32_t req_number;
    size_t timestamp;
};

struct PingRPCResp {
    uint8_t type;
    uint32_t req_number;
    int status;
    size_t timestamp;
};


struct GetDataReq {
    uint8_t type;
    uint32_t req_number;
    size_t user1_id;
    size_t user2_id;
    uint8_t get_user1_checking;
    uint8_t get_user1_saving;
    uint8_t get_user2_checking;
    uint8_t get_user2_saving;
};

struct GetDataResp {
    uint8_t type;
    uint32_t req_number;
    int status;
    size_t user1_checking;
    size_t user1_saving;
    size_t user2_checking;
    size_t user2_saving;
};

struct GetLockReq {
    uint8_t type;
    uint32_t req_number;
    size_t user1_id;
    size_t user2_id;
    uint8_t lock_user1_checking;
    uint8_t lock_user1_saving;
    uint8_t lock_user2_checking;
    uint8_t lock_user2_saving;
};

struct GetLockResp {
    uint8_t type;
    uint32_t req_number;
    int status;
    uint8_t success;
};

struct FinishReq {
    uint8_t type;
    uint32_t req_number;
    size_t user1_id;
    size_t user2_id;
    size_t user1_checking;
    size_t user1_saving;
    size_t user2_checking;
    // why can't access this user2_saving?
    size_t user2_saving;
    uint8_t changed_user1_checking;
    uint8_t changed_user1_saving;
    uint8_t changed_user2_checking;
    uint8_t changed_user2_saving;
    uint8_t unlock_user1_checking;
    uint8_t unlock_user1_saving;
    uint8_t unlock_user2_checking;
    uint8_t unlock_user2_saving;
};

struct FinishResp {
    uint8_t type;
    uint32_t req_number;
    int status;
    uint8_t success;
};
