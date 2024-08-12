#pragma once

#include "reporter.h"

namespace DOCA {
    class simple_reporter : public bench_reporter {

    public:
        simple_reporter() {
            id = 0;
            last_record_time = std::chrono::system_clock::now();
        }

        collected_bench_stat report_collect(std::vector<bench_stat *> stats) override {
            bench_stat new_stat;
            for (auto &stat: stats) {
                new_stat = new_stat + *stat;
            }

            std::chrono::system_clock::time_point now_point = std::chrono::system_clock::now();

            bench_stat gap_stat = new_stat - last_stat;
            double gap_time =
                    std::chrono::duration_cast<std::chrono::microseconds>(now_point - last_record_time).count() * 1.0;

            double throughput = double(gap_stat.num_ops_finished) / gap_time;
            double avg_latency = double(gap_stat.timer.get_now_timepoint()) / gap_stat.num_ops_finished;

            last_record_time = now_point;
            last_stat = new_stat;

            return collected_bench_stat{throughput, avg_latency, 0};
        }

    private:
        bench_stat last_stat;
        std::chrono::system_clock::time_point last_record_time;
        uint64_t id;
    };
}