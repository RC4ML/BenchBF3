/**
 * @file timely.h
 * @brief TIMELY congestion control [SIGCOMM 15]
 * From: http://people.eecs.berkeley.edu/~radhika/timely-code-snippet.cc
 * Units: Microseconds or TSC for time, bytes/sec for throughput
 */

#pragma once

#include <iomanip>
#include "cc/timely_sweep_params.h"
#include "common.h"
#include "util/latency.h"
#include "util/timer.h"

namespace erpc {
struct timely_record_t {
  double rtt_;
  double rate_;

  timely_record_t() : rtt_(0.0), rate_(0.0) {}
  timely_record_t(double rtt, double rate) : rtt_(rtt), rate_(rate) {}

  std::string to_string() {
    std::ostringstream ret;
    ret << "[RTT " << std::setprecision(5) << rtt_ << " us"
        << ", rate " << std::setprecision(4)
        << (rate_ / (1000 * 1000 * 1000)) * 8 << "]";
    return ret.str();
  }
};

/// Implementation of the Timely congestion control protocol from SIGCOMM 15
class Timely {
 public:
  // Debugging
  static constexpr bool kVerbose = false;
  static constexpr bool kRecord = false;        ///< Fast-record Timely steps
  static constexpr bool kLatencyStats = false;  ///< Track per-packet RTT stats

  // Config
  static constexpr double kMinRate = 15.0 * 1000 * 1000;
  static constexpr double kAddRate = 5.0 * 1000 * 1000;

  static constexpr double kMinRTT = 2;
  static constexpr double kTLow = 50;
  static constexpr double kTHigh = 1000;
  static constexpr size_t kHaiThresh = 5;

  double rate_ = 0.0;  ///< The current sending rate
  size_t neg_gradient_count_ = 0;
  double prev_rtt_ = kMinRTT;
  double avg_rtt_diff_ = 0.0;
  size_t last_update_tsc_ = 0;

  // Const
  double min_rtt_tsc_ = 0.0;
  double t_low_tsc_ = 0.0;
  double freq_ghz_ = 0.0;
  double link_bandwidth_ = 0.0;

  // For latency stats
  Latency latency_;

  // For recording, used only with kRecord
  size_t create_tsc_;
  std::vector<timely_record_t> record_vec_;

  Timely() {}
  Timely(double freq_ghz, double link_bandwidth)
      : last_update_tsc_(rdtsc()),
        min_rtt_tsc_(kMinRTT * freq_ghz * 1000),
        t_low_tsc_(kTLow * freq_ghz * 1000),
        freq_ghz_(freq_ghz),
        link_bandwidth_(link_bandwidth),
        create_tsc_(rdtsc()) {
    rate_ = link_bandwidth;  // Start sending at the max rate
    if (kRecord) record_vec_.reserve(1000000);
  }

  /// The w() function from the ECN-vs-delay paper by Zhu et al. (CoNEXT 16)
  static double w_func(double g) {
    assert(kPatched);
    if (g <= -0.25) return 0;
    if (g >= 0.25) return 1;
    return (2 * g + 0.5);
  }

