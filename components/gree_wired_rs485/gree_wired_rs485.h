#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "wired_protocol.h"

#ifdef USE_ESP32
#include "driver/gpio.h"
#endif

namespace esphome {
namespace gree_wired_rs485 {

static const char *const TAG = "gree_wired_rs485";

class GreeWiredRS485 : public Component, public uart::UARTDevice {
 public:
  void set_frame_timeout(uint32_t timeout_ms) { this->frame_timeout_ms_ = timeout_ms; }
  void set_bus_idle_timeout(uint32_t timeout_ms) { this->bus_idle_timeout_ms_ = timeout_ms; }
  void set_log_frames(bool log_frames) { this->log_frames_ = log_frames; }
  void set_passive_scan(bool passive_scan) { this->passive_scan_ = passive_scan; }
  void set_passive_scan_window(uint32_t window_ms) { this->passive_scan_window_ms_ = window_ms; }
  void set_rx_line_gpio(int gpio) { this->rx_line_gpio_ = gpio; }
  void set_direction_gpio(int gpio) { this->direction_gpio_ = gpio; }

  void set_bytes_received_sensor(sensor::Sensor *s) { this->bytes_received_sensor_ = s; }
  void set_valid_frames_sensor(sensor::Sensor *s) { this->valid_frames_sensor_ = s; }
  void set_checksum_failures_sensor(sensor::Sensor *s) { this->checksum_failures_sensor_ = s; }
  void set_frame_timeouts_sensor(sensor::Sensor *s) { this->frame_timeouts_sensor_ = s; }
  void set_invalid_lengths_sensor(sensor::Sensor *s) { this->invalid_lengths_sensor_ = s; }
  void set_unexpected_type_frames_sensor(sensor::Sensor *s) {
    this->unexpected_type_frames_sensor_ = s;
  }
  void set_reference_layout_frames_sensor(sensor::Sensor *s) {
    this->reference_layout_frames_sensor_ = s;
  }
  void set_route_variant_frames_sensor(sensor::Sensor *s) {
    this->route_variant_frames_sensor_ = s;
  }
  void set_unknown_route_frames_sensor(sensor::Sensor *s) {
    this->unknown_route_frames_sensor_ = s;
  }
  void set_route_00_ff_frames_sensor(sensor::Sensor *s) { this->route_00_ff_frames_sensor_ = s; }
  void set_route_ff_00_frames_sensor(sensor::Sensor *s) { this->route_ff_00_frames_sensor_ = s; }
  void set_route_ff_40_frames_sensor(sensor::Sensor *s) { this->route_ff_40_frames_sensor_ = s; }
  void set_last_source_sensor(sensor::Sensor *s) { this->last_source_sensor_ = s; }
  void set_last_destination_sensor(sensor::Sensor *s) { this->last_destination_sensor_ = s; }
  void set_last_body_length_sensor(sensor::Sensor *s) { this->last_body_length_sensor_ = s; }

  void set_last_frame_sensor(text_sensor::TextSensor *s) { this->last_frame_sensor_ = s; }
  void set_last_payload_sensor(text_sensor::TextSensor *s) { this->last_payload_sensor_ = s; }
  void set_last_route_sensor(text_sensor::TextSensor *s) { this->last_route_sensor_ = s; }
  void set_last_frame_class_sensor(text_sensor::TextSensor *s) {
    this->last_frame_class_sensor_ = s;
  }
  void set_last_changes_sensor(text_sensor::TextSensor *s) { this->last_changes_sensor_ = s; }
  void set_last_invalid_frame_sensor(text_sensor::TextSensor *s) {
    this->last_invalid_frame_sensor_ = s;
  }
  void set_protocol_sensor(text_sensor::TextSensor *s) { this->protocol_sensor_ = s; }
  void set_serial_profile_sensor(text_sensor::TextSensor *s) { this->serial_profile_sensor_ = s; }

  void set_bus_active_sensor(binary_sensor::BinarySensor *s) { this->bus_active_sensor_ = s; }
  void set_listen_only_sensor(binary_sensor::BinarySensor *s) { this->listen_only_sensor_ = s; }
  void set_rx_line_high_sensor(binary_sensor::BinarySensor *s) { this->rx_line_high_sensor_ = s; }
  void set_direction_high_sensor(binary_sensor::BinarySensor *s) { this->direction_high_sensor_ = s; }

