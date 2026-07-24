#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace esphome {
namespace sinclair_ac {

// Observational data only. This intentionally contains no physical-unit names
// beyond the independently confirmed 0x31 byte-42 indoor-temperature mapping.
struct TelemetryField {
  uint8_t command;
  uint8_t byte_index;
  uint8_t byte_width;
  uint32_t mask;
  uint8_t shift;
  bool signed_value;
  float scale;
  float offset;
  uint32_t unavailable_value;
};

enum class TelemetryProfile : uint8_t { GENERIC_SINCLAIR, GREE_LIVO_GEN3, GREE_VARO_GEN4 };
// Production profiles intentionally contain no unconfirmed field mappings.
// Add entries only with a command, byte/mask, scale, offset, and sentinel.
struct TelemetryProfileMapping {
  TelemetryProfile profile;
  const TelemetryField *fields;
  size_t field_count;
};

struct ByteDiscoveryStats {
  uint8_t latest{0}, minimum{0}, maximum{0}, changed_bits{0};
  uint32_t changes{0}, last_change_ms{0};
  bool seen{false};

  void observe(uint8_t value, uint32_t now) {
    if (!seen) {
      latest = minimum = maximum = value;
      seen = true;
      return;
    }
    if (latest != value) {
      changed_bits |= latest ^ value;
      changes++;
      last_change_ms = now;
    }
    latest = value;
    minimum = std::min(minimum, value);
    maximum = std::max(maximum, value);
  }
};

struct CommandDiscoveryStats {
  uint8_t command{0};
  uint16_t payload_length{0};
  uint32_t first_seen_ms{0}, last_seen_ms{0}, packets{0}, changes{0};
  std::vector<uint8_t> latest_payload, previous_payload;
  std::vector<ByteDiscoveryStats> bytes;

  void observe(const std::vector<uint8_t> &payload, uint32_t now) {
    if (packets == 0) first_seen_ms = now;
    const bool changed = packets != 0 && latest_payload != payload;
    if (changed) {
      previous_payload = latest_payload;
      changes++;
    }
    payload_length = payload.size();
    last_seen_ms = now;
    packets++;
    latest_payload = payload;
    if (bytes.size() < payload.size()) bytes.resize(payload.size());
    for (size_t i = 0; i < payload.size(); i++) bytes[i].observe(payload[i], now);
  }
};

struct CaptureRecord {
  uint32_t timestamp_ms{0};
  bool transmitted{false};
  uint8_t command{0};
  std::vector<uint8_t> payload;
  uint8_t decoded_mode{0};
  std::string requested_fan;
  float target_temperature{0};
  float indoor_temperature{0};
  std::string pending_command;
};

class TelemetryDiscovery {
 public:
  explicit TelemetryDiscovery(uint8_t history_depth = 16) { set_history_depth(history_depth); }

  void set_history_depth(uint8_t depth) {
    history_depth_ = std::max<uint8_t>(1, depth);
    trim_history();
  }

  uint8_t history_depth() const { return history_depth_; }

  void observe(uint8_t command, const std::vector<uint8_t> &payload, uint32_t now) {
    commands_[command].command = command;
    commands_[command].observe(payload, now);
    last_command_ = command;
  }

  // Identical routine 0x01 TX and 0x31 RX records add no discovery value and
  // previously erased rare 0x32/0x33/0x34/0x35/0x40/0x44 responses within
  // seconds. Keep changed routine records and every non-routine record.
  bool capture(const CaptureRecord &record) {
    const uint16_t stream = static_cast<uint16_t>((record.transmitted ? 0x100 : 0) | record.command);
    const bool routine = (record.transmitted && record.command == 0x01) ||
                         (!record.transmitted && record.command == 0x31);
    const auto previous = history_last_payloads_.find(stream);
    if (routine && previous != history_last_payloads_.end() && previous->second == record.payload) return false;
    if (routine) history_last_payloads_[stream] = record.payload;
    history_.push_back(record);
    trim_history();
    return true;
  }

