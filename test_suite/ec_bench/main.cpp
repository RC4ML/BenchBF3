#include "ec_bench.h"
#include "gflags_common.h"
#include "utils_common.h"


DOCA_LOG_REGISTER(MAIN_BENCH);

int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    ec_config config;
    config.pci_address = FLAGS_g_pci_address;
    config.size = FLAGS_g_payload;
    config.blocks = FLAGS_blocks;
    config.k = FLAGS_k;
    config.m = FLAGS_m;
    config.batch_size = FLAGS_g_batch_size;

    if (config.k <= 0 || config.size <= 0 || config.m <= 0) {
        DOCA_LOG_ERR("invalid size, exit");
        exit(1);
    }

    config.each = (config.size + config.k - 1) / config.k;
    config.each = (config.each + PADDING - 1) / PADDING * PADDING;

    config.ec_obj.ec = new DOCA::ec();
    config.ec_obj.encoding_matrix = new DOCA::matrix(config.ec_obj.ec, config.k, config.m);

    config.data.resize(config.blocks);
    config.data_size = (config.each * (config.k + config.m)) * config.blocks;
    config.k_data.resize(config.blocks);
    config.k_data_size = config.each * config.k * config.blocks;
    config.m_data.resize(config.blocks);
    config.m_data_size = config.each * config.m * config.blocks;

    char *total = static_cast<char *>(malloc(config.data_size));
    char *tmp_now = total;

    for (size_t i = 0; i < config.blocks; i++) {
        for (size_t j = 0; j < (config.k + config.m); j++) {
            config.data[i].push_back(tmp_now);
            tmp_now += config.each;
        }
    }

    total = static_cast<char *>(malloc(config.k_data_size));
    tmp_now = total;
    for (size_t i = 0; i < config.blocks; i++) {
        for (size_t j = 0; j < config.k; j++) {
            config.k_data[i].push_back(tmp_now);
            tmp_now += config.each;
        }
    }

    total = static_cast<char *>(malloc(config.m_data_size));
    tmp_now = total;
    for (size_t i = 0; i < config.blocks; i++) {
        for (size_t j = 0; j < config.m; j++) {
            config.m_data[i].push_back(tmp_now);
            tmp_now += config.each;
        }
    }

    auto runner_enconding = DOCA::bench_runner(FLAGS_g_thread_num);
    runner_enconding.run(ec_bench_encoding, static_cast<void *>(&config));

    sleep(FLAGS_g_life_time);

    runner_enconding.stop();


    auto runner_decoding = DOCA::bench_runner(FLAGS_g_thread_num);
    runner_decoding.run(ec_bench_decoding, static_cast<void *>(&config));
    sleep(FLAGS_g_life_time);

    runner_decoding.stop();

    free(config.data[0][0]);
    free(config.k_data[0][0]);
    free(config.m_data[0][0]);

    return EXIT_SUCCESS;
}