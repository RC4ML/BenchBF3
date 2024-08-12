#include "rdma_bench.h"

DEFINE_uint32(gid_index, UINT32_MAX, "gid index for rdma");

DEFINE_uint32(bind_offset, 0, "bind to offset core and thread ID");

DEFINE_uint32(bind_numa, 0, "bind to numa node");

std::string receive_and_send_rdma_param(uint64_t thread_id, std::string ip_and_port, std::string param) {
    auto pos = std::find(ip_and_port.begin(), ip_and_port.end(), ':');
    std::string port_str = ip_and_port.substr(pos - ip_and_port.begin() + 1);
    std::string ip = ip_and_port.substr(0, pos - ip_and_port.begin());
    uint16_t port = std::stoi(port_str);
    doca_conn_info receive_info;

    std::atomic<bool> done = false;
    std::thread udp_server_thread = std::thread([port, thread_id, &receive_info, &done]() {
        DOCA::UDPServer<dma_connect::doca_conn_info> server(port + thread_id);
        dma_connect::doca_conn_info receive_info_raw;

        server.recv_blocking(receive_info_raw);
        receive_info = doca_conn_info::from_protobuf(receive_info_raw);
        sleep(1);
        done = true;
        });

    DOCA::UDPClient<dma_connect::doca_conn_info> client;
    doca_conn_info send_info{};
    send_info.exports.push_back(param);
    while (!done) {
        client.send(ip + ":" + std::to_string(port + thread_id), doca_conn_info::to_protobuf(send_info));
        usleep(300000);
    }
    udp_server_thread.join();
    DOCA::rt_assert(receive_info.exports.size() == 1, "receive_info.exports.size() != 1");
    return receive_info.exports[0];
}

