#include "roofline.h"


int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    element *data;

    size_t memsize = sizeof(element) * FLAGS_mem_size / sizeof(element) * FLAGS_thread;
    assert(data = static_cast<element *>(malloc(FLAGS_mem_size * FLAGS_thread)));
    memset(data, 0x00, memsize);
    for (size_t i = 0; i < memsize / sizeof(element); i++) {
        data[i] = i;
    }
    bw_list.resize(FLAGS_thread);
    args_list.resize(FLAGS_thread);

    std::vector<long>compute_counts = { 1,2,3,4,6,8,12,16,24,32,64,128,256,512,1024,2048 };
    // std::vector<long>compute_counts = { 511,1023 };
    for (auto compute_count : compute_counts) {
        for (size_t i = 0;i < FLAGS_thread;i++) {
            args_list[i].data_ptr = data;
            args_list[i].compute_count = compute_count;
        }
        auto runner = DOCA::bench_runner(FLAGS_thread);
        runner.run(roofline_loop, nullptr);
        sleep(1);
        start_flag = true;

        runner.stop();
        start_flag = false;

        printf("%.3lf, %.5lf\n", 1.0 * compute_count / sizeof(element), std::accumulate(bw_list.begin(), bw_list.end(), 0.0));
    }
    printf("dummy %ld\n", total_dummy);
}