#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome {
namespace gree_oem_probe {

static constexpr size_t RTL_INFORMATION_44_STANDARD_PAYLOAD_SIZE = 24;
static constexpr size_t RTL_INFORMATION_44_BINDING_CODE_SIZE = 16;
static constexpr uint8_t RTL_INFORMATION_44_EXTENSION_MARKER = 0xC9;
static constexpr size_t RTL_IDENTITY_FRAME_SIZE = 19;
static constexpr size_t RTL_MAC_REPORT_FRAME_SIZE = 16;

// Recovered identically from the V2 and V3 command-0x02 builders.
static constexpr std::array<uint8_t, RTL_IDENTITY_FRAME_SIZE> RTL_BOOT_IDENTITY{
    0x7E, 0x7E, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x28, 0x1E, 0x19, 0x23, 0x23, 0x00, 0xBA,
};

struct RtlInformation44Fields {
  uint32_t device_mid{0};
  std::array<uint8_t, RTL_INFORMATION_44_BINDING_CODE_SIZE> binding_code{};
  uint32_t vendor_id{0};
  bool has_extension{false};
  bool extension_marker_c9{false};
  bool accepted_by_rtl_firmware{false};
};

inline bool decode_rtl_information_44(const std::vector<uint8_t> &payload,
                                      RtlInformation44Fields &out) {
  out = RtlInformation44Fields{};
  if (payload.size() < RTL_INFORMATION_44_STANDARD_PAYLOAD_SIZE) return false;

  // The V2 and V3 RTL8720CF decoders assemble the first value little-endian.
  out.device_mid = static_cast<uint32_t>(payload[0]) |
                   (static_cast<uint32_t>(payload[1]) << 8U) |
                   (static_cast<uint32_t>(payload[2]) << 16U) |
                   (static_cast<uint32_t>(payload[3]) << 24U);

  for (size_t i = 0; i < out.binding_code.size(); ++i) {
    out.binding_code[i] = payload[4 + i];
  }

  // The final four bytes are assembled in network/big-endian order.
  out.vendor_id = (static_cast<uint32_t>(payload[20]) << 24U) |
                  (static_cast<uint32_t>(payload[21]) << 16U) |
                  (static_cast<uint32_t>(payload[22]) << 8U) |
                  static_cast<uint32_t>(payload[23]);

  out.has_extension = payload.size() > RTL_INFORMATION_44_STANDARD_PAYLOAD_SIZE;
  out.extension_marker_c9 =
      out.has_extension && payload[RTL_INFORMATION_44_STANDARD_PAYLOAD_SIZE] ==
                               RTL_INFORMATION_44_EXTENSION_MARKER;

  // Both firmware versions only advance the normal startup state after the MID
  // and VendorInt values are non-zero. The 16-byte binding code may be all zero.
  out.accepted_by_rtl_firmware = out.device_mid != 0 && out.vendor_id != 0;
  return true;
}

constexpr uint8_t rtl_frame_checksum(const std::array<uint8_t, RTL_MAC_REPORT_FRAME_SIZE> &frame) {
  uint8_t checksum = 0;
  for (size_t i = 2; i + 1 < frame.size(); ++i) {
    checksum = static_cast<uint8_t>(checksum + frame[i]);
  }
  return checksum;
}

constexpr std::array<uint8_t, RTL_MAC_REPORT_FRAME_SIZE> build_rtl_mac_report(
    const std::array<uint8_t, 6> &mac) {
  // Recovered V2/V3 command-0x04 layout:
  //   7E 7E 0D 04 07 00 00 00 <MAC[6]> 00 <checksum>
  // LEN=0x0D counts CMD, the 11-byte payload, and the checksum.
  std::array<uint8_t, RTL_MAC_REPORT_FRAME_SIZE> frame{
      0x7E, 0x7E, 0x0D, 0x04, 0x07, 0x00, 0x00, 0x00,
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], 0x00, 0x00,
  };
  frame.back() = rtl_frame_checksum(frame);
  return frame;
}

}  // namespace gree_oem_probe
}  // namespace esphome
