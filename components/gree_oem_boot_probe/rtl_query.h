#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome {
namespace gree_oem_probe {

constexpr size_t RTL_REPORT_QUERY_SIZE = 29;

// RTL8720CF V2/V3 command-0x03 report request. LEN is 0x1A and therefore the
// complete frame is 29 bytes. Index 27 is a reserved byte and index 28 is the
// additive checksum. Older 28-byte experiments omitted that reserved byte.
constexpr std::array<uint8_t, RTL_REPORT_QUERY_SIZE> build_rtl_report_query(
    uint8_t primary_selector, uint8_t secondary_selector, uint8_t rssi_magnitude = 0x3B,
    uint8_t module_state = 0x01) {
  std::array<uint8_t, RTL_REPORT_QUERY_SIZE> frame{};
  frame[0] = 0x7E;
  frame[1] = 0x7E;
  frame[2] = 0x1A;
  frame[3] = 0x03;
  frame[4] = primary_selector;
  frame[10] = rssi_magnitude;
  frame[13] = rssi_magnitude;
  frame[14] = secondary_selector;
  frame[26] = module_state;
  frame[27] = 0x00;

  uint8_t checksum = 0;
  for (size_t i = 2; i + 1 < frame.size(); ++i) {
    checksum = static_cast<uint8_t>(checksum + frame[i]);
  }
  frame[28] = checksum;
  return frame;
}

constexpr bool valid_rtl_report_query(
    const std::array<uint8_t, RTL_REPORT_QUERY_SIZE> &frame) {
  if (frame[0] != 0x7E || frame[1] != 0x7E || frame[2] != 0x1A || frame[3] != 0x03 ||
      static_cast<size_t>(frame[2]) + 3 != frame.size()) {
    return false;
  }
  uint8_t checksum = 0;
  for (size_t i = 2; i + 1 < frame.size(); ++i) {
    checksum = static_cast<uint8_t>(checksum + frame[i]);
  }
  return checksum == frame.back();
}

}  // namespace gree_oem_probe
}  // namespace esphome
