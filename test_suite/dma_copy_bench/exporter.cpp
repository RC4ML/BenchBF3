#include "dma_copy_bench.h"
#include "bench.h"
#include "reporter.h"
#include "udp_client.h"

DOCA_LOG_REGISTER(EXPORTER);

void perform_exporter_routine(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat, void *args) {
    _unused(stat);
    if (thread_id > 0) {
        // just use one buffer for mmap
        return;
    }
    auto *config = static_cast<dma_config *>(args);

    char *src_buffer = reinterpret_cast<char *>(DOCA::get_huge_mem(config->bind_numa, config->mmap_length));
    memset(src_buffer, kMagic, config->mmap_length);

    DOCA::dev *dev = DOCA::open_doca_device_with_pci(config->pci_address.c_str(), nullptr);

    auto *src_mmap = new DOCA::mmap(true);
    src_mmap->add_device(dev);
    src_mmap->set_permissions(DOCA_ACCESS_FLAG_PCI_READ_WRITE);
    src_mmap->set_memrange_and_start(src_buffer, config->mmap_length);

    doca_conn_info conn_info{};
    conn_info.exports = src_mmap->export_dpus();
    conn_info.buffers.emplace_back(std::bit_cast<uint64_t>(src_buffer), config->mmap_length);
    DOCA_LOG_INFO("addr %p, sie %d", static_cast<void *>(src_buffer), config->mmap_length);

    DOCA::UDPClient<dma_connect::doca_conn_info> client;
    auto pos = std::find(config->network_addrss.begin(), config->network_addrss.end(), ':');
    std::string port_str = config->network_addrss.substr(pos - config->network_addrss.begin() + 1);
    std::string ip = config->network_addrss.substr(0, pos - config->network_addrss.begin());
    uint16_t port = std::stoi(port_str);

    client.send(ip + ":" + std::to_string(port + thread_id), doca_conn_info::to_protobuf(conn_info));

    while (runner->running() && stop_flag == false) {
        usleep(100);
    }

    delete src_mmap;
    delete dev;
}

void bootstrap_exporter(dma_config &config) {

    auto runner = DOCA::bench_runner(config.thread_count, config.bind_offset, config.bind_numa);

    runner.run(perform_exporter_routine, static_cast<void *>(&config));

    sleep(config.life_time);

    runner.stop();
}