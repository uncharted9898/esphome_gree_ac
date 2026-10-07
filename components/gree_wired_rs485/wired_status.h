#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "wired_protocol.h"

namespace esphome {
namespace gree_wired_rs485 {
namespace status {

using UnitSignature = std::array<uint8_t, 3>;

static constexpr uint8_t BASE_BODY_LENGTH_16 = 0x16;
static constexpr uint8_t BASE_BODY_LENGTH_17 = 0x17;
static constexpr uint8_t REGISTERED_BODY_LENGTH = 0x29;
static constexpr size_t REGISTERED_APPENDIX_OFFSET = 22;
static constexpr size_t MAX_PAYLOAD_SIZE = static_cast<size_t>(REGISTERED_BODY_LENGTH) - 1U;
static constexpr size_t REGISTERED_APPENDIX_SETPOINT_X2_INDEX = 5;

struct FF40Status {
  uint8_t body_length{0};
  UnitSignature unit_signature{0x00, 0x00, 0x00};
  bool registered_layout{false};
  std::vector<uint8_t> payload;
  std::vector<uint8_t> appendix;
  bool has_registered_setpoint_candidate{false};
  uint8_t registered_setpoint_x2_candidate{0};
  float registered_setpoint_celsius_candidate{0.0f};
};

inline bool decode_ff40(const protocol::ParsedFrame &frame, FF40Status &out) {
  if (frame.message_type != protocol::MESSAGE_TYPE) return false;
  if (frame.route != protocol::RouteKind::ROUTE_FF_40) return false;
  if (frame.payload.size() < 3) return false;

  out.body_length = frame.body_length;
  out.unit_signature = {frame.payload[0], frame.payload[1], frame.payload[2]};
  out.registered_layout = frame.body_length == REGISTERED_BODY_LENGTH;
  out.payload = frame.payload;
  out.appendix.clear();
  if (frame.payload.size() > REGISTERED_APPENDIX_OFFSET) {
    out.appendix.assign(frame.payload.begin() + REGISTERED_APPENDIX_OFFSET,
                        frame.payload.end());
  }

  // In the registered 0x29 capture the appendix byte at offset 5 mirrors the
  // controller's body[12] setpoint-x2 value (0x28 == 20.0 C). The upstream
  // reverse-engineering thread explicitly calls this a candidate rather than a
  // fully proven field, so expose it as such instead of silently promoting it
  // to authoritative setpoint state.
  if (out.registered_layout &&
      out.appendix.size() > REGISTERED_APPENDIX_SETPOINT_X2_INDEX) {
    out.has_registered_setpoint_candidate = true;
    out.registered_setpoint_x2_candidate =
        out.appendix[REGISTERED_APPENDIX_SETPOINT_X2_INDEX];
    out.registered_setpoint_celsius_candidate =
        static_cast<float>(out.registered_setpoint_x2_candidate) / 2.0f;
  }
  return true;
}

inline bool payload_byte(const FF40Status &status, size_t index, uint8_t &value) {
  if (index >= status.payload.size()) return false;
  value = status.payload[index];
  return true;
}

}  // namespace status
}  // namespace gree_wired_rs485
}  // namespace esphome