  const std::map<uint8_t, CommandDiscoveryStats> &commands() const { return commands_; }
  const std::deque<CaptureRecord> &history() const { return history_; }

  std::string summary() const {
    if (commands_.empty()) return "no captures";
    const auto &s = commands_.at(last_command_);
    std::ostringstream out;
    out << "cmd=0x" << hex(last_command_) << " changed=[";
    bool first = true;
    for (size_t i = 0; i < s.bytes.size(); i++) {
      if (!s.bytes[i].changes) continue;
      if (!first) out << ',';
      out << i;
      first = false;
    }
    out << ']';
    for (size_t i = 0; i < s.bytes.size(); i++) {
      if (!s.bytes[i].changes) continue;
      const auto &b = s.bytes[i];
      out << " b" << i << "=0x" << hex(b.latest) << " min=0x" << hex(b.minimum)
          << " max=0x" << hex(b.maximum) << " changes=" << b.changes;
    }
    return out.str();
  }

  // ESPHome API text states are serialized into JSON. Keep this comfortably
  // below the observed 5119-byte truncation point while retaining the newest
  // complete records.
  std::string export_csv(size_t max_chars = 3000) const {
    const std::string header =
        "timestamp_ms,direction,command,payload,mode,requested_fan,target_temperature,indoor_temperature,pending_command\n";
    std::vector<std::string> lines;
    lines.reserve(history_.size());
    for (const auto &r : history_) {
      std::ostringstream line;
      line << r.timestamp_ms << ',' << (r.transmitted ? "TX" : "RX") << ",0x" << hex(r.command)
           << ",\"" << payload_hex(r.payload) << "\"," << unsigned(r.decoded_mode) << ",\""
           << r.requested_fan << "\"," << r.target_temperature << ',' << r.indoor_temperature
           << ",\"" << r.pending_command << "\"\n";
      lines.push_back(line.str());
    }

    size_t start = lines.size();
    size_t used = header.size();
    while (start > 0 && used + lines[start - 1].size() <= max_chars) {
      --start;
      used += lines[start].size();
    }

    std::ostringstream out;
    out << header;
    for (size_t i = start; i < lines.size(); ++i) out << lines[i];
    return out.str();
  }

 private:
  static std::string hex(uint8_t value) {
    const char digits[] = "0123456789ABCDEF";
    std::string out;
    out += digits[value >> 4];
    out += digits[value & 15];
    return out;
  }

  static std::string payload_hex(const std::vector<uint8_t> &data) {
    std::ostringstream out;
    for (size_t i = 0; i < data.size(); i++) {
      if (i) out << ' ';
      out << hex(data[i]);
    }
    return out.str();
  }

  void trim_history() {
    while (history_.size() > history_depth_) history_.pop_front();
  }

  uint8_t history_depth_{16}, last_command_{0};
  std::map<uint8_t, CommandDiscoveryStats> commands_;
  std::deque<CaptureRecord> history_;
  std::map<uint16_t, std::vector<uint8_t>> history_last_payloads_;
};

// Supplemental queries are inert unless explicitly enabled. Climate commands
// and normal polls always win; this gate has no retry or acknowledgement role.
class SupplementalQueryGate {
 public:
  void configure(bool enabled, uint8_t max_attempts) {
    enabled_ = enabled;
    max_attempts_ = max_attempts;
  }
  bool enabled() const { return enabled_; }
  bool may_send(bool climate_request_active, bool normal_poll_due) const {
    return enabled_ && !outstanding_ && !climate_request_active && !normal_poll_due && attempts_ < max_attempts_;
  }
  void sent() {
    outstanding_ = true;
    attempts_++;
  }
  void complete() { outstanding_ = false; }

 private:
  bool enabled_{false}, outstanding_{false};
  uint8_t attempts_{0}, max_attempts_{0};
};

}  // namespace sinclair_ac
}  // namespace esphome