  float get_setup_priority() const override { return setup_priority::DATA; }

  void setup() override {
    // This component intentionally has no UART write path. With the Seeed XIAO
    // RS485 expansion board, uart.flow_control_pin remains low while idle, which
    // leaves DE disabled and /RE enabled. Keep the hardware 120-ohm termination
    // switch OFF when joining an already-terminated COM-MANUAL bus.
    ESP_LOGI(TAG, "Starting Gree COM-MANUAL monitor in hardware listen-only mode");
    ESP_LOGI(TAG, "Protocol profile: 1200 baud 8N1, 7E 7E framing, type 0x11, XOR checksum");
    if (this->passive_scan_) {
      ESP_LOGI(TAG, "Passive UART profile scan enabled; RS485 transmitter remains disabled");
      this->scan_profile_started_at_ = millis();
      this->scan_profile_byte_start_ = this->bytes_received_;
      this->publish_serial_profile_();
    } else {
      this->publish_serial_profile_();
    }

    if (this->listen_only_sensor_ != nullptr) this->listen_only_sensor_->publish_state(true);
    if (this->bus_active_sensor_ != nullptr) this->bus_active_sensor_->publish_state(false);
    if (this->protocol_sensor_ != nullptr) {
      this->protocol_sensor_->publish_state("1200-8N1; 7E7E; src,dst,11,len,body; xor=0");
    }
    this->publish_counters_();
    this->publish_line_states_();
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Gree wired-controller RS485 monitor:");
    ESP_LOGCONFIG(TAG, "  Mode: listen-only (no transmit method compiled)");
    ESP_LOGCONFIG(TAG, "  Frame gap timeout: %lu ms",
                  static_cast<unsigned long>(this->frame_timeout_ms_));
    ESP_LOGCONFIG(TAG, "  Bus idle timeout: %lu ms",
                  static_cast<unsigned long>(this->bus_idle_timeout_ms_));
    ESP_LOGCONFIG(TAG, "  Log valid frames: %s", YESNO(this->log_frames_));
    ESP_LOGCONFIG(TAG, "  Passive serial scan: %s", YESNO(this->passive_scan_));
    if (this->passive_scan_) {
      ESP_LOGCONFIG(TAG, "  Passive scan window: %lu ms",
                    static_cast<unsigned long>(this->passive_scan_window_ms_));
    }
    ESP_LOGCONFIG(TAG, "  RX line GPIO: %d", this->rx_line_gpio_);
    ESP_LOGCONFIG(TAG, "  Direction/DE GPIO: %d", this->direction_gpio_);
  }

