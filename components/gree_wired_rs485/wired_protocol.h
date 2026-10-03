#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome {
namespace gree_wired_rs485 {
namespace protocol {

static constexpr uint8_t SYNC = 0x7E;
static constexpr uint8_t MESSAGE_TYPE = 0x11;
static constexpr size_t HEADER_SIZE = 6;
static constexpr size_t MIN_BODY_LENGTH = 1;  // Includes the trailing XOR checksum.
static constexpr size_t MAX_BODY_LENGTH = 128;
static constexpr size_t MIN_FRAME_SIZE = HEADER_SIZE + MIN_BODY_LENGTH;
static constexpr size_t MAX_FRAME_SIZE = HEADER_SIZE + MAX_BODY_LENGTH;

enum class RouteKind : uint8_t {
  ROUTE_00_FF,
  ROUTE_FF_00,
  ROUTE_FF_40,
  UNKNOWN,
};

enum class FrameClass : uint8_t {
  REFERENCE_LAYOUT,
  KNOWN_ROUTE_VARIANT,
  UNKNOWN_ROUTE,
  UNEXPECTED_MESSAGE_TYPE,
};

enum class FrameRole : uint8_t {
  CONTROLLER_POLL_0E,
  CONTROLLER_STATE_LEGACY_15,
  CONTROLLER_STATE_22,
  INDOOR_STATUS_16,
  INDOOR_STATUS_17,
  INDOOR_STATUS_REGISTERED_29,
  KNOWN_ROUTE_VARIANT,
  UNKNOWN_ROUTE,
  UNEXPECTED_MESSAGE_TYPE,
};

struct ParsedFrame {
  uint8_t source{0};
  uint8_t destination{0};
  uint8_t message_type{0};
  uint8_t body_length{0};
  uint8_t checksum{0};
  RouteKind route{RouteKind::UNKNOWN};
  FrameClass frame_class{FrameClass::UNKNOWN_ROUTE};
  FrameRole role{FrameRole::UNKNOWN_ROUTE};
  bool reference_body_length{false};
  std::vector<uint8_t> payload;  // Body without the trailing XOR checksum.
};

inline uint8_t xor_bytes(const std::vector<uint8_t> &bytes) {
  uint8_t value = 0;
  for (const auto byte : bytes) value ^= byte;
  return value;
}

inline RouteKind classify_route(uint8_t source, uint8_t destination) {
  if (source == 0x00 && destination == 0xFF) return RouteKind::ROUTE_00_FF;
  if (source == 0xFF && destination == 0x00) return RouteKind::ROUTE_FF_00;
  if (source == 0xFF && destination == 0x40) return RouteKind::ROUTE_FF_40;
  return RouteKind::UNKNOWN;
}

inline bool is_reference_body_length(RouteKind route, uint8_t body_length) {
  switch (route) {
    case RouteKind::ROUTE_00_FF:
      return body_length == 0x0E;
    case RouteKind::ROUTE_FF_00:
      // 0x15 appears in older/alternate controller captures; 0x22 is the
      // proven controller-state layout used by XK76/GKH sessions.
      return body_length == 0x15 || body_length == 0x22;
    case RouteKind::ROUTE_FF_40:
      // 0x16/0x17 are unregistered/startup layouts. 0x29 is the expanded
      // status layout observed after a wired controller is accepted.
      return body_length == 0x16 || body_length == 0x17 || body_length == 0x29;
    case RouteKind::UNKNOWN:
    default:
      return false;
  }
}

inline const char *route_name(RouteKind route) {
  switch (route) {
    case RouteKind::ROUTE_00_FF:
      return "00->FF";
    case RouteKind::ROUTE_FF_00:
      return "FF->00";
    case RouteKind::ROUTE_FF_40:
      return "FF->40";
    case RouteKind::UNKNOWN:
    default:
      return "unknown";
  }
}

inline FrameRole classify_role(RouteKind route, uint8_t message_type,
                               uint8_t body_length) {
  if (message_type != MESSAGE_TYPE) return FrameRole::UNEXPECTED_MESSAGE_TYPE;
  switch (route) {
    case RouteKind::ROUTE_00_FF:
      return body_length == 0x0E ? FrameRole::CONTROLLER_POLL_0E
                                 : FrameRole::KNOWN_ROUTE_VARIANT;
    case RouteKind::ROUTE_FF_00:
      if (body_length == 0x15) return FrameRole::CONTROLLER_STATE_LEGACY_15;
      if (body_length == 0x22) return FrameRole::CONTROLLER_STATE_22;
      return FrameRole::KNOWN_ROUTE_VARIANT;
    case RouteKind::ROUTE_FF_40:
      if (body_length == 0x16) return FrameRole::INDOOR_STATUS_16;
      if (body_length == 0x17) return FrameRole::INDOOR_STATUS_17;
      if (body_length == 0x29) return FrameRole::INDOOR_STATUS_REGISTERED_29;
      return FrameRole::KNOWN_ROUTE_VARIANT;
    case RouteKind::UNKNOWN:
    default:
      return FrameRole::UNKNOWN_ROUTE;
  }
}

inline const char *frame_role_name(FrameRole role) {
  switch (role) {
    case FrameRole::CONTROLLER_POLL_0E:
      return "controller_poll_0e";
    case FrameRole::CONTROLLER_STATE_LEGACY_15:
      return "controller_state_15";
    case FrameRole::CONTROLLER_STATE_22:
      return "controller_state_22";
    case FrameRole::INDOOR_STATUS_16:
      return "indoor_status_16";
    case FrameRole::INDOOR_STATUS_17:
      return "indoor_status_17";
    case FrameRole::INDOOR_STATUS_REGISTERED_29:
      return "indoor_status_registered_29";
    case FrameRole::KNOWN_ROUTE_VARIANT:
      return "known_route_variant";
    case FrameRole::UNEXPECTED_MESSAGE_TYPE:
      return "unexpected_message_type";
    case FrameRole::UNKNOWN_ROUTE:
    default:
      return "unknown_route";
  }
}

inline const char *frame_class_name(FrameClass frame_class) {
  switch (frame_class) {
    case FrameClass::REFERENCE_LAYOUT:
      return "reference_layout";
    case FrameClass::KNOWN_ROUTE_VARIANT:
      return "known_route_variant";
    case FrameClass::UNKNOWN_ROUTE:
      return "unknown_route";
    case FrameClass::UNEXPECTED_MESSAGE_TYPE:
      return "unexpected_message_type";
    default:
      return "unknown";
  }
}

inline bool parse_frame(const std::vector<uint8_t> &raw, ParsedFrame &out) {
  if (raw.size() < MIN_FRAME_SIZE || raw.size() > MAX_FRAME_SIZE) return false;
  if (raw[0] != SYNC || raw[1] != SYNC) return false;

  const uint8_t body_length = raw[5];
  if (body_length < MIN_BODY_LENGTH || body_length > MAX_BODY_LENGTH) return false;
  if (raw.size() != HEADER_SIZE + static_cast<size_t>(body_length)) return false;

  // The protocol checksum is the final byte of Body. XOR across the complete
  // frame, including that byte, must therefore be zero.
  if (xor_bytes(raw) != 0) return false;

  out.source = raw[2];
  out.destination = raw[3];
  out.message_type = raw[4];
  out.body_length = body_length;
  out.checksum = raw.back();
  out.route = classify_route(out.source, out.destination);
  out.reference_body_length = is_reference_body_length(out.route, body_length);
  out.role = classify_role(out.route, out.message_type, body_length);
  out.payload.assign(raw.begin() + HEADER_SIZE, raw.end() - 1);

  if (out.message_type != MESSAGE_TYPE) {
    out.frame_class = FrameClass::UNEXPECTED_MESSAGE_TYPE;
  } else if (out.route == RouteKind::UNKNOWN) {
    out.frame_class = FrameClass::UNKNOWN_ROUTE;
  } else if (out.reference_body_length) {
    out.frame_class = FrameClass::REFERENCE_LAYOUT;
  } else {
    // Do not reject route-compatible length variants. Later Gree controller
    // generations may add fields while retaining the same addressing scheme.
    out.frame_class = FrameClass::KNOWN_ROUTE_VARIANT;
  }

  return true;
}

enum class AssembleResult : uint8_t {
  NONE,
  FRAME_READY,
  INVALID_LENGTH,
};

class FrameAssembler {
 public:
  AssembleResult push(uint8_t byte, std::vector<uint8_t> &completed) {
    if (this->buffer_.empty()) {
      if (byte == SYNC) this->buffer_.push_back(byte);
      return AssembleResult::NONE;
    }

    if (this->buffer_.size() == 1) {
      if (byte == SYNC) {
        this->buffer_.push_back(byte);
      } else {
        this->buffer_.clear();
      }
      return AssembleResult::NONE;
    }

    this->buffer_.push_back(byte);

    if (this->buffer_.size() == HEADER_SIZE) {
      const uint8_t body_length = this->buffer_[5];
      if (body_length < MIN_BODY_LENGTH || body_length > MAX_BODY_LENGTH) {
        this->buffer_.clear();
        return AssembleResult::INVALID_LENGTH;
      }
    }

    if (this->buffer_.size() >= HEADER_SIZE) {
      const size_t expected =
          HEADER_SIZE + static_cast<size_t>(this->buffer_[5]);
      if (this->buffer_.size() == expected) {
        completed = this->buffer_;
        this->buffer_.clear();
        return AssembleResult::FRAME_READY;
      }
      if (this->buffer_.size() > expected || this->buffer_.size() > MAX_FRAME_SIZE) {
        this->buffer_.clear();
        return AssembleResult::INVALID_LENGTH;
      }
    }

    return AssembleResult::NONE;
  }

  bool active() const { return !this->buffer_.empty(); }
  size_t size() const { return this->buffer_.size(); }
  void reset() { this->buffer_.clear(); }

 private:
  std::vector<uint8_t> buffer_;
};

}  // namespace protocol
}  // namespace gree_wired_rs485
}  // namespace esphome
