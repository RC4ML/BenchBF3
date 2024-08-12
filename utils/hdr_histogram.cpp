#include "hdr_histogram.h"

namespace DOCA {
    Histogram::Histogram(int64_t lowest, int64_t highest, int sigfigs, double scale) {
        hdr_init(lowest, highest, sigfigs, &latency_hist);
        scale_value = scale;
    }

    Histogram::Histogram(int64_t lowest, int64_t highest, double scale) {
        hdr_init(lowest, highest, 3, &latency_hist);
        scale_value = scale;
    }


    Histogram::~Histogram() {
        hdr_close(latency_hist);
    }

    void Histogram::reset() {
        hdr_reset(latency_hist);
    }


    void Histogram::print(FILE *stream, int32_t ticks) {
        hdr_percentiles_print(latency_hist, stream, ticks, scale_value, CLASSIC);
    }

    void Histogram::print_csv(FILE *stream, int32_t ticks) {
        hdr_percentiles_print(latency_hist, stream, ticks, scale_value, CSV);
    }

    void Histogram::record(int64_t value, int64_t count) {
        if (count == 0) {
            count = static_cast<long>(scale_value);
        }
        hdr_record_values(latency_hist, value, count);
    }
}