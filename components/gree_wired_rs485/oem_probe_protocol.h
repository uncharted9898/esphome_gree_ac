#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome {
namespace gree_wired_rs485 {
namespace oem {

static constexpr uint8_t SYNC = 0x7E;
static constexpr size_t MAX_FRAME_SIZE = 200;
static constexpr size_t IDENTITY_FRAME_SIZE = 19;
static constexpr size_t MAC_REPORT_FRAME_SIZE = 16;
static constexpr size_t STARTUP_SYNC_FRAME_SIZE = 29;
static constexpr size_t CONTROL_PAYLOAD_SIZE = 45;

static constexpr std::array<uint8_t, IDENTITY_FRAME_SIZE> BOOT_IDENTITY{
    0x7E, 0x7E, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x28, 0x1E, 0x19, 0x23, 0x23, 0x00, 0xBA,
};

template<size_t N>
constexpr uint8_t additive_checksum(const std::array<uint8_t, N> &frame) {
  uint8_t checksum = 0;
  for (size_t i = 2; i + 1 < frame.size(); ++i) {
    checksum = static_cast<uint8_t>(checksum + frame[i]);
  }
  return checksum;
}

inline uint8_t additive_checksum(const std::vector<uint8_t> &frame) {
  uint8_t checksum = 0;
  if (frame.size() < 4) return checksum;
  for (size_t i = 2; i + 1 < frame.size(); ++i) {
    checksum = static_cast<uint8_t>(checksum + frame[i]);
  }
  return checksum;
}

constexpr std::array<uint8_t, MAC_REPORT_FRAME_SIZE> build_mac_report(
    const std::array<uint8_t, 6> &mac) {
  std::array<uint8_t, MAC_REPORT_FRAME_SIZE> frame{
      0x7E, 0x7E, 0x0D, 0x04, 0x07, 0x00, 0x00, 0x00,
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], 0x00, 0x00,
  };
  frame.back() = additive_checksum(frame);
  return frame;
}

constexpr std::array<uint8_t, STARTUP_SYNC_FRAME_SIZE> build_startup_sync(
    uint8_t module_state = 0x01) {
  std::array<uint8_t, STARTUP_SYNC_FRAME_SIZE> frame{};
  frame[0] = 0x7E;
  frame[1] = 0x7E;
  frame[2] = 0x1A;
  frame[3] = 0x03;
  // Neutral selector/status request. The audited RTL V2/V3 firmware leaves
  // both time-context triples and the date/time fields zero until synchronized.
  frame[26] = module_state;
  frame[27] = 0x00;
  frame[28] = additive_checksum(frame);
  return frame;
}

struct ParsedFrame {
  std::vector<uint8_t> raw;
  uint8_t declared_length{0};
  uint8_t command{0};
  std::vector<uint8_t> payload;
};

inline bool parse_frame(const std::vector<uint8_t> &raw, ParsedFrame &out) {
  out = ParsedFrame{};
  if (raw.size() < 5 || raw.size() > MAX_FRAME_SIZE) return false;
  if (raw[0] != SYNC || raw[1] != SYNC) return false;
  const size_t expected = static_cast<size_t>(raw[2]) + 3U;
  if (raw[2] < 2 || expected != raw.size()) return false;
  if (additive_checksum(raw) != raw.back()) return false;

  out.raw = raw;
  out.declared_length = raw[2];
  out.command = raw[3];
  out.payload.assign(raw.begin() + 4, raw.end() - 1);
  return true;
}

inline bool accepted_information_44(const ParsedFrame &frame) {
  if (frame.command != 0x44 || frame.payload.size() < 24) return false;
  const uint32_t mid =
      static_cast<uint32_t>(frame.payload[0]) |
      (static_cast<uint32_t>(frame.payload[1]) << 8U) |
      (static_cast<uint32_t>(frame.payload[2]) << 16U) |
      (static_cast<uint32_t>(frame.payload[3]) << 24U);
  const uint32_t vendor =
      (static_cast<uint32_t>(frame.payload[20]) << 24U) |
      (static_cast<uint32_t>(frame.payload[21]) << 16U) |
      (static_cast<uint32_t>(frame.payload[22]) << 8U) |
      static_cast<uint32_t>(frame.payload[23]);
  return mid != 0 && vendor != 0;
}

// Build one normal no-change command-0x01 poll from a real command-0x31
// report payload. This is intentionally unavailable until the target has
// supplied enough state bytes to echo back safely.
inline std::vector<uint8_t> build_nochange_poll(
    const std::vector<uint8_t> &report_payload) {
  if (report_payload.size() < CONTROL_PAYLOAD_SIZE) return {};

  std::vector<uint8_t> payload(
      report_payload.begin(), report_payload.begin() + CONTROL_PAYLOAD_SIZE);
  payload[3] = static_cast<uint8_t>(payload[3] & static_cast<uint8_t>(~0xAFU));
  payload[7] = static_cast<uint8_t>(payload[7] | 0x02U);
  payload[11] = static_cast<uint8_t>(payload[11] | 0x08U);
  payload[39] = 0x02;

  std::vector<uint8_t> frame;
  frame.reserve(CONTROL_PAYLOAD_SIZE + 5U);
  frame.push_back(SYNC);
  frame.push_back(SYNC);
  frame.push_back(static_cast<uint8_t>(CONTROL_PAYLOAD_SIZE + 2U));
  frame.push_back(0x01);
  frame.insert(frame.end(), payload.begin(), payload.end());
  frame.push_back(0x00);
  frame.back() = additive_checksum(frame);
  return frame;
}

enum class AssembleResult : uint8_t {
  NONE,
  FRAME_READY,
  INVALID_LENGTH,
};

class FrameAssembler {
 public:
  AssembleResult push(uint8_t byte, std::vector<uint8_t> &complete) {
    complete.clear();

    if (buffer_.empty()) {
      if (byte == SYNC) buffer_.push_back(byte);
      return AssembleResult::NONE;
    }

    if (buffer_.size() == 1) {
      if (byte == SYNC) {
        buffer_.push_back(byte);
      } else {
        buffer_.clear();
      }
      return AssembleResult::NONE;
    }

    if (buffer_.size() == 2) {
      const size_t total = static_cast<size_t>(byte) + 3U;
      if (byte < 2 || total > MAX_FRAME_SIZE) {
        buffer_.clear();
        if (byte == SYNC) buffer_.push_back(byte);
        expected_size_ = 0;
        return AssembleResult::INVALID_LENGTH;
      }
      buffer_.push_back(byte);
      expected_size_ = total;
      return AssembleResult::NONE;
    }

    buffer_.push_back(byte);
    if (expected_size_ != 0 && buffer_.size() == expected_size_) {
      complete = buffer_;
      reset();
      return AssembleResult::FRAME_READY;
    }
    if (expected_size_ != 0 && buffer_.size() > expected_size_) {
      reset();
      return AssembleResult::INVALID_LENGTH;
    }
    return AssembleResult::NONE;
  }

  void reset() {
    buffer_.clear();
    expected_size_ = 0;
  }

 private:
  std::vector<uint8_t> buffer_;
  size_t expected_size_{0};
};

}  // namespace oem
}  // namespace gree_wired_rs485
}  // namespace esphome