  void loop() override {
    const uint32_t now = millis();
    if (this->assembler_.active() && this->last_byte_at_ != 0 &&
        static_cast<uint32_t>(now - this->last_byte_at_) > this->frame_timeout_ms_) {
      this->assembler_.reset();
      ++this->frame_timeouts_;
      this->publish_counters_();
      ESP_LOGW(TAG, "Discarded partial COM-MANUAL frame after inter-byte timeout");
    }

    while (this->available()) {
      uint8_t byte = 0;
      if (!this->read_byte(&byte)) break;
      this->last_byte_at_ = millis();
      ++this->bytes_received_;

      std::vector<uint8_t> complete;
      const auto result = this->assembler_.push(byte, complete);
      if (result == protocol::AssembleResult::INVALID_LENGTH) {
        ++this->invalid_lengths_;
        this->publish_counters_();
        ESP_LOGW(TAG, "Discarded COM-MANUAL frame with invalid declared body length");
      } else if (result == protocol::AssembleResult::FRAME_READY) {
        this->process_frame_(complete);
      }
    }

    if (this->bus_active_ && this->last_valid_frame_at_ != 0 &&
        static_cast<uint32_t>(now - this->last_valid_frame_at_) > this->bus_idle_timeout_ms_) {
      this->bus_active_ = false;
      if (this->bus_active_sensor_ != nullptr) this->bus_active_sensor_->publish_state(false);
    }

    if (this->passive_scan_ && !this->scan_locked_ &&
        static_cast<uint32_t>(now - this->scan_profile_started_at_) >=
            this->passive_scan_window_ms_) {
      const uint32_t profile_bytes = this->bytes_received_ - this->scan_profile_byte_start_;
      const auto profile = this->scan_profile_(this->scan_profile_index_);
      ESP_LOGI(TAG, "SCAN profile=%s bytes=%lu",
               profile.name, static_cast<unsigned long>(profile_bytes));

      // A handful of bytes in a short passive window is enough to distinguish
      // real UART activity from the one-byte startup artifact seen during
      // qualification. Lock to the first profile with sustained RX so the raw
      // debugger can capture a contiguous stream.
      if (profile_bytes >= 4) {
        this->scan_locked_ = true;
        ESP_LOGI(TAG, "SCAN locked profile=%s after %lu received bytes",
                 profile.name, static_cast<unsigned long>(profile_bytes));
        this->publish_serial_profile_();
      } else {
        this->scan_profile_index_ = (this->scan_profile_index_ + 1) % PASSIVE_SCAN_PROFILE_COUNT;
        this->apply_scan_profile_(this->scan_profile_index_);
        this->scan_profile_started_at_ = now;
        this->scan_profile_byte_start_ = this->bytes_received_;
      }
    }

    if (this->last_health_log_at_ == 0 ||
        static_cast<uint32_t>(now - this->last_health_log_at_) >= this->health_log_interval_ms_) {
      this->last_health_log_at_ = now;
      // Publish periodically even if electrical RX never forms a valid legacy
      // frame. This separates wiring/UART silence from protocol incompatibility.
      this->publish_counters_();
      const bool rx_recent =
          this->last_byte_at_ != 0 &&
          static_cast<uint32_t>(now - this->last_byte_at_) <= this->bus_idle_timeout_ms_;
      this->publish_line_states_();
      const int rx_level = this->read_gpio_level_(this->rx_line_gpio_);
      const int direction_level = this->read_gpio_level_(this->direction_gpio_);
      ESP_LOGI(TAG,
               "HEALTH profile=%s bytes=%lu valid=%lu xor_fail=%lu invalid_len=%lu "
               "timeouts=%lu rx_recent=%s valid_bus=%s rx_level=%d de_level=%d",
               this->scan_profile_(this->scan_profile_index_).name,
               static_cast<unsigned long>(this->bytes_received_),
               static_cast<unsigned long>(this->valid_frames_),
               static_cast<unsigned long>(this->checksum_failures_),
               static_cast<unsigned long>(this->invalid_lengths_),
               static_cast<unsigned long>(this->frame_timeouts_),
               YESNO(rx_recent), YESNO(this->bus_active_), rx_level, direction_level);
    }
  }

 protected:
  static std::string hex_(const std::vector<uint8_t> &bytes) {
    static const char digits[] = "0123456789ABCDEF";
    std::string out;
    if (bytes.empty()) return out;
    out.reserve(bytes.size() * 3 - 1);
    for (size_t i = 0; i < bytes.size(); ++i) {
      if (i != 0) out.push_back(' ');
      out.push_back(digits[bytes[i] >> 4]);
      out.push_back(digits[bytes[i] & 0x0F]);
    }
    return out;
  }

  static std::string route_text_(uint8_t source, uint8_t destination) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02X->%02X", source, destination);
    return buffer;
  }

  static std::string changed_indices_(const std::vector<uint8_t> &previous,
                                      const std::vector<uint8_t> &current) {
    if (previous.empty()) return "baseline";

    std::string out;
    const size_t common = std::min(previous.size(), current.size());
    for (size_t i = 0; i < common; ++i) {
      if (previous[i] == current[i]) continue;
      char token[40];
      std::snprintf(token, sizeof(token), "%u:%02X>%02X",
                    static_cast<unsigned>(i), previous[i], current[i]);
      if (!out.empty()) out.push_back(' ');
      out += token;
    }
    if (previous.size() != current.size()) {
      char token[40];
      std::snprintf(token, sizeof(token), "len:%u>%u",
                    static_cast<unsigned>(previous.size()),
                    static_cast<unsigned>(current.size()));
      if (!out.empty()) out.push_back(' ');
      out += token;
    }
    if (out.empty()) return "none";
    return out;
  }

