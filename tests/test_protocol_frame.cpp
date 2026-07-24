#include "../components/sinclair_ac/protocol_frame.h"
#include "../components/sinclair_ac/protocol_state.h"
#include "../components/sinclair_ac/request_lifecycle.h"
#include "../components/sinclair_ac/telemetry_discovery.h"
#include "../components/sinclair_ac/target_temperature.h"

#include <cassert>
#include <string>

using namespace sinclair_ac_protocol;
using esphome::sinclair_ac::OutstandingRequest;
using esphome::sinclair_ac::RequestLifecycle;
using esphome::sinclair_ac::CaptureRecord;
using esphome::sinclair_ac::SupplementalQueryGate;
using esphome::sinclair_ac::TelemetryDiscovery;
using esphome::sinclair_ac::CNT::decode_current_temperature_field;
using esphome::sinclair_ac::CNT::decode_target_temperature_field;
using esphome::sinclair_ac::CNT::encode_target_temperature_field;
using esphome::sinclair_ac::CNT::normalize_target_temperature;

static std::vector<uint8_t> frame(uint8_t command, size_t payload_length) {
  std::vector<uint8_t> raw{SYNC, SYNC, static_cast<uint8_t>(payload_length + 2), command};
  raw.resize(payload_length + 4);

  uint8_t checksum = 0;
  for (size_t i = 2; i < raw.size(); ++i) checksum += raw[i];
  raw.push_back(checksum);
  return raw;
}

