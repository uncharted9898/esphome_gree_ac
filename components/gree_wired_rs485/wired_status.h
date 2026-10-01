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

struct FF40Status {
  uint8_t body_length{0};
  UnitSignature unit_signature{0x00, 0x00, 0x00};
  bool registered_layout{false};
  std::vector<uint8_t> payload;
  std::vector<uint8_t> appendix;
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
  return true;
}

}  // namespace status
}  // namespace gree_wired_rs485
}  // namespace esphome