  int read_gpio_level_(int gpio) const {
#ifdef USE_ESP32
    if (gpio >= 0) return gpio_get_level(static_cast<gpio_num_t>(gpio));
#endif
    return -1;
  }

  void publish_line_states_() {
    const int rx_level = this->read_gpio_level_(this->rx_line_gpio_);
    const int direction_level = this->read_gpio_level_(this->direction_gpio_);
    if (this->rx_line_high_sensor_ != nullptr && rx_level >= 0) {
      this->rx_line_high_sensor_->publish_state(rx_level != 0);
    }
    if (this->direction_high_sensor_ != nullptr && direction_level >= 0) {
      this->direction_high_sensor_->publish_state(direction_level != 0);
    }
  }

  struct PassiveScanProfile {
    uint32_t baud;
    uart::UARTParityOptions parity;
    const char *name;
  };

  static constexpr size_t PASSIVE_SCAN_PROFILE_COUNT = 8;

  static PassiveScanProfile scan_profile_(size_t index) {
    switch (index % PASSIVE_SCAN_PROFILE_COUNT) {
      case 0:
        return {1200, uart::UART_CONFIG_PARITY_NONE, "1200-8N1"};
      case 1:
        return {4800, uart::UART_CONFIG_PARITY_NONE, "4800-8N1"};
      case 2:
        return {9600, uart::UART_CONFIG_PARITY_NONE, "9600-8N1"};
      case 3:
        return {4800, uart::UART_CONFIG_PARITY_EVEN, "4800-8E1"};
      case 4:
        return {9600, uart::UART_CONFIG_PARITY_EVEN, "9600-8E1"};
      case 5:
        return {2400, uart::UART_CONFIG_PARITY_NONE, "2400-8N1"};
      case 6:
        return {19200, uart::UART_CONFIG_PARITY_NONE, "19200-8N1"};
      case 7:
      default:
        return {38400, uart::UART_CONFIG_PARITY_NONE, "38400-8N1"};
    }
  }

  void publish_serial_profile_() {
    if (this->serial_profile_sensor_ != nullptr) {
      this->serial_profile_sensor_->publish_state(
          this->scan_profile_(this->scan_profile_index_).name);
    }
  }

  void apply_scan_profile_(size_t index) {
    const auto profile = this->scan_profile_(index);
    this->assembler_.reset();

    // Receiver-only qualification: changing UART decode settings does not
    // transmit anything on RS485. The ESP-IDF half-duplex flow-control pin
    // remains in receive state because there is still no write path.
    this->parent_->set_baud_rate(profile.baud);
    this->parent_->set_data_bits(8);
    this->parent_->set_stop_bits(1);
    this->parent_->set_parity(profile.parity);
#if defined(USE_ESP32)
    this->parent_->load_settings(false);
#endif
    ESP_LOGI(TAG, "SCAN listening profile=%s", profile.name);
    this->publish_serial_profile_();
  }

