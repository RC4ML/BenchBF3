#pragma once

#include "wrapper_flexio/common_cross.h"

#define	COPY_FROM_HOST 0
#define POSIX_MEM_ALIGN 1

const char* MemTypeStr[] = {
	"COPY_FROM_HOST",
	"POSIX_MEM_ALIGN"
};

#define MEMORY_TYPE COPY_FROM_HOST

#define LOOPS 1
#define STRIDE 16
#define EXPAND_SIZE 1

size_t THREAD_MEM_SIZE = 1024UL*1024*256;

struct MemoryArg
{
    int stream_id;
    uint32_t window_id;
    uint32_t mkey;
    void *haddr;

	DevP(uint64_t) data;
};
