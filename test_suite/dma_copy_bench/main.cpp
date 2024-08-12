#include "dma_copy_bench.h"
#include "gflags_common.h"

/**
 * host importer: ../bin_host/dma_copy_bench  --g_network_address 172.25.2.23:1234  -g_pci_address c1:00.0 --g_life_time 15 -g_payload 1024 -g_batch_size 32 -bind_offset 1 -bind_numa 1
 * dpu expoter:   ../bin_dpu/dma_copy_bench --g_is_exported --g_network_address 172.25.2.23:1234 -g_pci_address 03:00.0 --g_life_time 100 -g_mmap_length 1048576 --bind_numa 0
 *
 * host exporter: ../bin_host/dma_copy_bench  --g_is_exported --g_network_address 172.25.2.133:1234 -g_pci_address c1:00.0 --g_life_time 100 -g_mmap_length 1048576 --bind_numa 1
 * dpu importer:  ../bin_dpu/dma_copy_bench  --g_network_address 172.25.2.133:1234  -g_pci_address 03:00.0 --g_life_time 15 -g_payload 1024 -g_batch_size 32 -bind_offset 1 -bind_numa 0
*/
int main(int argc, char **argv) {
    signal(SIGINT, ctrl_c_handler);
    signal(SIGTERM, ctrl_c_handler);
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    dma_config config;
    config.network_addrss = FLAGS_g_network_address;
    config.pci_address = FLAGS_g_pci_address;
    config.is_read = FLAGS_g_is_read;
    config.is_export = FLAGS_g_is_exported;
    config.thread_count = FLAGS_g_thread_num;
    config.payload = FLAGS_g_payload;
    config.batch_size = FLAGS_g_batch_size;
    config.mmap_length = FLAGS_g_mmap_length;
    config.life_time = FLAGS_g_life_time;
    config.bind_offset = FLAGS_bind_offset;
    config.bind_numa = FLAGS_bind_numa;

    if (config.is_export) {
        bootstrap_exporter(config);
    } else {
        bootstrap_importer(config);
    }
}