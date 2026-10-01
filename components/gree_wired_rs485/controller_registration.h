#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome {
namespace gree_wired_rs485 {
namespace registration {

static constexpr size_t COUNTER_INDEX = 26;
static constexpr std::array<uint8_t, 10> ACCEPT_COUNTERS = {
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x20};

inline uint8_t xor_checksum(const uint8_t *data, size_t len_without_checksum) {
  uint8_t value = 0;
  for (size_t i = 0; i < len_without_checksum; ++i) value ^= data[i];
  return value;
}

inline uint8_t counter_for_attempt(size_t attempt) {
  return ACCEPT_COUNTERS[attempt % ACCEPT_COUNTERS.size()];
}

inline std::vector<uint8_t> make_frame(size_t attempt) {
  static constexpr std::array<uint8_t, 40> TEMPLATE = {
      0x7E, 0x7E, 0xFF, 0x00, 0x11, 0x22,
      0x09, 0x30, 0x83, 0x11, 0x1B, 0x00, 0x00, 0x10,
      0xE0, 0xE0, 0x08, 0x00, 0x28, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x30, 0x58};

  std::vector<uint8_t> frame(TEMPLATE.begin(), TEMPLATE.end());
  frame[COUNTER_INDEX] = counter_for_attempt(attempt);
  frame.back() = xor_checksum(frame.data(), frame.size() - 1);
  return frame;
}

}  // namespace registration
}  // namespace gree_wired_rs485
}  // namespace esphome
