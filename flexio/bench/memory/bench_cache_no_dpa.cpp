#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <utils/numautil.h>
#include <unistd.h>
#include "bench_cache_cross.h"

int main(int argc, char **argv)
{

    struct element *data;
    long *sizes;
    long *times, *bws;

    assert(sizes = (long *)malloc(timeslots * sizeof(long)));
    memset(sizes, 0x00, (timeslots * sizeof(long)));
    assert(times = (long *)malloc(timeslots * sizeof(long)));
    memset(times, 0x00, (timeslots * sizeof(long)));
    assert(bws = (long *)malloc(timeslots * sizeof(long)));
    memset(bws, 0x00, (timeslots * sizeof(long)));

    assert(data = (struct element *)malloc(memsize));
    memset(data, 0x00, memsize);

    initialize_sizes(sizes);

    std::thread t = std::thread(do_read_only, sizes, data, times, bws);
    DOCA::bind_to_core(t, 0, 1);
    t.join();
}