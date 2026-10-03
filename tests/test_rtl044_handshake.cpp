#include "../components/gree_oem_boot_probe/rtl_handshake.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

using esphome::gree_oem_probe::RTL_BOOT_IDENTITY;
using esphome::gree_oem_probe::RtlInformation44Fields;
using esphome::gree_oem_probe::build_rtl_mac_report;
using esphome::gree_oem_probe::decode_rtl_information_44;
using esphome::gree_oem_probe::rtl_frame_checksum;

template<size_t N> constexpr bool arrays_equal(const std::array<uint8_t, N> &left,
                                                const std::array<uint8_t, N> &right) {
  for (size_t i = 0; i < N; ++i) {
    if (left[i] != right[i]) return false;
  }
  return true;
}

int main() {
  constexpr std::array<uint8_t, 19> expected_identity{
      0x7E, 0x7E, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x03, 0x00, 0x28, 0x1E, 0x19, 0x23, 0x23, 0x00, 0xBA,
  };
  static_assert(arrays_equal(RTL_BOOT_IDENTITY, expected_identity),
                "RTL command-0x02 identity frame changed");

  constexpr std::array<uint8_t, 6> mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  constexpr auto report = build_rtl_mac_report(mac);
  constexpr std::array<uint8_t, 16> expected{
      0x7E, 0x7E, 0x0D, 0x04, 0x07, 0x00, 0x00, 0x00,
      0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x13,
  };
  static_assert(arrays_equal(report, expected),
                "full MAC-bearing RTL command 0x04 changed");
  static_assert(rtl_frame_checksum(report) == report.back(), "bad RTL checksum");

  // The exact standard 24-byte payload captured from the Livo installation.
  const std::vector<uint8_t> captured{
      0x01, 0x00, 0x01, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x01,
  };
  RtlInformation44Fields fields;
  assert(decode_rtl_information_44(captured, fields));
  assert(fields.device_mid == 0x00010001U);
  for (const auto value : fields.binding_code) assert(value == 0x00);
  assert(fields.vendor_id == 1U);
  assert(!fields.has_extension);
  assert(!fields.extension_marker_c9);
  assert(fields.accepted_by_rtl_firmware);

  std::vector<uint8_t> distinct(24, 0);
  distinct[0] = 0x78;
  distinct[1] = 0x56;
  distinct[2] = 0x34;
  distinct[3] = 0x12;
  for (size_t i = 0; i < fields.binding_code.size(); ++i) {
    distinct[4 + i] = static_cast<uint8_t>(i);
  }
  distinct[20] = 0x89;
  distinct[21] = 0xAB;
  distinct[22] = 0xCD;
  distinct[23] = 0xEF;
  assert(decode_rtl_information_44(distinct, fields));
  assert(fields.device_mid == 0x12345678U);
  for (size_t i = 0; i < fields.binding_code.size(); ++i) {
    assert(fields.binding_code[i] == i);
  }
  assert(fields.vendor_id == 0x89ABCDEFU);
  assert(fields.accepted_by_rtl_firmware);

  distinct.push_back(0xC9);
  assert(decode_rtl_information_44(distinct, fields));
  assert(fields.has_extension);
  assert(fields.extension_marker_c9);

  auto zero_mid = captured;
  zero_mid[0] = zero_mid[1] = zero_mid[2] = zero_mid[3] = 0;
  assert(decode_rtl_information_44(zero_mid, fields));
  assert(!fields.accepted_by_rtl_firmware);

  auto zero_vendor = captured;
  zero_vendor[20] = zero_vendor[21] = zero_vendor[22] = zero_vendor[23] = 0;
  assert(decode_rtl_information_44(zero_vendor, fields));
  assert(!fields.accepted_by_rtl_firmware);

  assert(!decode_rtl_information_44(std::vector<uint8_t>(23, 0), fields));
  return 0;
}
