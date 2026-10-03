#include <cassert>
#include <cstdint>
#include <vector>

#include "../components/gree_wired_rs485/wired_status.h"

using namespace esphome::gree_wired_rs485;

static std::vector<uint8_t> registered_status_frame() {
  return {
      0x7E, 0x7E, 0xFF, 0x40, 0x11, 0x29, 0x09, 0x30, 0x83, 0x7F,
      0x70, 0x0E, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x02, 0x11, 0x1B,
      0x00, 0x00, 0x04, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x0B,
  };
}

int main() {
  protocol::ParsedFrame parsed;
  const auto raw = registered_status_frame();
  assert(protocol::parse_frame(raw, parsed));

  status::FF40Status status_frame;
  assert(status::decode_ff40(parsed, status_frame));
  assert(status_frame.body_length == status::REGISTERED_BODY_LENGTH);
  assert(status_frame.registered_layout);
  assert(status_frame.unit_signature[0] == 0x09);
  assert(status_frame.unit_signature[1] == 0x30);
  assert(status_frame.unit_signature[2] == 0x83);
  assert(status_frame.payload.size() == 40);
  assert(status_frame.appendix.size() == 18);
  assert(status_frame.appendix[0] == 0x11);
  assert(status_frame.appendix[1] == 0x1B);
  assert(status_frame.appendix[5] == 0x28);
  assert(status_frame.has_registered_setpoint_candidate);
  assert(status_frame.registered_setpoint_x2_candidate == 0x28);
  assert(status_frame.registered_setpoint_celsius_candidate == 20.0f);
  assert(status_frame.appendix.back() == 0x20);
  uint8_t byte = 0;
  assert(status::payload_byte(status_frame, 0, byte) && byte == 0x09);
  assert(!status::payload_byte(status_frame, 40, byte));

  return 0;
}
