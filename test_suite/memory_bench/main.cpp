#include "memory_bench.h"


int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    element *data = nullptr;

    size_t memsize = sizeof(element) * FLAGS_elem * FLAGS_thread;
    assert(posix_memalign(reinterpret_cast<void **>(&data), 64, memsize) == 0);
    memset(data, 0x00, memsize);
    for (size_t i = 0; i < memsize / sizeof(element); i++) {
        data[i] = 0;
    }
    bw_list.resize(FLAGS_thread);


    auto runner = DOCA::bench_runner(FLAGS_thread);

    runner.run(benchmark_memory_ronly_loop, static_cast<void *>(data));

    sleep(1);
    start_flag = true;

    runner.stop();

    printf("Total sum %-15f MB/s\n", std::accumulate(bw_list.begin(), bw_list.end(), 0.0));
}