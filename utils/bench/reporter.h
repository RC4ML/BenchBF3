#pragma  once

#include "common.hpp"
#include "hs_clock.h"


namespace DOCA {

    class bench_stat {
    public:
        bench_stat operator+(const bench_stat &r) const {
            return bench_stat{ num_ops_finished + r.num_ops_finished, timer + r.timer };
        }

        bench_stat operator-(const bench_stat &r) const {
            return bench_stat{ num_ops_finished - r.num_ops_finished,
                              timer - r.timer };
        }

        inline void reset() {
            num_ops_finished = 0;
            timer.reset();
        }

        inline void start_one_op() {
            timer.minus_now_time();
        }

        inline void start_batch_op(size_t num_ops) {
            timer.minus_now_time(num_ops);
        }

        inline void finish_one_op_without_time() {
            num_ops_finished++;
        }

        inline void finish_one_op() {
            num_ops_finished++;
            timer.add_now_time();
        }

        inline void finish_batch_op(size_t num_ops) {
            num_ops_finished += num_ops;
            timer.add_now_time(num_ops);
        }

        size_t num_ops_finished{};
        Timer timer;
    };

    class collected_bench_stat {
    public:
        collected_bench_stat &operator+=(const collected_bench_stat &rhs) {
            throughtput += rhs.throughtput;
            avg_latency = (avg_latency + rhs.avg_latency) / 2.0;
            return *this;
        }

        std::string to_string(uint32_t payload) const {
            return string_format(
                "thread %lu: Throughput: %.4f Mops/s %.4f MBps/s, avg_latency: %f us\n",
                id, throughtput, throughtput * payload, avg_latency);
        }

        std::string to_string() const {
            return string_format("thread %lu: Throughput: %.4f Mops/s, avg_latency: %f us\n",
                id, throughtput, avg_latency);
        }

        std::string to_string_simple(uint32_t payload) const {
            return string_format("%.4f %.4f\n", throughtput * payload * 8 / 1000, avg_latency);
        }

        void reset() {
            throughtput = 0;
            avg_latency = 0;
        }

        std::string serialize() const {
            return string_format("%lu %f %f",
                id, throughtput, avg_latency);
        }

        void parse(const std::string &str) {
            std::stringstream oss(str);
            oss >> id >> throughtput >> avg_latency;
        }

        double throughtput;
        double avg_latency;
        uint64_t id;
    };

    // report local status
    class bench_reporter {
    public:
        virtual collected_bench_stat report_collect(std::vector<bench_stat *>) = 0;
    };

    // report local status to remote server by network
    class bench_report_async {
    public:
        virtual collected_bench_stat report_collect_async(std::vector<bench_stat *>) = 0;
    };


}