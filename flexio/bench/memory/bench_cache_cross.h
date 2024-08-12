#pragma once

#include "wrapper_flexio/common_cross.h"

#if defined(__x86_64__) || defined(__aarch64__)
#include <time.h>
#include <stdint.h>
#elif defined(__riscv)

#endif

struct MemoryArg {
    int stream_id;
    uint32_t window_id;
    uint32_t mkey;
    void *haddr;

    DevP(void) sizes;
    DevP(void) times;
    DevP(void) bws;
    DevP(struct element) data;
    DevP(size_t) cycles;
    long index;
};


#define	COPY_FROM_HOST 0
#define POSIX_MEM_ALIGN 1

const char *MemTypeStr[] = {
    "COPY_FROM_HOST",
    "POSIX_MEM_ALIGN"
};

#define MEMORY_TYPE COPY_FROM_HOST

#if defined(RUN_IN_DPA) || defined(__riscv)
#define LOOP_TOTAL 10000000
#define CACHE_MIN_BITS (8)
#define CACHE_MAX_BITS (28)//256MB
#define PADDING_SIZE 7L
#elif defined(__x86_64__) || defined(__aarch64__)
#define LOOP_TOTAL 100000000
#define CACHE_MIN_BITS (11)//2KB
#define CACHE_MAX_BITS (30)//1GB
#define PADDING_SIZE 0L
#endif

#define READONLY (1 << 0)
#define WRITEONLY (1 << 1)
#define NONPCHASE (1 << 2)
#define PCHASE (1 << 3)

#define CACHE_MIN (1L << CACHE_MIN_BITS)
#define CACHE_MAX (1L << CACHE_MAX_BITS)

#define REPEAT_COUNT (2)
#define RESOLUTION (2)


#if defined(__x86_64__) || defined(__aarch64__)
#elif defined(__riscv)
#pragma clang diagnostic ignored "-Wzero-length-array"
#endif
struct element {
    struct element *next;
    size_t padding[PADDING_SIZE];
};

static long  memsize = 1 << CACHE_MAX_BITS;

static long repeat_count = REPEAT_COUNT;
static long resolution = RESOLUTION;

static long timeslots = RESOLUTION * (CACHE_MAX_BITS - CACHE_MIN_BITS) + 1;




static size_t t1, t2;

void timer_start(void) {
#if defined(__x86_64__) || defined(__aarch64__)
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    t1 = (long)ts.tv_sec * 1000000000.0 + (long)ts.tv_nsec;
    //    printf("%ld %ld %ld\n", ts.tv_sec, ts.tv_nsec, t1);

#elif defined(__riscv)
    t1 = __dpa_thread_cycles();
#endif
}

void timer_stop(void) {
#if defined(__x86_64__) || defined(__aarch64__)
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    t2 = (long)ts.tv_sec * 1000000000.0 + (long)ts.tv_nsec;
    //    printf("%ld %ld %ld\n", ts.tv_sec, ts.tv_nsec, t2);
#elif defined(__riscv)
    t2 = __dpa_thread_cycles();
#endif
}

long timer_elapsed(void) {
#if defined(__x86_64__) || defined(__aarch64__)
    // printf("%ld %ld %ld\n", t2, t1, t2 - t1);
    return (t2 - t1);
#elif defined(__riscv)

    size_t total_cycles = 0;
    if (t1 < t2) {
        total_cycles += (t2 - t1);
    } else {
        total_cycles += ((1UL << 32) - 1 + t2 - t1);
    }
    return total_cycles * 10 / 18;//1.8GHz 
#endif
}

int is_space(char ch) {
    return (ch == ' ' || ch == '\t' || ch == '\n' ||
        ch == '\r' || ch == '\v' || ch == '\f');
}

// Custom function to check if a char is a digit
int is_digit(char ch) {
    return (ch >= '0' && ch <= '9');
}

long atol(char *str) {
    long res = 0;
    int sign = 1;

    // Check for valid input
    if (!*str) {
        LOG_ERROR("str is null");
        return 0;
    }

    // Skip any leading white space
    while (is_space(*str))
        ++str;

    // Check for sign
    if (*str == '-' || *str == '+') {
        sign = 1 - 2 * (*str++ == '-');
    }

    // Calculate number
    while (is_digit(*str)) {
        res = 10 * res + *str++ - '0';
    }

    // Return result with sign
    return sign * res;
}