  void process_frame_(const std::vector<uint8_t> &raw) {
    protocol::ParsedFrame frame;
    if (!protocol::parse_frame(raw, frame)) {
      ++this->checksum_failures_;
      if (this->last_invalid_frame_sensor_ != nullptr) {
        this->last_invalid_frame_sensor_->publish_state(hex_(raw));
      }
      this->publish_counters_();
      ESP_LOGW(TAG, "COM-MANUAL frame failed XOR validation: %s", hex_(raw).c_str());
      return;
    }

    ++this->valid_frames_;
    this->last_valid_frame_at_ = millis();
    if (this->passive_scan_ && !this->scan_locked_) {
      this->scan_locked_ = true;
      ESP_LOGI(TAG, "SCAN locked profile=%s after checksum-valid frame",
               this->scan_profile_(this->scan_profile_index_).name);
      this->publish_serial_profile_();
    }
    if (!this->bus_active_) {
      this->bus_active_ = true;
      if (this->bus_active_sensor_ != nullptr) this->bus_active_sensor_->publish_state(true);
    }

    switch (frame.route) {
      case protocol::RouteKind::ROUTE_00_FF:
        ++this->route_00_ff_frames_;
        break;
      case protocol::RouteKind::ROUTE_FF_00:
        ++this->route_ff_00_frames_;
        break;
      case protocol::RouteKind::ROUTE_FF_40:
        ++this->route_ff_40_frames_;
        break;
      case protocol::RouteKind::UNKNOWN:
      default:
        break;
    }

    switch (frame.frame_class) {
      case protocol::FrameClass::REFERENCE_LAYOUT:
        ++this->reference_layout_frames_;
        break;
      case protocol::FrameClass::KNOWN_ROUTE_VARIANT:
        ++this->route_variant_frames_;
        break;
      case protocol::FrameClass::UNKNOWN_ROUTE:
        ++this->unknown_route_frames_;
        break;
      case protocol::FrameClass::UNEXPECTED_MESSAGE_TYPE:
        ++this->unexpected_type_frames_;
        break;
      default:
        break;
    }

    const uint16_t route_key =
        (static_cast<uint16_t>(frame.source) << 8) | frame.destination;
    auto &previous = this->previous_payloads_[route_key];
    const std::string changes = changed_indices_(previous, frame.payload);
    previous = frame.payload;

    const std::string raw_hex = hex_(raw);
    const std::string payload_hex = hex_(frame.payload);
    const std::string route = route_text_(frame.source, frame.destination);
    const char *frame_class = protocol::frame_class_name(frame.frame_class);

    // Publish on every valid source frame, including byte-for-byte duplicates,
    // so Home Assistant timestamps reflect actual bus freshness.
    if (this->last_frame_sensor_ != nullptr) this->last_frame_sensor_->publish_state(raw_hex);
    if (this->last_payload_sensor_ != nullptr) this->last_payload_sensor_->publish_state(payload_hex);
    if (this->last_route_sensor_ != nullptr) this->last_route_sensor_->publish_state(route);
    if (this->last_frame_class_sensor_ != nullptr) {
      this->last_frame_class_sensor_->publish_state(frame_class);
    }
    if (this->last_changes_sensor_ != nullptr) this->last_changes_sensor_->publish_state(changes);
    if (this->last_source_sensor_ != nullptr) this->last_source_sensor_->publish_state(frame.source);
    if (this->last_destination_sensor_ != nullptr) {
      this->last_destination_sensor_->publish_state(frame.destination);
    }
    if (this->last_body_length_sensor_ != nullptr) {
      this->last_body_length_sensor_->publish_state(frame.body_length);
    }

    this->publish_counters_();

    if (this->log_frames_) {
      ESP_LOGI(TAG,
               "RX route=%s type=0x%02X body=%u payload=%u checksum=0x%02X "
               "class=%s changes=[%s] raw=%s",
               route.c_str(), frame.message_type, static_cast<unsigned>(frame.body_length),
               static_cast<unsigned>(frame.payload.size()), frame.checksum, frame_class,
               changes.c_str(), raw_hex.c_str());
    }
  }

  void publish_counters_() {
    if (this->bytes_received_sensor_ != nullptr) {
      this->bytes_received_sensor_->publish_state(this->bytes_received_);
    }
    if (this->valid_frames_sensor_ != nullptr) {
      this->valid_frames_sensor_->publish_state(this->valid_frames_);
    }
    if (this->checksum_failures_sensor_ != nullptr) {
      this->checksum_failures_sensor_->publish_state(this->checksum_failures_);
    }
    if (this->frame_timeouts_sensor_ != nullptr) {
      this->frame_timeouts_sensor_->publish_state(this->frame_timeouts_);
    }
    if (this->invalid_lengths_sensor_ != nullptr) {
      this->invalid_lengths_sensor_->publish_state(this->invalid_lengths_);
    }
    if (this->unexpected_type_frames_sensor_ != nullptr) {
      this->unexpected_type_frames_sensor_->publish_state(this->unexpected_type_frames_);
    }
    if (this->reference_layout_frames_sensor_ != nullptr) {
      this->reference_layout_frames_sensor_->publish_state(this->reference_layout_frames_);
    }
    if (this->route_variant_frames_sensor_ != nullptr) {
      this->route_variant_frames_sensor_->publish_state(this->route_variant_frames_);
    }
    if (this->unknown_route_frames_sensor_ != nullptr) {
      this->unknown_route_frames_sensor_->publish_state(this->unknown_route_frames_);
    }
    if (this->route_00_ff_frames_sensor_ != nullptr) {
      this->route_00_ff_frames_sensor_->publish_state(this->route_00_ff_frames_);
    }
    if (this->route_ff_00_frames_sensor_ != nullptr) {
      this->route_ff_00_frames_sensor_->publish_state(this->route_ff_00_frames_);
    }
    if (this->route_ff_40_frames_sensor_ != nullptr) {
      this->route_ff_40_frames_sensor_->publish_state(this->route_ff_40_frames_);
    }
  }

