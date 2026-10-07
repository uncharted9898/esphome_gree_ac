#pragma once

#include <cstdint>

namespace esphome {
namespace gree_wired_rs485 {
namespace diagnostics {

struct LineActivitySnapshot {
  uint32_t samples{0};
  uint32_t high_samples{0};
  uint32_t transitions{0};

  float high_percent() const {
    if (this->samples == 0) return 0.0f;
    return 100.0f * static_cast<float>(this->high_samples) /
           static_cast<float>(this->samples);
  }
};

class LineActivityTracker {
 public:
  void observe(int level) {
    if (level < 0) return;

    const bool high = level != 0;
    ++this->window_samples_;
    if (high) ++this->window_high_samples_;

    if (this->has_last_level_ && high != this->last_high_) {
      ++this->window_transitions_;
      ++this->total_transitions_;
    }

    this->last_high_ = high;
    this->has_last_level_ = true;
  }

  LineActivitySnapshot take_window() {
    const LineActivitySnapshot snapshot{
        this->window_samples_, this->window_high_samples_, this->window_transitions_};
    this->window_samples_ = 0;
    this->window_high_samples_ = 0;
    this->window_transitions_ = 0;
    return snapshot;
  }

  uint32_t total_transitions() const { return this->total_transitions_; }
  bool has_last_level() const { return this->has_last_level_; }
  bool last_high() const { return this->last_high_; }

 private:
  uint32_t window_samples_{0};
  uint32_t window_high_samples_{0};
  uint32_t window_transitions_{0};
  uint32_t total_transitions_{0};
  bool has_last_level_{false};
  bool last_high_{false};
};

}  // namespace diagnostics
}  // namespace gree_wired_rs485
}  // namespace esphome
