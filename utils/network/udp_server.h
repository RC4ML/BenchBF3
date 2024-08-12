#pragma once

#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

#include "common.hpp"

namespace DOCA {
    template<class T>
    class UDPServer {
    public:
        explicit UDPServer(std::string ip_and_port) {
            auto pos = std::find(ip_and_port.begin(), ip_and_port.end(), ':');
            std::string ip = ip_and_port.substr(0, pos - ip_and_port.begin());
            std::string port_str = ip_and_port.substr(pos - ip_and_port.begin() + 1);
            uint16_t port = std::stoi(port_str);

            socket_ = std::make_unique<asio::ip::udp::socket>(io_context_, asio::ip::udp::endpoint(asio::ip::address::from_string(ip), port));
        }
        explicit UDPServer(uint16_t port): socket_(
            new asio::ip::udp::socket(io_context_, asio::ip::udp::endpoint(asio::ip::udp::v4(), port))) {
        }

        UDPServer(const UDPServer &) = delete;

        ~UDPServer() = default;

        size_t recv_blocking(T &msg) {
            char recv_buffer[2048];
            size_t bytes_recv = socket_->receive(asio::buffer(recv_buffer));
            msg.ParseFromArray(recv_buffer, bytes_recv);
            return bytes_recv;
        }

    private:
        asio::io_context io_context_;
        std::unique_ptr<asio::ip::udp::socket> socket_;
    };
}