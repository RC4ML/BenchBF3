#pragma once

#include "common_cross.h"

#define LOG_SQ_RING_DEPTH 7 /* 2^7 entries, max is 2^15 */
#define LOG_RQ_RING_DEPTH 7 /* 2^7 entries, max is 2^15 */
#define LOG_CQ_RING_DEPTH 7 /* 2^7 entries, max is 2^15 */

#define LOG_WQ_DATA_ENTRY_BSIZE 11 /* WQ buffer logarithmic size */

static const size_t entry_size = 65536;

#define STOP_NUMBER 5000000

#define MAX_ELEMENT_NUM 1
struct kv_element {
    uint64_t key;
    uint64_t value;
};
