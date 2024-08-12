#pragma once

#include <common.hpp>
#include "bench.h"
#include <gflags/gflags.h>

DEFINE_uint64(loop_total, 10, "Total number of loops");

DEFINE_uint64(mem_size, 1024UL * 1024 * 1024, "host mem size per thread");

DEFINE_uint64(thread, 1, "Thread number");

//DECLARE_uint64(stride);

#define LOOP_EXPAND_SIZE 1
#define STRIDE 1
#define element size_t


#define REP2(S)        S ;        S
#define REP3(S)   REP2(S);        S
#define REP4(S)   REP2(S);   REP2(S)
#define REP8(S)   REP4(S);   REP4(S)
#define REP16(S)  REP8(S);   REP8(S)
#define REP32(S)  REP16(S);  REP16(S)
#define REP64(S)  REP32(S);  REP32(S)
#define REP128(S) REP64(S);  REP64(S)
#define REP256(S) REP128(S); REP128(S)
#define REP512(S) REP256(S); REP256(S)
#define REP1024(S) REP512(S); REP512(S)


struct thread_args {
    element *data_ptr;
    long compute_count;
};


std::atomic<bool> start_flag(false);
std::vector<double> bw_list;
std::vector<thread_args> args_list;
size_t total_dummy;

size_t timer_start() {
    struct timespec ts {};
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

size_t timer_stop() {
    struct timespec ts {};
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
}


void compute_stat(size_t count, size_t nanosec, long compute_count, size_t thread_id) {
    // printf("thread %-3ld %.4lf, %.4lf\n",
    //     thread_id,
    //     1.0 * compute_count / sizeof(element),
    //     1.0 * compute_count * count / (1.0 * nanosec));

    bw_list[thread_id] = 1.0 * compute_count * count / (1.0 * nanosec);
}

long add_read_1(size_t *data, long num_ele, size_t loop) {
    long sum = 1;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            sum += data[i];
        }
    }
    return sum;
}

long add_read_2(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum1 += tmp;
            sum2 += tmp;
        }
    }
    return sum1 + sum2;
}

long add_read_3(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum1 += tmp;
            sum2 += tmp;
            sum3 += tmp;
        }
    }
    return sum1 + sum2 + sum3;
}

long add_read_4(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum1 += tmp;
            sum2 += tmp;
            sum3 += tmp;
            sum4 += tmp;
        }
    }
    return sum1 + sum2 + sum3 + sum4;
}


long add_read_6(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum1 += tmp;
            sum2 += tmp;
            sum3 += tmp;
            sum4 += tmp;
            sum5 += tmp;
            sum6 += tmp;
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6;
}

