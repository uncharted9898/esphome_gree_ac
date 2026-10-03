#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome {
namespace gree_oem_probe {

constexpr size_t RTL_REPORT_QUERY_SIZE = 29;
constexpr size_t RTL_ENERGY_FLOW_QUERY_SIZE = 53;

// The RTL8720CF startup scheduler packs the current calendar into full-frame
// bytes 15..20 before sending command 0x03. The year occupies 12 bits, followed
// by a four-bit month; day, hour, minute and second are one byte each. A zero
// value is the firmware's normal "time not synchronized" representation.
struct RtlDateTimeContext {
  uint16_t year{0};
  uint8_t month{0};
  uint8_t day{0};
  uint8_t hour{0};
  uint8_t minute{0};
  uint8_t second{0};
};

template<size_t N>
constexpr uint8_t rtl_additive_checksum(const std::array<uint8_t, N> &frame) {
  uint8_t checksum = 0;
  for (size_t i = 2; i + 1 < frame.size(); ++i) {
    checksum = static_cast<uint8_t>(checksum + frame[i]);
  }
  return checksum;
}

// RTL8720CF V2/V3 command-0x03 report request. LEN is 0x1A and therefore the
// complete frame is 29 bytes. Index 27 is a reserved byte and index 28 is the
// additive checksum. Older 28-byte experiments omitted that reserved byte.
//
// The archived firmware proves that frame[8..10] and frame[11..13] are two
// HH:MM:SS context triples. The firmware writes 17:3B:3B only when the high
// context flags in frame[4] are set. They are not RSSI fields. A read-only
// selector request with no time-window context must leave all six bytes zero.
constexpr std::array<uint8_t, RTL_REPORT_QUERY_SIZE> build_rtl_report_query(
    uint8_t primary_selector, uint8_t secondary_selector,
    uint8_t module_state = 0x01,
    RtlDateTimeContext date_time = {}) {
  std::array<uint8_t, RTL_REPORT_QUERY_SIZE> frame{};
  frame[0] = 0x7E;
  frame[1] = 0x7E;
  frame[2] = 0x1A;
  frame[3] = 0x03;
  frame[4] = primary_selector;
  frame[14] = secondary_selector;
  frame[15] = static_cast<uint8_t>((date_time.year >> 4U) & 0xFFU);
  frame[16] = static_cast<uint8_t>(((date_time.year & 0x0FU) << 4U) |
                                   (date_time.month & 0x0FU));
  frame[17] = date_time.day;
  frame[18] = date_time.hour;
  frame[19] = date_time.minute;
  frame[20] = date_time.second;
  frame[26] = module_state;
  frame[27] = 0x00;
  frame[28] = rtl_additive_checksum(frame);
  return frame;
}

// After accepting command 0x44, both archived RTL8720CF versions send this
// complete command-0x03 envelope four times while the startup counter advances.
// It is the same initialized command buffer used by later selector reads, with
// no report selector asserted. State 1 is the value the OEM firmware assigns
// immediately after the ACE/cloud server login succeeds. Using it here emulates
// the fully online adapter state; an unsynchronized clock is represented by zeros.
constexpr std::array<uint8_t, RTL_REPORT_QUERY_SIZE> build_rtl_startup_sync(
    uint8_t module_state = 0x01,
    RtlDateTimeContext date_time = {}) {
  return build_rtl_report_query(0x00, 0x00, module_state, date_time);
}

constexpr bool valid_rtl_report_query(
    const std::array<uint8_t, RTL_REPORT_QUERY_SIZE> &frame) {
  if (frame[0] != 0x7E || frame[1] != 0x7E || frame[2] != 0x1A ||
      frame[3] != 0x03 || static_cast<size_t>(frame[2]) + 3 != frame.size() ||
      frame[27] != 0x00 || rtl_additive_checksum(frame) != frame.back()) {
    return false;
  }

  const std::array<uint8_t, 3> first_context{
      frame[8], frame[9], frame[10]};
  const std::array<uint8_t, 3> second_context{
      frame[11], frame[12], frame[13]};
  const std::array<uint8_t, 3> disabled_context{0x00, 0x00, 0x00};
  const std::array<uint8_t, 3> enabled_context{0x17, 0x3B, 0x3B};

  if (first_context != ((frame[4] & 0x80U) != 0 ? enabled_context
                                                 : disabled_context)) {
    return false;
  }
  if (second_context != ((frame[4] & 0x40U) != 0 ? enabled_context
                                                  : disabled_context)) {
    return false;
  }
  return true;
}

// Separate RTL8720CF EnergyFlow request recovered identically from V2 and V3.
// This is not a command-0x03 selector. The OEM scheduler sends command 0x09 and
// the receive dispatcher expects command 0x53. The 48-byte request payload is
// zero-filled; LEN=0x32 makes the complete wire frame 53 bytes.
constexpr std::array<uint8_t, RTL_ENERGY_FLOW_QUERY_SIZE>
build_rtl_energy_flow_query() {
  std::array<uint8_t, RTL_ENERGY_FLOW_QUERY_SIZE> frame{};
  frame[0] = 0x7E;
  frame[1] = 0x7E;
  frame[2] = 0x32;
  frame[3] = 0x09;
  frame.back() = rtl_additive_checksum(frame);
  return frame;
}

constexpr bool valid_rtl_energy_flow_query(
    const std::array<uint8_t, RTL_ENERGY_FLOW_QUERY_SIZE> &frame) {
  if (frame[0] != 0x7E || frame[1] != 0x7E || frame[2] != 0x32 ||
      frame[3] != 0x09 || static_cast<size_t>(frame[2]) + 3 != frame.size()) {
    return false;
  }
  for (size_t index = 4; index + 1 < frame.size(); ++index) {
    if (frame[index] != 0x00) return false;
  }
  return rtl_additive_checksum(frame) == frame.back();
}

// EnergyFlow may be refreshed when the appliance advertises the page or after
// a real 0x53 response has already proven support. A forced unadvertised read is
// restricted to one initial full-discovery attempt.
constexpr bool should_query_rtl_energy_flow(
    bool enabled, bool advertised, bool response_seen,
    bool force_discovery, bool discovery_attempted, bool full_discovery_cycle) {
  if (!enabled) return false;
  if (advertised || response_seen) return true;
  return force_discovery && full_discovery_cycle && !discovery_attempted;
}

// The optional command-0x03/response-0x40 page follows a similar bounded
// policy, but legacy behavior already permitted one discovery attempt when no
// 0x32 synchronization response existed. A present capability byte with bit 0
// clear suppresses the read unless the research configuration explicitly
// requests one corrected-frame retry.
constexpr bool should_query_rtl_electrical_page(
    bool capability_known, bool advertised, bool response_seen,
    bool force_discovery, bool discovery_attempted,
    bool full_discovery_cycle) {
  if (advertised || response_seen) return true;
  if (!full_discovery_cycle || discovery_attempted) return false;
  if (!capability_known) return true;
  return force_discovery;
}

}  // namespace gree_oem_probe
}  // namespace esphome
