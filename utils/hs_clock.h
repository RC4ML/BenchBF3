#pragma once

#include <iostream>
#include <chrono>
#include <vector>

namespace DOCA {
    class Timer {
        typedef std::chrono::high_resolution_clock clock;
        typedef std::chrono::microseconds res;

    public:
        Timer() : t1(res::zero()), t2(res::zero()) {}

        explicit Timer(int64_t time_point) : t1(res::zero()), t2(res::zero()), time_point(time_point) {}

        ~Timer() = default;

        Timer operator+(const Timer &r) const {
            return Timer(time_point + r.time_point);
        }

        Timer operator-(const Timer &r) const {
            return Timer(time_point - r.time_point);
        }

        void tic() {
            t1 = clock::now();
        }

        long toc() {
            t2 = clock::now();
            return std::chrono::duration_cast<res>(t2 - t1).count();
        }

        long toc_ns() {
            t2 = clock::now();
            return std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
        }

        void minus_now_time(size_t num_ops = 1) {
            clock::time_point tmp = clock::now();
            time_point -=
                    int64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(tmp.time_since_epoch()).count() *
                            num_ops);
        }

        void add_now_time(size_t num_ops = 1) {
            clock::time_point tmp = clock::now();
            time_point +=
                    int64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(tmp.time_since_epoch()).count() *
                            num_ops);
        }

        int64_t get_now_timepoint() const {
            return time_point;
        }

        void reset() {
            t1 = clock::now();
            t2 = clock::now();
            time_point = 0;
        }

    private:
        clock::time_point t1;
        clock::time_point t2;
        int64_t time_point = 0;
    };
}