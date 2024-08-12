#pragma once

#include "common_cross.h"

#define LOG_SQ_RING_DEPTH 7 /* 2^7 entries, max is 2^15 */
#define LOG_RQ_RING_DEPTH 7 /* 2^7 entries, max is 2^15 */
#define LOG_CQ_RING_DEPTH 7 /* 2^7 entries, max is 2^15 */

#define LOG_WQ_DATA_ENTRY_BSIZE 11 /* WQ buffer logarithmic size */

#define MAGIC_NUMBER 0xfffffffffffffffe
