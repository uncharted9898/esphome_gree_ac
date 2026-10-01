#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "wired_controller_state.h"

namespace esphome {
namespace gree_wired_rs485 {
namespace registration {

using UnitSignature = controller::UnitSignature;

static constexpr size_t COUNTER_INDEX =
    protocol::HEADER_SIZE + controller::ACCEPT_COUNTER_PAYLOAD_INDEX;
static constexpr UnitSignature REFERENCE_UNIT_SIGNATURE =
    controller::REFERENCE_UNIT_SIGNATURE;
static constexpr std::array<uint8_t, 10> ACCEPT_COUNTERS = {
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x20};

inline uint8_t xor_checksum(const uint8_t *data, size_t len_without_checksum) {
  uint8_t value = 0;
  for (size_t i = 0; i < len_without_checksum; ++i) value ^= data[i];
  return value;
}

inline uint8_t counter_for_attempt(size_t attempt) {
  return ACCEPT_COUNTERS[attempt % ACCEPT_COUNTERS.size()];
}

inline std::vector<uint8_t> make_frame(size_t attempt,
                                       const UnitSignature &unit_signature) {
  auto state = controller::reference_state(unit_signature);
  return controller::encode(state, counter_for_attempt(attempt));
}

inline std::vector<uint8_t> make_frame(size_t attempt) {
  return make_frame(attempt, REFERENCE_UNIT_SIGNATURE);
}

}  // namespace registration
}  // namespace gree_wired_rs485
}  // namespace esphome
