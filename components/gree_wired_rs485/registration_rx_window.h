#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome {
namespace gree_wired_rs485 {
namespace diagnostics {

class RegistrationRxWindow {
 public:
  static constexpr size_t MAX_RECORDED_OFFSETS = 128;

  void open(uint32_t opened_at_us, uint32_t de_released_at_us,
            uint32_t pending_probe_at_us, size_t pending_at_probe,
            uint32_t valid_frames_at_open) {
    this->active_ = true;
    this->opened_at_us_ = opened_at_us;
    this->de_released_at_us_ = de_released_at_us;
    this->pending_probe_at_us_ = pending_probe_at_us;
    this->pending_at_probe_ = pending_at_probe;
    this->valid_frames_at_open_ = valid_frames_at_open;
    this->bytes_observed_ = 0;
    this->first_drain_us_ = 0;
    this->last_drain_us_ = 0;
    this->drain_offsets_us_.clear();
  }

  void observe_byte(uint32_t observed_at_us) {
    if (!this->active_) return;

    const uint32_t offset_us =
        static_cast<uint32_t>(observed_at_us - this->opened_at_us_);
    if (this->bytes_observed_ == 0) {
      this->first_drain_us_ = offset_us;
    }
    this->last_drain_us_ = offset_us;
    ++this->bytes_observed_;

    if (this->drain_offsets_us_.size() < MAX_RECORDED_OFFSETS) {
      this->drain_offsets_us_.push_back(offset_us);
    }
  }

  void close() { this->active_ = false; }

  bool active() const { return this->active_; }
  uint32_t opened_at_us() const { return this->opened_at_us_; }
  uint32_t de_released_at_us() const { return this->de_released_at_us_; }
  size_t pending_at_probe() const { return this->pending_at_probe_; }
  uint32_t de_release_delay_us() const {
    return static_cast<uint32_t>(
        this->de_released_at_us_ - this->opened_at_us_);
  }
  uint32_t pending_probe_delay_us() const {
    return static_cast<uint32_t>(
        this->pending_probe_at_us_ - this->opened_at_us_);
  }
  uint32_t release_to_probe_delay_us() const {
    return static_cast<uint32_t>(
        this->pending_probe_at_us_ - this->de_released_at_us_);
  }
  uint32_t bytes_observed() const { return this->bytes_observed_; }
  uint32_t first_drain_us() const {
    return this->bytes_observed_ == 0 ? 0 : this->first_drain_us_;
  }
  uint32_t last_drain_us() const {
    return this->bytes_observed_ == 0 ? 0 : this->last_drain_us_;
  }
  uint32_t drain_span_us() const {
    return this->bytes_observed_ < 2
               ? 0
               : static_cast<uint32_t>(
                     this->last_drain_us_ - this->first_drain_us_);
  }
  uint32_t valid_frame_delta(uint32_t current_valid_frames) const {
    return static_cast<uint32_t>(
        current_valid_frames - this->valid_frames_at_open_);
  }
  const std::vector<uint32_t> &drain_offsets_us() const {
    return this->drain_offsets_us_;
  }

 private:
  bool active_{false};
  uint32_t opened_at_us_{0};
  uint32_t de_released_at_us_{0};
  uint32_t pending_probe_at_us_{0};
  size_t pending_at_probe_{0};
  uint32_t valid_frames_at_open_{0};
  uint32_t bytes_observed_{0};
  uint32_t first_drain_us_{0};
  uint32_t last_drain_us_{0};
  std::vector<uint32_t> drain_offsets_us_;
};

}  // namespace diagnostics
}  // namespace gree_wired_rs485
}  // namespace esphome
