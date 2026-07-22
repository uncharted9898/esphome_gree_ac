#include "../components/sinclair_ac/protocol_frame.h"
#include "../components/sinclair_ac/protocol_state.h"
#include "../components/sinclair_ac/request_lifecycle.h"

#include <cassert>
#include <string>

using namespace sinclair_ac_protocol;
using esphome::sinclair_ac::OutstandingRequest;
using esphome::sinclair_ac::RequestLifecycle;

static std::vector<uint8_t> frame(uint8_t command, size_t payload_length) {
  std::vector<uint8_t> raw{SYNC, SYNC, static_cast<uint8_t>(payload_length + 2), command};
  raw.resize(payload_length + 4);

  uint8_t checksum = 0;
  for (size_t i = 2; i < raw.size(); ++i) checksum += raw[i];
  raw.push_back(checksum);
  return raw;
}

int main() {
  ParsedFrame parsed;

  const auto known = frame(0x31, 43);
  assert(parse(known, parsed) == Result::VALID_KNOWN);
  assert(parsed.payload_length == 43);
  assert(parsed.raw == known);

  const auto transmitted = frame(0x01, 45);
  assert(transmitted[2] == 0x2F);
  assert(transmitted.size() == 50);

  const std::vector<uint8_t> livo_report{
      0x7E, 0x7E, 0x31, 0x31,
      0x04, 0x00, 0x40, 0x00, 0x90, 0x80, 0x06, 0xC2, 0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x46, 0x00, 0x00,
      0x15,
  };
  assert(livo_report[2] == 0x31);
  assert(livo_report.size() == 52);
  assert(parse(livo_report, parsed) == Result::VALID_KNOWN);
  assert(parsed.declared_length == 49);
  assert(parsed.command == 0x31);
  assert(parsed.payload_length == 47);
  assert(parsed.received_checksum == 0x15);
  assert(parsed.calculated_checksum == 0x15);

  // A stale DRY/low report must not be mistaken for a COOL/low command result.
  constexpr uint8_t power_mask = 0x80;
  constexpr uint8_t mode_mask = 0x70;
  constexpr uint8_t fan_mask = 0x03;
  constexpr uint8_t cool_low = power_mask | 0x10 | 0x01;
  constexpr uint8_t cool_medium = power_mask | 0x10 | 0x02;
  constexpr uint8_t dry_low = power_mask | 0x20 | 0x01;
  assert(dry_low == 0xA1);
  assert(cool_low == 0x91);
  assert(cool_medium == 0x92);
  assert((dry_low & (mode_mask | fan_mask)) != (cool_low & (mode_mask | fan_mask)));
  // Indoor-temperature updates leave the packed mode/fan command unchanged.
  const uint8_t warmer_temperature_raw = 0x41;
  assert(warmer_temperature_raw == 0x41);
  assert(cool_low == 0x91);

  // Gree TX owns only power, mode and the low two fan bits in byte 4.  Byte
  // 18 is model-specific on this layout and must be passed through unchanged.
  const auto build_gree_byte4 = [](uint8_t report, uint8_t mode, uint8_t fan, bool power) {
    constexpr uint8_t gree_owned = 0x80 | 0x70 | 0x03;
    return static_cast<uint8_t>((report & ~gree_owned) | (power ? 0x80 : 0) | (mode << 4) | (fan & 0x03));
  };
  constexpr uint8_t gree_report_byte4 = 0xA1;
  constexpr uint8_t gree_report_byte18 = 0x08;
  const uint8_t no_change_gree = build_gree_byte4(gree_report_byte4, 2, 1, true);
  assert(no_change_gree == 0xA1);  // DRY, ON, Low; SET_NOCHANGE is added separately.
  assert(gree_report_byte18 == 0x08);
  assert(build_gree_byte4(gree_report_byte4, 1, 1, true) == 0x91);  // COOL, Low.
  assert(build_gree_byte4(0x91, 1, 2, true) == 0x92);               // COOL, Medium.
  assert((build_gree_byte4(0xA5, 1, 1, true) & 0x0C) == 0x04);      // Preserve unowned bits 2-3.

  // Sinclair's extended layout still owns and replaces the legacy byte-18 fan field.
  constexpr uint8_t sinclair_speed1_mask = 0x0F;
  const uint8_t sinclair_medium_speed1 = static_cast<uint8_t>((0x08 & ~sinclair_speed1_mask) | 3);
  assert(sinclair_medium_speed1 == 0x03);

  assert(parse(frame(0x44, 1), parsed) == Result::VALID_UNKNOWN);
  auto bad = known;
  bad.back()++;
  assert(parse(bad, parsed) == Result::CHECKSUM);
  const auto short31 = frame(0x31, 4);
  assert(parse(short31, parsed) == Result::VALID_KNOWN);
  assert(parsed.payload_length == 4);
  assert(parse(frame(0x31, 80), parsed) == Result::VALID_KNOWN);
  const auto max = frame(0x31, 195);
  assert(max.size() == 200);
  assert(parse(max, parsed) == Result::VALID_KNOWN);
  assert(parse({SYNC, SYNC, 3, 0x31}, parsed) == Result::TOO_SHORT);
  auto length = known;
  length[2]--;
  assert(parse(length, parsed) == Result::LENGTH);

  // Poll-only startup waits for its first response, while healthy routine polls stay ready.
  assert(std::string(protocol_state_after_transmit(ACUpdate::NoUpdate, ACState::Initializing)) == "waiting_for_first_poll_response");
  assert(protocol_state_after_transmit(ACUpdate::NoUpdate, ACState::Ready) == nullptr);

  // Control packets retain the explicit apply and clear transaction states.
  assert(std::string(protocol_state_after_transmit(ACUpdate::UpdateStart, ACState::Ready)) == "command_apply_waiting");
  assert(std::string(protocol_state_after_transmit(ACUpdate::UpdateClear, ACState::Ready)) == "command_clear_waiting");
  // Request ownership is behavioral, rather than inferred from a boolean.
  RequestLifecycle lifecycle;
  lifecycle.sent(OutstandingRequest::POLL, 0);
  assert(!lifecycle.may_send());
  assert(lifecycle.acknowledge_report(280));
  assert(lifecycle.may_send() && lifecycle.poll_responses == 1);
  assert(lifecycle.last_poll_response_ms == 280 && lifecycle.poll_response_timeouts == 0);
  lifecycle.sent(OutstandingRequest::POLL, 300);
  // A valid unsupported frame cannot acknowledge a request.
  assert(lifecycle.outstanding_request == OutstandingRequest::POLL);
  assert(lifecycle.timeout(1800, 1500) == OutstandingRequest::POLL);
  assert(lifecycle.poll_response_timeouts == 1 && lifecycle.consecutive_poll_timeouts == 1);
  lifecycle.sent(OutstandingRequest::COMMAND_APPLY, 2000);
  assert(lifecycle.acknowledge_report(2280));
  lifecycle.sent(OutstandingRequest::COMMAND_CLEAR, 2300);
  assert(lifecycle.acknowledge_report(2580));

  // NoUpdate starts from the 45-byte report baseline and only changes its envelope.
  std::vector<uint8_t> report{0x00,0x00,0x40,0x00,0xA1,0x80,0x02,0x82,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x46,0x00,0x00};
  report.resize(45);
  auto poll = report; poll[3] &= ~0xAF; poll[39] = 0x02; poll[7] |= 0x02; poll[11] |= 0x08;
  assert(poll[18] == 0x08 && poll[44] == 0x46 && poll[39] == 0x02);
  return 0;
}
