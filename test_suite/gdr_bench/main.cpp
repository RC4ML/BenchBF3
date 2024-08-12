#include "gdr_bench.h"
#include "gflags_common.h"


/**
 * exporter(host): sudo ../bin_host/gdr_bench --g_is_exported --g_network_address 172.25.2.131:1234 -g_pci_address 40:00.0 --g_life_time 100 -g_mmap_length 134217728
 * importer(dpu): sudo ../bin_dpu/gdr_bench  --g_network_address 172.25.2.131:1234 -g_pci_address 03:00.0 --g_life_time 100
*/
int main(int argc, char **argv) {

    gflags::ParseCommandLineFlags(&argc, &argv, true);

    gdr_config config;
    config.network_addrss = FLAGS_g_network_address;
    config.pci_address = FLAGS_g_pci_address;
    config.is_read = FLAGS_g_is_read;
    config.is_export = FLAGS_g_is_exported;
    config.thread_count = FLAGS_g_thread_num;
    config.payload = FLAGS_g_payload;
    config.batch_size = FLAGS_g_batch_size;
    config.mmap_length = FLAGS_g_mmap_length;
    config.life_time = FLAGS_g_life_time;

    if (config.is_export) {
        bootstrap_exporter(config);
    } else {
        bootstrap_importer(config);
    }
}