#include "rdma_bench.h"
#include "gflags_common.h"


/**
 * sender(host1): sudo numactl -N 1 -m 1 ../bin_host/rdma_send_bench -g_network_address 172.25.2.24:1234 -g_ibdev_name mlx5_1 -gid_index 3 -g_payload 1024 -g_batch_size 64 -g_life_time 60   -g_is_exported -g_thread_num 4 -bind_offset 0 -bind_numa 1
 * receiver(host2): sudo numactl -N 1 -m 1 ../bin_host/rdma_send_bench -g_network_address 172.25.2.23:1234 -gid_index 3 -g_payload 1024 -g_batch_size 128 -g_life_time 60 -g_ibdev_name mlx5_1 -g_thread_num 10 -bind_numa 1
*/
int main(int argc, char **argv) {

    gflags::ParseCommandLineFlags(&argc, &argv, true);

    rdma_config config;
    config.network_addrss = FLAGS_g_network_address;
    config.ibdev_name = FLAGS_g_ibdev_name;
    config.is_sender = FLAGS_g_is_exported;
    config.thread_count = FLAGS_g_thread_num;
    config.payload = FLAGS_g_payload;
    config.batch_size = FLAGS_g_batch_size;
    config.life_time = FLAGS_g_life_time;
    config.gid_index = FLAGS_gid_index;
    if (config.gid_index == UINT32_MAX) {
        printf("GID index is not set, exit!\n");
        return 1;
    }
    config.bind_offset = FLAGS_bind_offset;
    config.bind_numa = FLAGS_bind_numa;

    if (config.is_sender) {
        bootstrap_sender(config);
    } else {
        bootstrap_receiver(config);
    }
}