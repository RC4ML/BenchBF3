#pragma once

#include <common.hpp>
#include "bench.h"
#include <gflags/gflags.h>

DEFINE_uint64(loop_total, 10, "Total number of loops");

DEFINE_uint64(elem, 100000000, "Element number for one loop");

DEFINE_uint64(thread, 1, "Thread number");

//DECLARE_uint64(stride);

#define STRIDE 1
#define element size_t

std::atomic<bool> start_flag(false);
std::vector<double> bw_list;

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


void compute_stat(size_t count, size_t nanosec, size_t thread_id) {
    printf("thread %-3ld KBytes:%-15ld %-15d %-15ld %-15ld\n",
        thread_id,
        FLAGS_thread * FLAGS_elem * sizeof(element) / 1024,
        STRIDE,
        nanosec * 100 / count,
        count * sizeof(element) * 1000 / (nanosec));
    bw_list[thread_id] = (count * sizeof(element) * 1000.0) / (nanosec) / 1.024 / 1.024;
}

uint64_t benchmark_memory_ronly_expand(element *data, uint64_t num_elem) {
    uint64_t loop_num = 0;
    element *p = data;
    uint64_t dummy0 = 0;
    size_t begin_time = timer_start();

    while (loop_num < FLAGS_loop_total) {
        for (uint64_t i = 0; i < num_elem; i += STRIDE) {
            dummy0 += p[i];
        }
        loop_num++;
    }
    begin_time = timer_stop() - begin_time;

    printf("%ld\n", dummy0);
    return begin_time;
}

uint64_t benchmark_memory_wonly_expand(element *data, uint64_t num_elem) {
    uint64_t loop_num = 0;
    element *p = data;
    size_t begin_time = timer_start();

    while (loop_num < FLAGS_loop_total) {
        for (uint64_t i = 0; i < num_elem; i += STRIDE) {
            p[i] = i;
        }
        loop_num++;
    }
    begin_time = timer_stop() - begin_time;

    return begin_time;
}




void benchmark_memory_ronly_loop(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat, void *args) {

    element *data = static_cast<element *>(args) + thread_id * FLAGS_elem;
    _unused(runner);
    _unused(stat);
    while (!start_flag) {
        usleep(1);
    }

    size_t time_total = benchmark_memory_ronly_expand(data, FLAGS_elem);

    compute_stat(FLAGS_loop_total * FLAGS_elem, time_total, thread_id);
}

void benchmark_memory_wonly_loop(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat, void *args) {

    element *data = static_cast<element *>(args) + thread_id * FLAGS_elem;
    _unused(runner);
    _unused(stat);
    while (!start_flag) {
        usleep(1);
    }

    size_t time_total = benchmark_memory_wonly_expand(data, FLAGS_elem);

    compute_stat(FLAGS_loop_total * FLAGS_elem, time_total, thread_id);
}


void benchmark_memory_memcpy_loop(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat, void *args) {
    element *data = static_cast<element *>(args) + thread_id * FLAGS_elem;
    _unused(runner);
    _unused(stat);

    element *dst = data + FLAGS_elem / 2;

    while (!start_flag) {
        usleep(1);
    }
    uint64_t loop_num = 0;
    size_t begin_time = timer_start();
    data[0] = 0;
    while (loop_num < FLAGS_loop_total) {
        memcpy(dst + data[0], data, sizeof(element) * FLAGS_elem / 2);
        loop_num++;
    }

    begin_time = timer_stop() - begin_time;
    compute_stat(FLAGS_loop_total * FLAGS_elem / 2, begin_time, thread_id);
}


#if defined(__x86_64__)
#include <immintrin.h>
uint64_t benchmark_cache_ronly_expand(element *data, uint64_t num_elem) {
    uint64_t loop_num = 0;
    element *p = data;
    size_t begin_time = timer_start();
    while (loop_num < FLAGS_loop_total) {
        p = data;
        for (uint64_t i = 0; i < num_elem; i += 8, p += 8) {
            __m512 vec;
            // why can't use vmovdqa64? we already aligned memory 
            asm volatile (
                "vmovdqu64 %[data], %%zmm0"  // 使用vmovdqu64指令加载数据
                : [vec] "=m" (vec)
                : [data] "m" (p)
                );
        }
        loop_num++;
    }
    begin_time = timer_stop() - begin_time;

    return begin_time;
}

void benchmark_cache_ronly_loop(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat, void *args) {

    element *data = static_cast<element *>(args) + thread_id * FLAGS_elem;
    _unused(runner);
    _unused(stat);
    while (!start_flag) {
        usleep(1);
    }

    size_t time_total = benchmark_cache_ronly_expand(data, FLAGS_elem);

    compute_stat(FLAGS_loop_total * FLAGS_elem, time_total, thread_id);
}
#endif
