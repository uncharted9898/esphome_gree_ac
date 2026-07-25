#pragma once

#include <cmath>
#include <cstdint>

namespace esphome {
namespace sinclair_ac {

class TemperatureStabilizer {
 public:
  void reset() {
    has_output_ = false;
    candidate_active_ = false;
    output_ = 0.0f;
    candidate_ = 0.0f;
    candidate_since_ = 0;
  }

  bool process(float sample, uint32_t now, bool enabled, uint32_t settle_time_ms,
               float immediate_delta_c, float &accepted) {
    if (!std::isfinite(sample)) return false;
    if (!has_output_) return accept_(sample, accepted);
    if (!enabled) {
      candidate_active_ = false;
      if (same_(sample, output_)) return false;
      return accept_(sample, accepted);
    }
    if (same_(sample, output_)) {
      candidate_active_ = false;
      return false;
    }
    if (immediate_delta_c <= 0.0f || std::fabs(sample - output_) >= immediate_delta_c)
      return accept_(sample, accepted);
    if (!candidate_active_ || !same_(sample, candidate_)) {
      candidate_active_ = true;
      candidate_ = sample;
      candidate_since_ = now;
      return false;
    }
    if (settle_time_ms == 0 || static_cast<uint32_t>(now - candidate_since_) >= settle_time_ms)
      return accept_(candidate_, accepted);
    return false;
  }

 private:
  static bool same_(float a, float b) { return std::fabs(a - b) < 0.01f; }
  bool accept_(float value, float &accepted) {
    has_output_ = true;
    candidate_active_ = false;
    output_ = value;
    accepted = value;
    return true;
  }

  bool has_output_{false};
  bool candidate_active_{false};
  float output_{0.0f};
  float candidate_{0.0f};
  uint32_t candidate_since_{0};
};

}  // namespace sinclair_ac
}  // namespace esphome