long add_read_8(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum1 += tmp;
            sum2 += tmp;
            sum3 += tmp;
            sum4 += tmp;
            sum5 += tmp;
            sum6 += tmp;
            sum7 += tmp;
            sum8 += tmp;
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long add_read_12(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum1 += tmp;
            sum2 += tmp;
            sum3 += tmp;
            sum4 += tmp;
            sum5 += tmp;
            sum6 += tmp;
            sum7 += tmp;
            sum8 += tmp;
            sum1 += sum5;
            sum2 += sum6;
            sum3 += sum7;
            sum4 += sum8;
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long add_read_16(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP2(sum1 += sum5;
            sum2 += sum6;
            sum3 += sum7;
            sum4 += sum8;
            sum5 += sum1;
            sum6 += sum2;
            sum7 += sum3;
            sum8 += sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long add_read_24(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP3(sum1 += sum5;
            sum2 += sum6;
            sum3 += sum7;
            sum4 += sum8;
            sum5 += sum1;
            sum6 += sum2;
            sum7 += sum3;
            sum8 += sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long add_read_32(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP4(sum1 += sum5;
            sum2 += sum6;
            sum3 += sum7;
            sum4 += sum8;
            sum5 += sum1;
            sum6 += sum2;
            sum7 += sum3;
            sum8 += sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long add_read_64(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP8(sum1 += sum5;
            sum2 += sum6;
            sum3 += sum7;
            sum4 += sum8;
            sum5 += sum1;
            sum6 += sum2;
            sum7 += sum3;
            sum8 += sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long add_read_128(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP16(sum1 += sum5;
            sum2 += sum6;
            sum3 += sum7;
            sum4 += sum8;
            sum5 += sum1;
            sum6 += sum2;
            sum7 += sum3;
            sum8 += sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long add_read_256(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP32(sum1 += sum5;
            sum2 += sum6;
            sum3 += sum7;
            sum4 += sum8;
            sum5 += sum1;
            sum6 += sum2;
            sum7 += sum3;
            sum8 += sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long add_read_512(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP64(sum1 += sum5;
            sum2 += sum6;
            sum3 += sum7;
            sum4 += sum8;
            sum5 += sum1;
            sum6 += sum2;
            sum7 += sum3;
            sum8 += sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long add_read_1024(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP128(sum1 += sum5;
            sum2 += sum6;
            sum3 += sum7;
            sum4 += sum8;
            sum5 += sum1;
            sum6 += sum2;
            sum7 += sum3;
            sum8 += sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long add_read_2048(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP256(sum1 += sum5;
            sum2 += sum6;
            sum3 += sum7;
            sum4 += sum8;
            sum5 += sum1;
            sum6 += sum2;
            sum7 += sum3;
            sum8 += sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long add_512(long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            REP64(sum1 += sum5;
            sum2 += sum6;
            sum3 += sum7;
            sum4 += sum8;
            sum5 += sum1;
            sum6 += sum2;
            sum7 += sum3;
            sum8 += sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long add_1024(long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            REP128(sum1 += sum5;
            sum2 += sum6;
            sum3 += sum7;
            sum4 += sum8;
            sum5 += sum1;
            sum6 += sum2;
            sum7 += sum3;
            sum8 += sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long mul_read_1(size_t *data, long num_ele, size_t loop) {
    long sum = 1;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum *= tmp;
        }
    }
    return sum;
}

long mul_read_2(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum1 *= tmp;
            sum2 *= tmp;
        }
    }
    return sum1 + sum2;
}

long mul_read_3(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum1 *= tmp;
            sum2 *= tmp;
            sum3 *= tmp;
        }
    }
    return sum1 + sum2 + sum3;
}

long mul_read_4(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum1 *= tmp;
            sum2 *= tmp;
            sum3 *= tmp;
            sum4 *= tmp;
        }
    }
    return sum1 + sum2 + sum3 + sum4;
}

long mul_read_6(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum1 *= tmp;
            sum2 *= tmp;
            sum3 *= tmp;
            sum4 *= tmp;
            sum5 *= tmp;
            sum6 *= tmp;
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6;
}

long mul_read_8(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum1 *= tmp;
            sum2 *= tmp;
            sum3 *= tmp;
            sum4 *= tmp;
            sum5 *= tmp;
            sum6 *= tmp;
            sum7 *= tmp;
            sum8 *= tmp;
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long mul_read_12(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum1 *= tmp;
            sum2 *= tmp;
            sum3 *= tmp;
            sum4 *= tmp;
            sum5 *= tmp;
            sum6 *= tmp;
            sum7 *= tmp;
            sum8 *= tmp;
            sum1 *= sum5;
            sum2 *= sum6;
            sum3 *= sum7;
            sum4 *= sum8;
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long mul_read_16(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP2(sum1 *= sum5;
            sum2 *= sum6;
            sum3 *= sum7;
            sum4 *= sum8;
            sum5 *= sum1;
            sum6 *= sum2;
            sum7 *= sum3;
            sum8 *= sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long mul_read_24(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP3(sum1 *= sum5;
            sum2 *= sum6;
            sum3 *= sum7;
            sum4 *= sum8;
            sum5 *= sum1;
            sum6 *= sum2;
            sum7 *= sum3;
            sum8 *= sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long mul_read_32(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP4(sum1 *= sum5;
            sum2 *= sum6;
            sum3 *= sum7;
            sum4 *= sum8;
            sum5 *= sum1;
            sum6 *= sum2;
            sum7 *= sum3;
            sum8 *= sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long mul_read_64(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP8(sum1 *= sum5;
            sum2 *= sum6;
            sum3 *= sum7;
            sum4 *= sum8;
            sum5 *= sum1;
            sum6 *= sum2;
            sum7 *= sum3;
            sum8 *= sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long mul_read_128(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP16(sum1 *= sum5;
            sum2 *= sum6;
            sum3 *= sum7;
            sum4 *= sum8;
            sum5 *= sum1;
            sum6 *= sum2;
            sum7 *= sum3;
            sum8 *= sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long mul_read_256(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP32(sum1 *= sum5;
            sum2 *= sum6;
            sum3 *= sum7;
            sum4 *= sum8;
            sum5 *= sum1;
            sum6 *= sum2;
            sum7 *= sum3;
            sum8 *= sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long mul_read_512(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP64(sum1 *= sum5;
            sum2 *= sum6;
            sum3 *= sum7;
            sum4 *= sum8;
            sum5 *= sum1;
            sum6 *= sum2;
            sum7 *= sum3;
            sum8 *= sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long mul_read_1024(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP128(sum1 *= sum5;
            sum2 *= sum6;
            sum3 *= sum7;
            sum4 *= sum8;
            sum5 *= sum1;
            sum6 *= sum2;
            sum7 *= sum3;
            sum8 *= sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long mul_read_2048(size_t *data, long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            size_t tmp = data[i];
            sum8 += tmp;
            REP256(sum1 *= sum5;
            sum2 *= sum6;
            sum3 *= sum7;
            sum4 *= sum8;
            sum5 *= sum1;
            sum6 *= sum2;
            sum7 *= sum3;
            sum8 *= sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long mul_512(long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            REP64(sum1 *= sum5;
            sum2 *= sum6;
            sum3 *= sum7;
            sum4 *= sum8;
            sum5 *= sum1;
            sum6 *= sum2;
            sum7 *= sum3;
            sum8 *= sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long mul_1024(long num_ele, size_t loop) {
    long sum1 = 1, sum2 = 2, sum3 = 3, sum4 = 4, sum5 = 5, sum6 = 6, sum7 = 7, sum8 = 8;
    while (loop--) {
        for (long i = 0; i < num_ele; i++) {
            REP128(sum1 *= sum5;
            sum2 *= sum6;
            sum3 *= sum7;
            sum4 *= sum8;
            sum5 *= sum1;
            sum6 *= sum2;
            sum7 *= sum3;
            sum8 *= sum4;
            );
        }
    }
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}

long roofline_sizet_add_read(size_t *data, long num_ele, long compute_count, size_t loop) {
    long sum = 0;
    switch (compute_count) {
    case 1:
        sum += add_read_1(data, num_ele, loop);
        break;
    case 2:
        sum += add_read_2(data, num_ele, loop);
        break;
    case 3:
        sum += add_read_3(data, num_ele, loop);
        break;
    case 4:
        sum += add_read_4(data, num_ele, loop);
        break;
    case 6:
        sum += add_read_6(data, num_ele, loop);
        break;
    case 8:
        sum += add_read_8(data, num_ele, loop);
        break;
    case 12:
        sum += add_read_12(data, num_ele, loop);
        break;
    case 16:
        sum += add_read_16(data, num_ele, loop);
        break;
    case 24:
        sum += add_read_24(data, num_ele, loop);
        break;
    case 32:
        sum += add_read_32(data, num_ele, loop);
        break;
    case 64:
        sum += add_read_64(data, num_ele, loop);
        break;
    case 128:
        sum += add_read_128(data, num_ele, loop);
        break;
    case 256:
        sum += add_read_256(data, num_ele, loop);
        break;
    case 512:
        sum += add_read_512(data, num_ele, loop);
        break;
    case 1024:
        sum += add_read_1024(data, num_ele, loop);
        break;
    case 2048:
        sum += add_read_2048(data, num_ele, loop);
        break;
    case 511:
        sum += add_512(num_ele, loop);
        break;
    case 1023:
        sum += add_1024(num_ele, loop);
        break;
    default:
        break;
    }

    return sum;
}

long roofline_sizet_mul_read(size_t *data, long num_ele, long compute_count, size_t loop) {
    long sum = 1;
    switch (compute_count) {
    case 1:
        sum += mul_read_1(data, num_ele, loop);
        break;
    case 2:
        sum += mul_read_2(data, num_ele, loop);
        break;
    case 3:
        sum += mul_read_3(data, num_ele, loop);
        break;
    case 4:
        sum += mul_read_4(data, num_ele, loop);
        break;
    case 6:
        sum += mul_read_6(data, num_ele, loop);
        break;
    case 8:
        sum += mul_read_8(data, num_ele, loop);
        break;
    case 12:
        sum += mul_read_12(data, num_ele, loop);
        break;
    case 16:
        sum += mul_read_16(data, num_ele, loop);
        break;
    case 24:
        sum += mul_read_24(data, num_ele, loop);
        break;
    case 32:
        sum += mul_read_32(data, num_ele, loop);
        break;
    case 64:
        sum += mul_read_64(data, num_ele, loop);
        break;
    case 128:
        sum += mul_read_128(data, num_ele, loop);
        break;
    case 256:
        sum += mul_read_256(data, num_ele, loop);
        break;
    case 512:
        sum += mul_read_512(data, num_ele, loop);
        break;
    case 1024:
        sum += mul_read_1024(data, num_ele, loop);
        break;
    case 2048:
        sum += mul_read_2048(data, num_ele, loop);
        break;
    case 511:
        sum += mul_512(num_ele, loop);
        break;
    case 1023:
        sum += mul_1024(num_ele, loop);
        break;
    default:
        break;
    }

    return sum;
}


void roofline_loop(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat, void *args) {
    (void)args;
    thread_args *arg = &args_list[thread_id];

    element *data = arg->data_ptr + thread_id * FLAGS_mem_size / sizeof(element);
    _unused(runner);
    _unused(stat);
    while (!start_flag) {
        usleep(1);
    }
    size_t begin_time = timer_start();
    size_t dummy = roofline_sizet_add_read(data, FLAGS_mem_size / sizeof(element), arg->compute_count, FLAGS_loop_total);
    begin_time = timer_stop() - begin_time;
    total_dummy += dummy;
    compute_stat(FLAGS_loop_total * FLAGS_mem_size / sizeof(element), begin_time, arg->compute_count, thread_id);
}
