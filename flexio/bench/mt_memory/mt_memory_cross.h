#pragma once

#include "wrapper_flexio/common_cross.h"

#if defined(__x86_64__) || defined(__aarch64__)

#elif defined(__riscv)

#endif


#define	HOST_MEM 0
#define PRIVATE_MEM 1

const char* MemTypeStr[] = {
	"HOST_MEM",
	"PRIVATE_MEM"
};

#define EXPAND_SIZE 1

struct MemoryArg{
    int stream_id;
	uint32_t window_id;
	uint32_t mkey;
	int mem_type;
	size_t size;
	int stride;
	size_t loop;

	void* haddr;
	DevP(uint64_t) ddata;
	
};
