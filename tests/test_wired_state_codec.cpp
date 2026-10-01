#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

#include "../components/gree_wired_rs485/wired_controller_state.h"
#include "../components/gree_wired_rs485/wired_protocol.h"
#include "../components/gree_wired_rs485/wired_status.h"

using namespace esphome::gree_wired_rs485;

static protocol::ParsedFrame parse(const std::vector<uint8_t> &raw) {
  protocol::ParsedFrame out;
  assert(protocol::parse_frame(raw, out));
  return out;
}

int main() {
  auto state = controller::reference_state({0x09, 0x30, 0x83});
  assert(controller::mode_power_raw(state) == 0x11);
  assert(controller::secondary_control_raw(state) == 0x1B);
  assert(controller::setpoint_x2(state) == 0x28);
  assert(std::fabs(controller::setpoint_celsius(state) - 20.0f) < 0.001f);
  assert(controller::accept_counter(state) == 0x21);

  controller::set_mode_power_raw(state, 0x19);
  assert(controller::mode_power_raw(state) == 0x19);
  assert(controller::set_setpoint_celsius(state, 23.5f));
  assert(controller::setpoint_x2(state) == 0x2F);

  const auto encoded = controller::encode(state, 0x24);
  assert(encoded.size() == protocol::HEADER_SIZE + controller::BODY_LENGTH);
  assert(protocol::xor_bytes(encoded) == 0);
  const auto decoded_frame = parse(encoded);
  assert(decoded_frame.route == protocol::RouteKind::ROUTE_FF_00);
  assert(decoded_frame.body_length == 0x22);
  assert(decoded_frame.frame_class == protocol::FrameClass::REFERENCE_LAYOUT);

  controller::ControllerState decoded_state;
  assert(controller::decode(decoded_frame, decoded_state));
  assert(controller::mode_power_raw(decoded_state) == 0x19);
  assert(controller::setpoint_x2(decoded_state) == 0x2F);
  assert(controller::accept_counter(decoded_state) == 0x24);

  const std::vector<uint8_t> registered_status = {
      0x7E, 0x7E, 0xFF, 0x40, 0x11, 0x29, 0x09, 0x30, 0x83, 0x7F,
      0x70, 0x0E, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x02, 0x11, 0x1B,
      0x00, 0x00, 0x04, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x0B};
  const auto status_frame = parse(registered_status);
  assert(status_frame.frame_class == protocol::FrameClass::REFERENCE_LAYOUT);

  status::FF40Status status_decoded;
  assert(status::decode_ff40(status_frame, status_decoded));
  assert(status_decoded.registered_layout);
  assert(status_decoded.body_length == 0x29);
  assert(status_decoded.unit_signature[0] == 0x09);
  assert(status_decoded.unit_signature[1] == 0x30);
  assert(status_decoded.unit_signature[2] == 0x83);
  assert(!status_decoded.appendix.empty());
  assert(status_decoded.appendix[0] == 0x11);
  assert(status_decoded.appendix[1] == 0x1B);

  return 0;
}
