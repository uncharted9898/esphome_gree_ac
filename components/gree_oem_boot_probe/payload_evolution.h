#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace esphome {
namespace gree_oem_probe {

inline std::string payload_hex(const std::vector<uint8_t> &payload) {
  static const char digits[] = "0123456789ABCDEF";
  std::string output;
  output.reserve(payload.size() * 3);
  for (size_t index = 0; index < payload.size(); ++index) {
    if (index != 0) output.push_back('.');
    output.push_back(digits[payload[index] >> 4]);
    output.push_back(digits[payload[index] & 0x0F]);
  }
  return output;
}

class PayloadEvolution {
 public:
  std::string observe(const std::vector<uint8_t> &payload,
                      size_t maximum_diff_entries = 24) {
    ++this->samples_;
    if (this->last_.empty()) {
      this->baseline_ = payload;
      this->last_ = payload;
      this->minimum_ = payload;
      this->maximum_ = payload;
      this->xor_mask_.assign(payload.size(), 0);
      this->change_count_.assign(payload.size(), 0);
      return "baseline";
    }

    const size_t old_size = this->last_.size();
    const size_t common_size = std::min(old_size, payload.size());
    std::string differences;
    size_t emitted = 0;

    if (payload.size() > this->minimum_.size()) {
      this->minimum_.resize(payload.size(), 0xFF);
      this->maximum_.resize(payload.size(), 0x00);
      this->xor_mask_.resize(payload.size(), 0x00);
      this->change_count_.resize(payload.size(), 0);
    }

    for (size_t index = 0; index < payload.size(); ++index) {
      this->minimum_[index] = std::min(this->minimum_[index], payload[index]);
      this->maximum_[index] = std::max(this->maximum_[index], payload[index]);
    }

    for (size_t index = 0; index < common_size; ++index) {
      if (this->last_[index] == payload[index]) continue;
      this->xor_mask_[index] |= static_cast<uint8_t>(this->last_[index] ^ payload[index]);
      ++this->change_count_[index];
      if (emitted < maximum_diff_entries) {
        char token[32];
        std::snprintf(token, sizeof(token), "%u:%02X>%02X",
                      static_cast<unsigned>(index), this->last_[index], payload[index]);
        if (!differences.empty()) differences.push_back(',');
        differences += token;
        ++emitted;
      }
    }

    if (old_size != payload.size()) {
      char token[48];
      std::snprintf(token, sizeof(token), "size:%u>%u",
                    static_cast<unsigned>(old_size),
                    static_cast<unsigned>(payload.size()));
      if (!differences.empty()) differences.push_back(',');
      differences += token;
    }
    if (emitted >= maximum_diff_entries) differences += ",...";

    this->last_ = payload;
    return differences.empty() ? "none" : differences;
  }

  size_t samples() const { return this->samples_; }
  size_t size() const { return this->minimum_.size(); }
  const std::vector<uint8_t> &baseline() const { return this->baseline_; }
  const std::vector<uint8_t> &last() const { return this->last_; }
  uint8_t minimum(size_t index) const { return this->minimum_.at(index); }
  uint8_t maximum(size_t index) const { return this->maximum_.at(index); }
  uint8_t xor_mask(size_t index) const { return this->xor_mask_.at(index); }
  uint32_t change_count(size_t index) const { return this->change_count_.at(index); }

 private:
  size_t samples_{0};
  std::vector<uint8_t> baseline_;
  std::vector<uint8_t> last_;
  std::vector<uint8_t> minimum_;
  std::vector<uint8_t> maximum_;
  std::vector<uint8_t> xor_mask_;
  std::vector<uint32_t> change_count_;
};

}  // namespace gree_oem_probe
}  // namespace esphome