  protocol::FrameAssembler assembler_;
  std::map<uint16_t, std::vector<uint8_t>> previous_payloads_;

  uint32_t frame_timeout_ms_{75};
  uint32_t bus_idle_timeout_ms_{10000};
  uint32_t last_byte_at_{0};
  uint32_t last_valid_frame_at_{0};
  uint32_t last_health_log_at_{0};
  uint32_t health_log_interval_ms_{10000};
  uint32_t passive_scan_window_ms_{2000};
  uint32_t scan_profile_started_at_{0};
  uint32_t scan_profile_byte_start_{0};
  size_t scan_profile_index_{0};
  int rx_line_gpio_{-1};
  int direction_gpio_{-1};

  uint32_t bytes_received_{0};
  uint32_t valid_frames_{0};
  uint32_t checksum_failures_{0};
  uint32_t frame_timeouts_{0};
  uint32_t invalid_lengths_{0};
  uint32_t unexpected_type_frames_{0};
  uint32_t reference_layout_frames_{0};
  uint32_t route_variant_frames_{0};
  uint32_t unknown_route_frames_{0};
  uint32_t route_00_ff_frames_{0};
  uint32_t route_ff_00_frames_{0};
  uint32_t route_ff_40_frames_{0};

  bool log_frames_{true};
  bool bus_active_{false};
  bool passive_scan_{false};
  bool scan_locked_{false};

  sensor::Sensor *bytes_received_sensor_{nullptr};
  sensor::Sensor *valid_frames_sensor_{nullptr};
  sensor::Sensor *checksum_failures_sensor_{nullptr};
  sensor::Sensor *frame_timeouts_sensor_{nullptr};
  sensor::Sensor *invalid_lengths_sensor_{nullptr};
  sensor::Sensor *unexpected_type_frames_sensor_{nullptr};
  sensor::Sensor *reference_layout_frames_sensor_{nullptr};
  sensor::Sensor *route_variant_frames_sensor_{nullptr};
  sensor::Sensor *unknown_route_frames_sensor_{nullptr};
  sensor::Sensor *route_00_ff_frames_sensor_{nullptr};
  sensor::Sensor *route_ff_00_frames_sensor_{nullptr};
  sensor::Sensor *route_ff_40_frames_sensor_{nullptr};
  sensor::Sensor *last_source_sensor_{nullptr};
  sensor::Sensor *last_destination_sensor_{nullptr};
  sensor::Sensor *last_body_length_sensor_{nullptr};

  text_sensor::TextSensor *last_frame_sensor_{nullptr};
  text_sensor::TextSensor *last_payload_sensor_{nullptr};
  text_sensor::TextSensor *last_route_sensor_{nullptr};
  text_sensor::TextSensor *last_frame_class_sensor_{nullptr};
  text_sensor::TextSensor *last_changes_sensor_{nullptr};
  text_sensor::TextSensor *last_invalid_frame_sensor_{nullptr};
  text_sensor::TextSensor *protocol_sensor_{nullptr};
  text_sensor::TextSensor *serial_profile_sensor_{nullptr};

  binary_sensor::BinarySensor *bus_active_sensor_{nullptr};
  binary_sensor::BinarySensor *listen_only_sensor_{nullptr};
  binary_sensor::BinarySensor *rx_line_high_sensor_{nullptr};
  binary_sensor::BinarySensor *direction_high_sensor_{nullptr};
};

}  // namespace gree_wired_rs485
}  // namespace esphome
