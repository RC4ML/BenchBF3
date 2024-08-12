#include "channel_bench.h"
#include "gflags_common.h"

int main(int argc, char **argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    channel_config config;
    config.rep_pci_addrss = FLAGS_g_rep_pci_address;
    config.pci_address = FLAGS_g_pci_address;
    config.is_server = FLAGS_g_is_server;
    config.is_sender = FLAGS_g_is_exported;
    config.is_read = FLAGS_g_is_read;
    config.thread_count = FLAGS_g_thread_num;
    config.payload = FLAGS_g_payload;
    config.batch_size = FLAGS_g_batch_size;
    config.mmap_length = FLAGS_g_mmap_length;
    config.life_time = FLAGS_g_life_time;


    if (config.is_server) {
        bootstrap_server(config);
    } else {
        bootstrap_client(config);
    }
}