#pragma once

#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

#include <memory>
#include "common.hpp"

namespace DOCA {
    template<class T>
    class UDPClient {
    public:
        UDPClient() : resolver_(new asio::ip::udp::resolver(io_context_)),
                      socket_(new asio::ip::udp::socket(io_context_)) {
            socket_->open(asio::ip::udp::v4());
        }

        UDPClient(const UDPClient &) = delete;

        ~UDPClient() = default;

        size_t send(const std::string& hostname_port, const T &msg) {
            auto pos = std::find(hostname_port.begin(), hostname_port.end(), ':');
            std::string ip = hostname_port.substr(0, pos - hostname_port.begin());
            std::string port_str = hostname_port.substr(pos - hostname_port.begin() + 1);
            uint16_t port = std::stoi(port_str);
            return send(ip, port, msg);
        }
        size_t send(const std::string& hostname, uint16_t port, const T &msg) {

            asio::error_code error;
            asio::ip::udp::resolver::results_type results =
                    resolver_->resolve(hostname, std::to_string(port), error);

            if (results.empty()) {
                printf("Failed to resolve %s, asio error: %s", hostname.c_str(), error.message().c_str());
                return SIZE_MAX;
            }

            for (const auto &endpoint_iter: results) {
                if (!endpoint_iter.endpoint().address().is_v4()) {
                    continue;
                }

                try {
                    std::string str;
                    rt_assert(msg.SerializeToString(&str));
                    const size_t bytes_sent = socket_->send_to(asio::buffer(str), endpoint_iter.endpoint());
                    return bytes_sent;
                } catch (const asio::system_error &e) {
                    printf("asio send_to() failed to %s, error: %s", hostname.c_str(), e.what());
                    return SIZE_MAX;
                }
            }
            printf("Failed to find IPV4 endpoint to %s", hostname.c_str());
            return SIZE_MAX;
        }

    private:
        asio::io_context io_context_;
        std::unique_ptr<asio::ip::udp::resolver> resolver_;
        std::unique_ptr<asio::ip::udp::socket> socket_;
    };
}