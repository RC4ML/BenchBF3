#pragma once

#include "reporter.h"
#include "numautil.h"

namespace DOCA {
    class bench_runner;

    using worker_func = std::function<void(uint64_t, bench_runner *, bench_stat *, void *args)>;

    class bench_runner {
    public:
        explicit bench_runner(uint64_t nw) {
            num_workers = nw;
            is_running = false;
            bind_offset = 0;
            bind_numa = 0;
        }

        explicit bench_runner(uint64_t nw, uint32_t offset) {
            num_workers = nw;
            is_running = false;
            bind_offset = offset;
            bind_numa = 0;
        }

        explicit bench_runner(uint64_t nw, uint32_t offset, uint32_t numa_node) {
            num_workers = nw;
            is_running = false;
            bind_offset = offset;
            bind_numa = numa_node;
        }

        void run(worker_func func, void *args) {
            rt_assert(!is_running, "already running");
            for (uint64_t i = 0; i < num_workers; i++) {
                worker_stats.push_back(new bench_stat);
                handlers.emplace_back(func, i + bind_offset, this, worker_stats[i], args);
                bind_to_core(handlers[i], bind_numa, i + bind_offset);
            }
            is_running = true;
        }

        void stop() {
            is_running = false;
            for (auto &h : handlers) {
                h.join();
            }
        }

        collected_bench_stat report(bench_reporter *reporter) {
            return reporter->report_collect(worker_stats);
        }

        collected_bench_stat report_async(bench_report_async *report) {
            return report->report_collect_async(worker_stats);
        }

        [[nodiscard]] bool running() {
            return is_running;
        }

    private:
        std::vector<std::thread> handlers;
        std::vector<bench_stat *> worker_stats;
        uint64_t num_workers;
        std::atomic<bool> is_running;
        uint32_t bind_offset;
        uint32_t bind_numa;
    };


}