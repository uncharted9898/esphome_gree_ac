#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "wired_protocol.h"

namespace esphome {
namespace gree_wired_rs485 {
namespace controller {

using UnitSignature = std::array<uint8_t, 3>;

static constexpr uint8_t BODY_LENGTH = 0x22;
static constexpr size_t PAYLOAD_SIZE = static_cast<size_t>(BODY_LENGTH) - 1U;

static constexpr size_t UNIT_SIGNATURE_PAYLOAD_INDEX = 0;
static constexpr size_t MODE_POWER_PAYLOAD_INDEX = 3;
static constexpr uint8_t POWER_MASK = 0x08;
static constexpr uint8_t REFERENCE_MODE_POWER_OFF = 0x11;
static constexpr uint8_t REFERENCE_MODE_POWER_COOL_ON = 0x19;
static constexpr size_t SECONDARY_CONTROL_PAYLOAD_INDEX = 4;
static constexpr size_t SETPOINT_X2_PAYLOAD_INDEX = 12;
static constexpr size_t ACCEPT_COUNTER_PAYLOAD_INDEX = 20;

static constexpr UnitSignature REFERENCE_UNIT_SIGNATURE = {0x09, 0x30, 0x83};

struct ControllerState {
  std::array<uint8_t, PAYLOAD_SIZE> payload{};
};

inline ControllerState reference_state(
    const UnitSignature &unit_signature = REFERENCE_UNIT_SIGNATURE) {
  ControllerState state;
  state.payload = {
      0x09, 0x30, 0x83, 0x11, 0x1B, 0x00, 0x00, 0x10,
      0xE0, 0xE0, 0x08, 0x00, 0x28, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x30,
  };
  for (size_t i = 0; i < unit_signature.size(); ++i) {
    state.payload[UNIT_SIGNATURE_PAYLOAD_INDEX + i] = unit_signature[i];
  }
  return state;
}

inline UnitSignature unit_signature(const ControllerState &state) {
  return {
      state.payload[UNIT_SIGNATURE_PAYLOAD_INDEX],
      state.payload[UNIT_SIGNATURE_PAYLOAD_INDEX + 1],
      state.payload[UNIT_SIGNATURE_PAYLOAD_INDEX + 2],
  };
}

inline void set_unit_signature(ControllerState &state,
                               const UnitSignature &unit_signature) {
  for (size_t i = 0; i < unit_signature.size(); ++i) {
    state.payload[UNIT_SIGNATURE_PAYLOAD_INDEX + i] = unit_signature[i];
  }
}

inline uint8_t mode_power_raw(const ControllerState &state) {
  return state.payload[MODE_POWER_PAYLOAD_INDEX];
}

inline void set_mode_power_raw(ControllerState &state, uint8_t value) {
  state.payload[MODE_POWER_PAYLOAD_INDEX] = value;
}

inline bool power_enabled(const ControllerState &state) {
  return (mode_power_raw(state) & POWER_MASK) != 0;
}

inline void set_power_enabled(ControllerState &state, bool enabled) {
  if (enabled) {
    state.payload[MODE_POWER_PAYLOAD_INDEX] |= POWER_MASK;
  } else {
    state.payload[MODE_POWER_PAYLOAD_INDEX] &=
        static_cast<uint8_t>(~POWER_MASK);
  }
}

inline bool set_payload_byte(ControllerState &state, size_t index, uint8_t value) {
  if (index >= state.payload.size()) return false;
  state.payload[index] = value;
  return true;
}

inline uint8_t secondary_control_raw(const ControllerState &state) {
  return state.payload[SECONDARY_CONTROL_PAYLOAD_INDEX];
}

inline void set_secondary_control_raw(ControllerState &state, uint8_t value) {
  state.payload[SECONDARY_CONTROL_PAYLOAD_INDEX] = value;
}

inline uint8_t setpoint_x2(const ControllerState &state) {
  return state.payload[SETPOINT_X2_PAYLOAD_INDEX];
}

inline float setpoint_celsius(const ControllerState &state) {
  return static_cast<float>(setpoint_x2(state)) / 2.0f;
}

inline void set_setpoint_x2(ControllerState &state, uint8_t value) {
  state.payload[SETPOINT_X2_PAYLOAD_INDEX] = value;
}

inline bool set_setpoint_celsius(ControllerState &state, float value) {
  if (value < 0.0f || value > 127.5f) return false;
  const float doubled = value * 2.0f;
  const auto encoded = static_cast<unsigned>(doubled + 0.5f);
  if (encoded > 0xFFU) return false;
  state.payload[SETPOINT_X2_PAYLOAD_INDEX] = static_cast<uint8_t>(encoded);
  return true;
}

inline uint8_t accept_counter(const ControllerState &state) {
  return state.payload[ACCEPT_COUNTER_PAYLOAD_INDEX];
}

inline void set_accept_counter(ControllerState &state, uint8_t value) {
  state.payload[ACCEPT_COUNTER_PAYLOAD_INDEX] = value;
}

inline std::vector<uint8_t> encode(const ControllerState &state,
                                   uint8_t accept_counter_value) {
  std::vector<uint8_t> frame;
  frame.reserve(protocol::HEADER_SIZE + PAYLOAD_SIZE + 1U);
  frame.push_back(protocol::SYNC);
  frame.push_back(protocol::SYNC);
  frame.push_back(0xFF);
  frame.push_back(0x00);
  frame.push_back(protocol::MESSAGE_TYPE);
  frame.push_back(BODY_LENGTH);

  ControllerState encoded = state;
  set_accept_counter(encoded, accept_counter_value);
  frame.insert(frame.end(), encoded.payload.begin(), encoded.payload.end());

  uint8_t checksum = 0;
  for (const auto value : frame) checksum ^= value;
  frame.push_back(checksum);
  return frame;
}

inline bool decode(const protocol::ParsedFrame &frame, ControllerState &out) {
  if (frame.message_type != protocol::MESSAGE_TYPE) return false;
  if (frame.route != protocol::RouteKind::ROUTE_FF_00) return false;
  if (frame.body_length != BODY_LENGTH) return false;
  if (frame.payload.size() != PAYLOAD_SIZE) return false;
  for (size_t i = 0; i < PAYLOAD_SIZE; ++i) out.payload[i] = frame.payload[i];
  return true;
}

}  // namespace controller
}  // namespace gree_wired_rs485
}  // namespace esphome
