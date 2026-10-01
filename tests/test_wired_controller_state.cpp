#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

#include "../components/gree_wired_rs485/wired_controller_state.h"

using namespace esphome::gree_wired_rs485;

static std::vector<uint8_t> captured_controller_frame() {
  return {
      0x7E, 0x7E, 0xFF, 0x00, 0x11, 0x22, 0x09, 0x30, 0x83, 0x11,
      0x1B, 0x00, 0x00, 0x10, 0xE0, 0xE0, 0x08, 0x00, 0x28, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x58,
  };
}

int main() {
  auto state = controller::reference_state();
  assert(controller::unit_signature(state) == controller::REFERENCE_UNIT_SIGNATURE);
  assert(controller::mode_power_raw(state) == 0x11);
  assert(controller::secondary_control_raw(state) == 0x1B);
  assert(controller::setpoint_x2(state) == 0x28);
  assert(std::fabs(controller::setpoint_celsius(state) - 20.0f) < 0.001f);
  assert(controller::accept_counter(state) == 0x21);

  const auto encoded = controller::encode(state, 0x21);
  assert(encoded == captured_controller_frame());
  assert(protocol::xor_bytes(encoded) == 0);

  protocol::ParsedFrame parsed;
  assert(protocol::parse_frame(encoded, parsed));
  controller::ControllerState decoded;
  assert(controller::decode(parsed, decoded));
  assert(decoded.payload == state.payload);

  const controller::UnitSignature other_signature{0x0D, 0x30, 0x83};
  controller::set_unit_signature(state, other_signature);
  assert(controller::unit_signature(state) == other_signature);

  controller::set_mode_power_raw(state, 0x19);
  controller::set_secondary_control_raw(state, 0x1B);
  assert(controller::mode_power_raw(state) == 0x19);
  assert(controller::secondary_control_raw(state) == 0x1B);

  assert(controller::set_setpoint_celsius(state, 23.5f));
  assert(controller::setpoint_x2(state) == 47);
  assert(std::fabs(controller::setpoint_celsius(state) - 23.5f) < 0.001f);

  const auto changed = controller::encode(state, 0x27);
  assert(changed[protocol::HEADER_SIZE + controller::ACCEPT_COUNTER_PAYLOAD_INDEX] == 0x27);
  assert(protocol::xor_bytes(changed) == 0);

  return 0;
}
