#include "regex_bench.h"
#include "gflags_common.h"
#include "utils_common.h"

int main(int argc, char **argv) {

    doca_error_t result;
    /* Create a logger backend that prints to the standard output */

    gflags::ParseCommandLineFlags(&argc, &argv, true);

    regex_config config;
    config.pci_address = FLAGS_g_pci_address;
    config.input_str = get_file_contents(FLAGS_input_path.c_str());
    config.rule_str = get_file_contents(FLAGS_rule_path.c_str());
    config.batch_size = FLAGS_g_batch_size;

    result = regex_bench(config);
    if (result != DOCA_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}