#include "../components/gree_wired_rs485/oem_probe_protocol.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

using namespace esphome::gree_wired_rs485::oem;

template<size_t N>
constexpr bool arrays_equal(const std::array<uint8_t, N> &a,
                            const std::array<uint8_t, N> &b) {
  for (size_t i = 0; i < N; ++i) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

int main() {
  constexpr std::array<uint8_t, 19> expected_identity{
      0x7E, 0x7E, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x03, 0x00, 0x28, 0x1E, 0x19, 0x23, 0x23, 0x00, 0xBA,
  };
  static_assert(arrays_equal(BOOT_IDENTITY, expected_identity),
                "OEM identity frame changed");

  constexpr std::array<uint8_t, 6> mac{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  constexpr auto report = build_mac_report(mac);
  constexpr std::array<uint8_t, 16> expected_mac{
      0x7E, 0x7E, 0x0D, 0x04, 0x07, 0x00, 0x00, 0x00,
      0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x13,
  };
  static_assert(arrays_equal(report, expected_mac), "OEM MAC report changed");

  constexpr auto sync = build_startup_sync();
  static_assert(sync[0] == 0x7E && sync[1] == 0x7E);
  static_assert(sync[2] == 0x1A && sync[3] == 0x03);
  static_assert(sync[26] == 0x01 && sync[27] == 0x00 && sync[28] == 0x1E);
  static_assert(additive_checksum(sync) == sync.back());

  std::vector<uint8_t> info44{
      0x7E, 0x7E, 0x1A, 0x44,
      0x01, 0x00, 0x01, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x01,
      0x00
  };
  info44.back() = additive_checksum(info44);
  ParsedFrame parsed;
  assert(parse_frame(info44, parsed));
  assert(parsed.command == 0x44);
  assert(parsed.payload.size() == 24);
  assert(accepted_information_44(parsed));

  auto bad = info44;
  bad.back() ^= 0x01;
  assert(!parse_frame(bad, parsed));

  FrameAssembler assembler;
  std::vector<uint8_t> complete;
  for (size_t i = 0; i < info44.size(); ++i) {
    const auto result = assembler.push(info44[i], complete);
    if (i + 1 == info44.size()) {
      assert(result == AssembleResult::FRAME_READY);
      assert(complete == info44);
    } else {
      assert(result == AssembleResult::NONE);
    }
  }

  std::vector<uint8_t> report_payload(45, 0);
  report_payload[3] = 0xAF;
  const auto poll = build_nochange_poll(report_payload);
  assert(poll.size() == 50);
  assert(poll[0] == 0x7E && poll[1] == 0x7E);
  assert(poll[2] == 47 && poll[3] == 0x01);
  assert((poll[4 + 3] & 0xAF) == 0);
  assert((poll[4 + 7] & 0x02) != 0);
  assert((poll[4 + 11] & 0x08) != 0);
  assert(poll[4 + 39] == 0x02);
  assert(additive_checksum(poll) == poll.back());

  assert(build_nochange_poll(std::vector<uint8_t>(44, 0)).empty());

  return 0;
}
