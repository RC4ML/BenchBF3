#include "gdr_bench.h"
#include "bench.h"
#include "reporter.h"
#include "udp_client.h"

DOCA_LOG_REGISTER(EXPORTER);

#if defined(__x86_64__)
#include "cuda.h"
#define CUCHECK(stmt) \
	do { \
	CUresult result = (stmt); \
	DOCA::rt_assert(CUDA_SUCCESS == result); \
} while (0)

static CUcontext init_gpu(std::string gpu_device_pci_addr) {
    CUCHECK(cuInit(0));

    CUdevice gpu_dev;
    CUCHECK(cuDeviceGetByPCIBusId(&gpu_dev, gpu_device_pci_addr.c_str()));

    char name[128];
    CUCHECK(cuDeviceGetName(name, sizeof(name), gpu_dev));
    printf("[pid = %d, dev = %d] device name = [%s]\n", getpid(), gpu_dev, name);
    printf("creating CUDA Ctx\n");

    CUcontext dev_ctx;
    CUCHECK(cuDevicePrimaryCtxRetain(&dev_ctx, gpu_dev));
    CUCHECK(cuCtxSetCurrent(dev_ctx));
    return dev_ctx;
}


void perform_exporter_routine(uint64_t thread_id, DOCA::bench_runner *runner, DOCA::bench_stat *stat, void *args) {
    _unused(stat);
    CUcontext gpu_ctx = init_gpu("3f:00.0");
    if (thread_id > 0) {
        // just use one buffer for mmap
        return;
    }
    auto *config = static_cast<gdr_config *>(args);

    CUdeviceptr d_A;
    CUCHECK(cuMemAlloc(&d_A, config->mmap_length));

    char *src_buffer = reinterpret_cast<char *>(d_A);

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

    while (runner->running()) {
        usleep(100);
    }

    cuMemFree(d_A);
    printf("free CUDA memory\n");
    cuCtxDestroy(gpu_ctx);
    delete src_mmap;
    delete dev;
}

void bootstrap_exporter(gdr_config &config) {
    auto runner = DOCA::bench_runner(config.thread_count);

    runner.run(perform_exporter_routine, static_cast<void *>(&config));

    sleep(config.life_time);

    runner.stop();
}
#else
void bootstrap_exporter(gdr_config &config) {
    (void)config;
    DOCA_LOG_ERR("This test is only supported on x86_64");
}
#endif