  /**
   * @brief Perform a rate update
   *
   * @param _rdtsc A recently sampled RDTSC. This can reduce calls to rdtsc()
   * when the caller can reuse a sampled RDTSC.
   * @param sample_rtt_tsc The RTT sample in RDTSC cycles
   */
  void update_rate(size_t _rdtsc, size_t sample_rtt_tsc) {
    assert(_rdtsc >= 1000000000 && _rdtsc >= last_update_tsc_);  // Sanity check

    if (kCcOptTimelyBypass &&
        (rate_ == link_bandwidth_ && sample_rtt_tsc <= t_low_tsc_)) {
      // Bypass expensive computation, but include the latency sample in stats.
      if (kLatencyStats) {
        latency_.update(
            static_cast<size_t>(to_usec(sample_rtt_tsc, freq_ghz_)));
      }
      return;
    }

    // Sample RTT can be lower than min RTT during retransmissions
    if (unlikely(sample_rtt_tsc < min_rtt_tsc_)) return;

    // Convert the sample RTT to usec, and don't use _sample_rtt_tsc from now
    double sample_rtt = to_usec(sample_rtt_tsc, freq_ghz_);

    double rtt_diff = sample_rtt - prev_rtt_;
    neg_gradient_count_ = (rtt_diff < 0) ? neg_gradient_count_ + 1 : 0;
    avg_rtt_diff_ =
        ((1 - kEwmaAlpha) * avg_rtt_diff_) + (kEwmaAlpha * rtt_diff);

    double delta_factor = (_rdtsc - last_update_tsc_) / min_rtt_tsc_;  // fdiv
    delta_factor = (std::min)(delta_factor, 1.0);

    double ai_factor = kAddRate * delta_factor;

    double new_rate;
    if (sample_rtt < kTLow) {
      // Additive increase
      new_rate = rate_ + ai_factor;
    } else {
      double md_factor = delta_factor * kBeta;     // Scaled factor for decrease
      double norm_grad = avg_rtt_diff_ / kMinRTT;  // Normalized gradient

      if (likely(sample_rtt <= kTHigh)) {
        if (kPatched) {
          double wght = w_func(norm_grad);
          double err = (sample_rtt - kTLow) / kTLow;
          if (kVerbose) {
            printf("wght = %.4f, err = %.4f, md = x%.3f, ai = %.3f Gbps\n",
                   wght, err, (1 - md_factor * wght * err),
                   rate_to_gbps(ai_factor * (1 - wght)));
          }

          new_rate =
              rate_ * (1 - md_factor * wght * err) + ai_factor * (1 - wght);
        } else {
          // Original logic
          if (norm_grad <= 0) {
            size_t n = neg_gradient_count_ >= kHaiThresh ? 5 : 1;
            new_rate = rate_ + n * ai_factor;
          } else {
            new_rate = rate_ * (1.0 - md_factor * norm_grad);
          }
        }
      } else {
        // Multiplicative decrease based on current RTT sample, not average
        new_rate = rate_ * (1 - md_factor * (1 - kTHigh / sample_rtt));
      }
    }

    rate_ = (std::max)(new_rate, rate_ * 0.5);
    rate_ = (std::min)(rate_, link_bandwidth_);
    rate_ = (std::max)(rate_, double(kMinRate));

    prev_rtt_ = sample_rtt;
    last_update_tsc_ = _rdtsc;

    // Debug/stats code goes here
    if (kLatencyStats) latency_.update(static_cast<size_t>(sample_rtt));
    if (kRecord && rate_ != link_bandwidth_) {
      record_vec_.emplace_back(sample_rtt, rate_);
    }

    if (kRecord && rate_ == kMinRate) {
      // If we reach min rate after steady state, print a log and exit
      double sec_since_creation = to_sec(rdtsc() - create_tsc_, freq_ghz_);
      if (sec_since_creation >= 0.0) {
        for (auto &r : record_vec_) printf("%s\n", r.to_string().c_str());
        exit(-1);
      }
    }
  }

  /// Get RTT percentile if latency stats are enabled, and reset latency stats
  double get_rtt_perc(double perc) {
    if (!kLatencyStats || latency_.count() == 0) return -1.0;
    double ret = latency_.perc(perc);
    return ret;
  }

  void reset_rtt_stats() { latency_.reset(); }

  double get_avg_rtt_diff() const { return avg_rtt_diff_; }
  double get_rate_gbps() const { return rate_to_gbps(rate_); }

  /// Convert a default bytes/second rate to Gbit/s
  static double rate_to_gbps(double r) {
    return (r / (1000 * 1000 * 1000)) * 8;
  }

  /// Convert a Gbit/s rate to the default bytes/second
  static double gbps_to_rate(double r) {
    return (r / 8) * (1000 * 1000 * 1000);
  }
};
}  // namespace erpc
