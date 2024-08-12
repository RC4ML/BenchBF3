#include "compress_bench.h"
#include "gflags_common.h"

DOCA_LOG_REGISTER(MAIN)

int main(int argc, char **argv)
{
    doca_error_t result;

    gflags::ParseCommandLineFlags(&argc, &argv, true);

    compress_config config;
    config.pci_address = FLAGS_g_pci_address;
    config.input_str = FLAGS_input_path.c_str();
    config.batch_size = FLAGS_g_batch_size;

    result = compress_bench(config, false);
    if (result != DOCA_SUCCESS)
    {
        return EXIT_FAILURE;
    }

    //    result = compress_bench(config, true);
    //    if (result != DOCA_SUCCESS) {
    //        return EXIT_FAILURE;
    //    }

    return EXIT_SUCCESS;
}