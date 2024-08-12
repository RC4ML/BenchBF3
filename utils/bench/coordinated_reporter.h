#pragma once
#include "reporter.h"

namespace DOCA {
    class coordinated_reporter_master {
    public:
        explicit coordinated_reporter_master(uint64_t num_worker, std::string addr_and_port) {
            size_t colon_pos = addr_and_port.find(':');
            rt_assert(colon_pos != std::string::npos, "Invalid address and port");

            std::string ip = addr_and_port.substr(0, colon_pos);
            std::string port_str = addr_and_port.substr(colon_pos + 1);
            uint16_t port = static_cast<uint16_t>(std::stoi(port_str));

            int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            rt_assert(sockfd != -1, "Failed to create socket");


            struct sockaddr_in server_addr{};
            std::memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
            server_addr.sin_port = htons(port);

            rt_assert(bind(sockfd, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr)) != -1,
                      "socket addr bind error");

            fd = sockfd;

            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

            for (uint64_t i = 0; i < num_worker; i++) {
                last_stats.emplace_back();
                last_records.emplace_back(now);
            }
        }

        ~coordinated_reporter_master() {
            close(fd);
        }

        void receive_loop(std::chrono::system_clock::duration duration,
                          std::chrono::system_clock::duration worker_report_duration) {

            char buffer[1024];
            struct sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);

            std::chrono::system_clock::time_point start_time = std::chrono::system_clock::now();
            // tick_time used for print log
            std::chrono::system_clock::time_point tick_time = std::chrono::system_clock::now();
            std::chrono::system_clock::time_point current_time = std::chrono::system_clock::now();

            while (current_time - start_time < duration) {
                std::memset(buffer, 0, sizeof(buffer));

                ssize_t bytes_received = recvfrom(
                        fd,
                        buffer,
                        sizeof(buffer) - 1,
                        0,
                        reinterpret_cast<struct sockaddr *>(&client_addr),
                        &client_len
                );

                if (bytes_received == -1) {
                    std::cerr << "Failed to receive from client" << std::endl;
                    continue;
                }

                std::cout << "Received message from " << inet_ntoa(client_addr.sin_addr) << ":"
                          << ntohs(client_addr.sin_port) << std::endl;
                collected_bench_stat receive_stat;
                receive_stat.parse(std::string(buffer, bytes_received));

                current_time = std::chrono::system_clock::now();
                last_stats[receive_stat.id] = receive_stat;
                last_records[receive_stat.id] = current_time;

                if (current_time - tick_time > worker_report_duration) {
                    std::cout << arggregrate_stats(current_time).to_string() << std::endl;
                    tick_time = std::chrono::system_clock::now();;
                }
                current_time = std::chrono::system_clock::now();

            }
        }

    private:
        collected_bench_stat arggregrate_stats(std::chrono::system_clock::time_point cur_time) {
            collected_bench_stat res;

            for (uint64_t i = 0; i < last_stats.size(); i++) {
                if (cur_time - last_records[i] < std::chrono::milliseconds(1500)) {
                    res += last_stats[i];
                }
            }
            return res;
        }

        int fd;
        std::vector<collected_bench_stat> last_stats;
        std::vector<std::chrono::system_clock::time_point> last_records;
    };


    class coordinated_reporter_worker : public bench_report_async {
    public:
        explicit coordinated_reporter_worker(std::string addr_and_port, bench_reporter *r) {
            size_t colon_pos = addr_and_port.find(':');
            rt_assert(colon_pos != std::string::npos, "Invalid address and port");

            std::string ip = addr_and_port.substr(0, colon_pos);
            std::string port_str = addr_and_port.substr(colon_pos + 1);
            uint16_t port = static_cast<uint16_t>(std::stoi(port_str));

            fd = socket(AF_INET, SOCK_DGRAM, 0);
            rt_assert(fd != -1, "Failed to create socket");


            std::memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
            server_addr.sin_port = htons(port);
        }

        collected_bench_stat report_collect_async(std::vector<bench_stat *> stats) override {
            collected_bench_stat stat = reporter->report_collect(stats);
            std::string buf = stat.serialize();
            int res = sendto(
                    fd,
                    buf.c_str(),
                    buf.length(),
                    0,
                    reinterpret_cast<const struct sockaddr *>(&server_addr),
                    sizeof(server_addr)
            );
            rt_assert(res != -1, "Failed to send message");
            return stat;
        }

    private:
        bench_reporter *reporter;
        int fd;
        struct sockaddr_in server_addr{};

    };
}