void initialize_sizes(long *sizes) {
    long i, j;

    for (j = 0; j < timeslots; j += resolution) {
        sizes[j] = 1L << (CACHE_MIN_BITS + j / resolution);
        for (i = 1; i < resolution; i++) {
            if (j + i < timeslots) {
                sizes[j + i] = sizes[j] + i * (sizes[j] / resolution);
                sizes[j + i] = sizes[j + i] - sizes[j + i] % (long)sizeof(struct element);
            }
        }
    }
}

uint64_t benchmark_cache_ronly_p(struct element *data, long *times, long *count) {
    long loop_total = LOOP_TOTAL;
    long loop_num = 0;
    struct element *p = data;
    timer_start();
    while (loop_num < loop_total) {
        p = p->next;
        loop_num++;
    }
    timer_stop();

    *times = timer_elapsed();
    *count = loop_num;
    return (uint64_t)p;
}

void init_pchase(struct element *data, long num_elem) {
    for (long i = 0; i < num_elem; i++) {
        data[i].next = &data[(i + 1) % num_elem];
        for (int j = 0;j < PADDING_SIZE;j++) {
            data[i].padding[j] = 0;
        }
    }
}

void compute_stat(long timeslot, long *times, long *bws, long *sizes, long count, long nanosec) {

    times[timeslot] = nanosec * 100 / count;
    bws[timeslot] =
        count * sizeof(struct element) * 1000 / (nanosec);
#if defined(__x86_64__) || defined(__aarch64__)
    LOG_INFO("KBytes:%-15ld %-15ld %-15ld %-15ld\n",
        sizes[timeslot] / 1024,
        PADDING_SIZE,
        times[timeslot],
        bws[timeslot]);
#elif defined(__riscv)
    LOG_INFO("Bytes:%-15ld %-15ld %-15ld %-15ld\n",
        sizes[timeslot],
        PADDING_SIZE,
        times[timeslot],
        bws[timeslot]);
#endif
}

void do_read_only(long *sizes, struct element *data, long *times, long *bws) {
    LOG_INFO("C Size\t\tPADDING_SIZE\t\tNanosec/100\t\tMB/sec\t\t\n");
    LOG_INFO("-------\t\t------\t\t------\t\t-------\t\t\n");


#if defined(__x86_64__) || defined(__aarch64__)
    usleep(1000);
#elif defined(__riscv)
#endif
    long num_elem, k, i, tcount, tnanosec;
    long count, nanosec;
    uint64_t result = 0;

    for (i = 0; i < timeslots; i++) {
        num_elem = sizes[i] / (long)sizeof(struct element);
        tcount = 0, tnanosec = 0;
        for (k = 0; k < repeat_count; k++) {
            init_pchase(data, num_elem);
            result += benchmark_cache_ronly_p(data, &nanosec, &count);
            tcount += count;
            tnanosec += nanosec;
        }
        compute_stat(i, times, bws, sizes, tcount, tnanosec);
    }
    LOG_INFO("%lx\n", result);
    LOG_I("End of read only bench\n");

}
#if defined(__riscv)

void bucket_cycles_count(size_t *data, size_t num, size_t *buckets, size_t num_buckets) {
    for (size_t i = 0;i < num_buckets + 1;i++) {
        buckets[i] = 0;//size must be num_buckets+1
    }
    size_t min = 1UL << 32;
    size_t max = 0;
    for (size_t i = 0;i < num;i++) {
        if (data[i] > max) {
            max = data[i];
        }
        if (data[i] < min) {
            min = data[i];
        }
    }
    int range = (max - min + 1);
    int step = (range - (range % num_buckets)) / num_buckets;

    for (size_t i = 0;i < num;i++) {
        for (size_t j = 0;j < num_buckets;j++) {
            if (min + j * step <= data[i]) {
                if (min + (j + 1) * step > data[i]) {
                    buckets[j] += 1;
                }
            }
        }
        if (min + step * num_buckets <= data[i]) {
            buckets[num_buckets] += 1;
        }
    }

    size_t sum = 0;
    for (size_t i = 0;i < num_buckets + 1;i++) {
        sum += buckets[i];
    }

    if (sum != num) {
        LOG_E("Total number mismatch!\n");
    }

    LOG_I("Max: %ld, Min: %ld\n", max, min);
    for (size_t i = 0;i < num_buckets + 1;i++) {
        LOG_F(1, "Range %ld-%ld : %ld\n", min + i * step, min + (i + 1) * step, buckets[i]);
    }
}
#define NUM_BUCKET 10
void statistics(size_t *cycles) {
    size_t buckets[NUM_BUCKET + 1];
    bucket_cycles_count(cycles, LOOP_TOTAL, buckets, NUM_BUCKET);
}
#endif