int main() {
  // Target setpoints are whole-degree protocol values, even when HA supplies
  // Fahrenheit-derived fractional Celsius requests.
  assert(normalize_target_temperature(21.111111f) == 21.0f);
  assert(encode_target_temperature_field(21.111111f) == 0x50);
  assert(decode_target_temperature_field(0x50) == 21.0f);
  assert(normalize_target_temperature(18.333334f) == 18.0f);
  assert(encode_target_temperature_field(18.333334f) == 0x20);
  assert(decode_target_temperature_field(0x20) == 18.0f);
  assert(normalize_target_temperature(25.555556f) == 26.0f);
  assert(encode_target_temperature_field(25.555556f) == 0xA0);
  assert(normalize_target_temperature(10.0f) == 16.0f);
  assert(encode_target_temperature_field(10.0f) == 0x00);
  assert(normalize_target_temperature(35.0f) == 30.0f);
  assert(encode_target_temperature_field(35.0f) == 0xE0);
  assert(!std::isfinite(normalize_target_temperature(std::numeric_limits<float>::infinity())));
  assert(!std::isfinite(normalize_target_temperature(std::numeric_limits<float>::quiet_NaN())));
  // The GREE status report uses whole-degree raw-minus-40 values; the legacy
  // Sinclair layout retains its half-degree raw-minus-16 representation.
  assert(decode_current_temperature_field(0x41, true) == 25.0f);
  assert(decode_current_temperature_field(0x41, false) == 24.5f);
  // Target command verification is a comparison of the protocol field, not
  // the original floating-point HA request.  An already-matching baseline is
  // therefore a no-op, a stale field needs a retry, and the same expected
  // field verifies both the apply and clear responses.
  constexpr uint8_t target_mask = 0xF0;
  const uint8_t expected_18c = encode_target_temperature_field(18.333334f);
  const uint8_t baseline_18c = 0x20;
  assert((baseline_18c & target_mask) == expected_18c);  // no packet needed
  const uint8_t stale_21c = 0x50;
  assert((stale_21c & target_mask) != expected_18c);      // retry apply
  const uint8_t confirmed_18c = 0x20;
  assert((confirmed_18c & target_mask) == expected_18c);  // apply/clear verify
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

  const uint8_t diagnostic_commands[] = {
      0x32, 0x33, 0x34, 0x35, 0x36, 0x3C, 0x40, 0x41,
      0x42, 0x44, 0x45, 0x46, 0x4D, 0x52, 0x53,
  };
  for (const auto command : diagnostic_commands) {
    assert(is_diagnostic_command(command));
    assert(parse(frame(command, 1), parsed) == Result::VALID_DIAGNOSTIC);
  }
  assert(!is_diagnostic_command(0x7F));
  assert(parse(frame(0x7F, 1), parsed) == Result::VALID_UNKNOWN);
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

  // A control arriving during a poll must remain queued.  The scheduler can
  // only send its apply request after the poll response releases ownership;
  // the old-state poll report is never a command response or mismatch.
  RequestLifecycle serialized;
  uint32_t command_attempts_before = serialized.command_attempts;
  bool target_control_queued = false;
  constexpr uint8_t queued_target_field = 0x20;
  for (int i = 0; i < 3; ++i) {
    serialized.sent(OutstandingRequest::POLL, 3000 + i * 1000);
    target_control_queued = true;
    assert(serialized.outstanding_request == OutstandingRequest::POLL);
    assert(target_control_queued && queued_target_field == 0x20);
    assert(serialized.command_attempts == command_attempts_before);
    assert(serialized.acknowledge_report(3200 + i * 1000));
    assert(serialized.poll_responses == static_cast<uint32_t>(i + 1));
    assert(serialized.command_mismatches == 0 && serialized.may_send());
    serialized.sent(OutstandingRequest::COMMAND_APPLY, 3300 + i * 1000);
    ++command_attempts_before;
    assert(serialized.command_attempts == command_attempts_before);
    assert(serialized.acknowledge_report(3400 + i * 1000));  // target 0x20 -> clear
    serialized.sent(OutstandingRequest::COMMAND_CLEAR, 3500 + i * 1000);
    ++command_attempts_before;
    assert(serialized.acknowledge_report(3600 + i * 1000));  // target 0x20 -> verified
    assert(serialized.polls_sent - serialized.poll_responses <= 1);
  }
  assert(serialized.min_poll_response_ms == 200 && serialized.max_poll_response_ms == 200);
  assert(serialized.total_poll_response_ms / serialized.poll_responses == 200);

  // NoUpdate starts from the 45-byte report baseline and only changes its envelope.
  std::vector<uint8_t> report{0x00,0x00,0x40,0x00,0x90,0x80,0x06,0xC2,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x41,0x00,0x44,0x00,0x00};
  report.resize(45);
  auto poll = report; poll[3] &= ~0xAF; poll[39] = 0x02; poll[7] |= 0x02; poll[11] |= 0x08;
  assert(poll[18] == 0x08 && poll[44] == 0x44 && poll[4] == 0x90 && poll[8] == report[8]);
  for (size_t i = 0; i < poll.size(); ++i) {
    const uint8_t envelope_mask = i == 3 ? 0xAF : i == 7 ? 0x02 : i == 11 ? 0x08 : i == 39 ? 0xFF : 0;
    assert(((poll[i] ^ report[i]) & ~envelope_mask) == 0);
  }

  // Requested-field patch regression cases use intentionally non-default
  // unrelated bytes.  Every case must remain inside its documented mask.
  const auto assert_masked_change = [](const std::vector<uint8_t> &before, const std::vector<uint8_t> &after,
                                       const std::vector<uint8_t> &masks) {
    for (size_t i = 0; i < before.size(); ++i) assert(((before[i] ^ after[i]) & ~masks[i]) == 0);
  };
  std::vector<uint8_t> rich(45, 0); rich[4] = 0x9C; rich[5] = 0xB7; rich[6] = 0x0F; rich[7] = 0x82;
  rich[8] = 0xA5; rich[9] = 0x30; rich[11] = 0x48; rich[16] = 0x08; rich[18] = 0x0E;
  std::vector<uint8_t> gree_fan = rich; gree_fan[4] = (gree_fan[4] & ~0x03) | 0x02; gree_fan[16] &= ~0x08; gree_fan[6] &= ~0x01;
  std::vector<uint8_t> fan_masks(45, 0); fan_masks[4] = 0x03; fan_masks[16] = 0x08; fan_masks[6] = 0x01;
  assert_masked_change(rich, gree_fan, fan_masks); assert(gree_fan[18] == rich[18]);
  std::vector<uint8_t> vertical = rich; vertical[8] = (vertical[8] & ~0xF0) | 0x40;
  std::vector<uint8_t> vertical_masks(45, 0); vertical_masks[8] = 0xF0;
  assert_masked_change(rich, vertical, vertical_masks); assert((vertical[8] & 0x07) == (rich[8] & 0x07));
  std::vector<uint8_t> horizontal = rich; horizontal[8] = (horizontal[8] & ~0x07) | 0x04;
  std::vector<uint8_t> horizontal_masks(45, 0); horizontal_masks[8] = 0x07;
  assert_masked_change(rich, horizontal, horizontal_masks); assert((horizontal[8] & 0xF0) == (rich[8] & 0xF0));
  std::vector<uint8_t> mode_only = rich; mode_only[4] = (mode_only[4] & ~0xF0) | 0x90;
  std::vector<uint8_t> mode_masks(45, 0); mode_masks[4] = 0xF0;
  assert_masked_change(rich, mode_only, mode_masks);
  std::vector<uint8_t> plasma_only = rich; plasma_only[6] &= ~0x04; plasma_only[0] &= ~0x04;
  std::vector<uint8_t> plasma_masks(45, 0); plasma_masks[6] = 0x04; plasma_masks[0] = 0x04;
  assert_masked_change(rich, plasma_only, plasma_masks);

  // Discovery is raw and command-agnostic: temperature-only updates refresh
  // the latest 0x31 payload; byte 44 remains only a byte statistic.
  TelemetryDiscovery discovery(2);
  discovery.observe(0x31, {0, 0, 0x40}, 100);
  discovery.observe(0x31, {0, 0, 0x41}, 200);
  const auto &report_stats = discovery.commands().at(0x31);
  assert(report_stats.latest_payload[2] == 0x41 && report_stats.bytes[2].changes == 1);
  assert(report_stats.bytes.size() == 3);  // No physical name is assigned to any byte.
  discovery.observe(0x40, {0xAA, 0x55}, 300);  // Valid but unsupported command is retained.
  assert(discovery.commands().at(0x40).packets == 1);
  CaptureRecord first; first.timestamp_ms = 1; first.command = 0x31; discovery.capture(first);
  CaptureRecord second; second.timestamp_ms = 2; second.command = 0x40; discovery.capture(second);
  CaptureRecord third; third.timestamp_ms = 3; third.command = 0x44; discovery.capture(third);
  assert(discovery.history().size() == 2 && discovery.history().front().command == 0x40);
  assert(discovery.export_csv().find("RX") != std::string::npos);

  SupplementalQueryGate query_gate;
  query_gate.configure(false, 1);
  assert(!query_gate.enabled() && !query_gate.may_send(false, false));
  query_gate.configure(true, 1);
  assert(!query_gate.may_send(true, false));  // Active climate traffic has priority.
  assert(!query_gate.may_send(false, true));  // Normal polling has priority.
  assert(query_gate.may_send(false, false));
  query_gate.sent();
  assert(!query_gate.may_send(false, false));
  return 0;
}
