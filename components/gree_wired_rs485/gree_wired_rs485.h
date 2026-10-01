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
#include "controller_registration.h"
#include "line_activity.h"
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
  void set_active_probe(bool active_probe) { this->active_probe_ = active_probe; }
  void set_active_probe_interval(uint32_t interval_ms) { this->active_probe_interval_ms_ = interval_ms; }
  void set_registration_attempts(uint8_t attempts) { this->registration_attempt_limit_ = attempts; }

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
  void set_rx_transitions_sensor(sensor::Sensor *s) { this->rx_transitions_sensor_ = s; }
  void set_rx_high_percent_sensor(sensor::Sensor *s) { this->rx_high_percent_sensor_ = s; }

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
  void set_last_raw_rx_sensor(text_sensor::TextSensor *s) { this->last_raw_rx_sensor_ = s; }

  void set_bus_active_sensor(binary_sensor::BinarySensor *s) { this->bus_active_sensor_ = s; }
  void set_listen_only_sensor(binary_sensor::BinarySensor *s) { this->listen_only_sensor_ = s; }
  void set_rx_line_high_sensor(binary_sensor::BinarySensor *s) { this->rx_line_high_sensor_ = s; }
  void set_direction_high_sensor(binary_sensor::BinarySensor *s) { this->direction_high_sensor_ = s; }
  void set_electrical_activity_sensor(binary_sensor::BinarySensor *s) { this->electrical_activity_sensor_ = s; }
  void set_direction_high_seen_sensor(binary_sensor::BinarySensor *s) { this->direction_high_seen_sensor_ = s; }

  // Run before the UART bus (setup_priority::BUS). The Seeed board ties
  // TP8485E DE and /RE to the same GPIO, so the safest software-only startup is
  // to force that pin LOW before ESPHome configures the RX-only UART.
  float get_setup_priority() const override { return setup_priority::POWER - 1.0f; }

  void setup() override {
    this->setup_started_at_ = millis();
    this->force_receive_mode_();

    // GPIO4 is never delegated to ESP-IDF RTS. In passive mode it stays LOW
    // continuously. In active-controller mode this component alone may raise
    // DE for a bounded FF->00 controller-registration frame.
    ESP_LOGI(TAG, "Starting Gree COM-MANUAL monitor in software-directed RS485 mode active_probe=%s",
             YESNO(this->active_probe_));
    ESP_LOGI(TAG, "Protocol profile: 1200 baud 8N1, 7E 7E framing, type 0x11, XOR checksum");
    if (this->passive_scan_) {
      ESP_LOGI(TAG, "Passive UART profile scan enabled; RS485 transmitter remains disabled");
      this->scan_profile_started_at_ = millis();
      this->scan_profile_byte_start_ = this->bytes_received_;
      this->publish_serial_profile_();
    } else {
      this->publish_serial_profile_();
    }

    if (this->listen_only_sensor_ != nullptr) this->listen_only_sensor_->publish_state(!this->active_probe_);
    if (this->bus_active_sensor_ != nullptr) this->bus_active_sensor_->publish_state(false);
    if (this->electrical_activity_sensor_ != nullptr) this->electrical_activity_sensor_->publish_state(false);
    if (this->direction_high_seen_sensor_ != nullptr) this->direction_high_seen_sensor_->publish_state(false);
    if (this->protocol_sensor_ != nullptr) {
      this->protocol_sensor_->publish_state("1200-8N1; 7E7E; src,dst,11,len,body; xor=0");
    }
    this->publish_counters_();
    this->observe_line_activity_();
    this->publish_line_states_();
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Gree wired-controller RS485 monitor:");
    ESP_LOGCONFIG(TAG, "  Mode: %s", this->active_probe_ ? "active controller registration" : "listen-only");
    if (this->active_probe_) {
      ESP_LOGCONFIG(TAG, "  Registration interval: %lu ms",
                    static_cast<unsigned long>(this->active_probe_interval_ms_));
      ESP_LOGCONFIG(TAG, "  Registration attempts: %u",
                    static_cast<unsigned>(this->registration_attempt_limit_));
    }
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
    this->observe_line_activity_();
    if (this->assembler_.active() && this->last_byte_at_ != 0 &&
        static_cast<uint32_t>(now - this->last_byte_at_) > this->frame_timeout_ms_) {
      this->assembler_.reset();
      ++this->frame_timeouts_;
      this->publish_counters_();
      ESP_LOGW(TAG, "Discarded partial COM-MANUAL frame after inter-byte timeout");
    }

    if (this->registration_waiting_for_response_ &&
        static_cast<uint32_t>(now - this->last_registration_at_) >=
            this->active_probe_interval_ms_) {
      this->finish_registration_response_window_();
    }

    if (this->active_probe_ && this->should_send_registration_(now)) {
      this->send_registration_();
    }

    while (this->available()) {
      uint8_t byte = 0;
      if (!this->read_byte(&byte)) break;
      this->last_byte_at_ = millis();
      ++this->bytes_received_;
      this->raw_rx_burst_.push_back(byte);
      this->raw_rx_burst_last_at_ = this->last_byte_at_;
      if (this->startup_rx_capture_.size() < 128) {
        this->startup_rx_capture_.push_back(byte);
      }
      if (this->registration_waiting_for_response_ &&
          this->registration_response_capture_.size() < 128) {
        this->registration_response_capture_.push_back(byte);
      }

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

    if (!this->raw_rx_burst_.empty() && this->raw_rx_burst_last_at_ != 0 &&
        static_cast<uint32_t>(millis() - this->raw_rx_burst_last_at_) >= 50) {
      this->last_raw_rx_hex_ = hex_(this->raw_rx_burst_);
      this->last_raw_rx_size_ = this->raw_rx_burst_.size();
      ESP_LOGI(TAG, "RX raw burst (%u bytes): %s",
               static_cast<unsigned>(this->last_raw_rx_size_),
               this->last_raw_rx_hex_.c_str());
      if (this->last_raw_rx_sensor_ != nullptr) {
        this->last_raw_rx_sensor_->publish_state(this->last_raw_rx_hex_);
      }
      this->raw_rx_burst_.clear();
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

      const auto rx_activity = this->rx_line_activity_.take_window();
      const uint32_t uart_bytes_window = this->bytes_received_ - this->last_health_byte_count_;
      this->last_health_byte_count_ = this->bytes_received_;
      const bool electrical_activity =
          rx_activity.transitions > 0 || uart_bytes_window > 0;

      if (this->rx_transitions_sensor_ != nullptr) {
        this->rx_transitions_sensor_->publish_state(this->rx_line_activity_.total_transitions());
      }
      if (this->rx_high_percent_sensor_ != nullptr && rx_activity.samples > 0) {
        this->rx_high_percent_sensor_->publish_state(rx_activity.high_percent());
      }
      if (this->electrical_activity_sensor_ != nullptr) {
        this->electrical_activity_sensor_->publish_state(electrical_activity);
      }

      const int rx_level = this->read_gpio_level_(this->rx_line_gpio_);
      const int direction_level = this->read_gpio_level_(this->direction_gpio_);
      ESP_LOGI(TAG,
               "HEALTH mode=%s reg=%u/%u waiting=%s established=%s profile=%s bytes=%lu uart_window=%lu "
               "valid=%lu xor_fail=%lu invalid_len=%lu timeouts=%lu rx_recent=%s "
               "valid_bus=%s rx_level=%d de_level=%d rx_edges_window=%lu "
               "rx_edges_total=%lu rx_high=%.1f%% rx_samples=%lu last_raw=%u:%s "
               "startup_rx=%u:%s",
               this->active_probe_ ? "ACTIVE" : "PASSIVE",
               static_cast<unsigned>(this->registration_attempts_sent_),
               static_cast<unsigned>(this->registration_attempt_limit_),
               YESNO(this->registration_waiting_for_response_),
               YESNO(this->registration_established_),
               this->scan_profile_(this->scan_profile_index_).name,
               static_cast<unsigned long>(this->bytes_received_),
               static_cast<unsigned long>(uart_bytes_window),
               static_cast<unsigned long>(this->valid_frames_),
               static_cast<unsigned long>(this->checksum_failures_),
               static_cast<unsigned long>(this->invalid_lengths_),
               static_cast<unsigned long>(this->frame_timeouts_),
               YESNO(rx_recent), YESNO(this->bus_active_), rx_level, direction_level,
               static_cast<unsigned long>(rx_activity.transitions),
               static_cast<unsigned long>(this->rx_line_activity_.total_transitions()),
               rx_activity.high_percent(),
               static_cast<unsigned long>(rx_activity.samples),
               static_cast<unsigned>(this->last_raw_rx_size_),
               this->last_raw_rx_hex_.empty() ? "-" : this->last_raw_rx_hex_.c_str(),
               static_cast<unsigned>(this->startup_rx_capture_.size()),
               this->startup_rx_capture_.empty() ? "-" : hex_(this->startup_rx_capture_).c_str());

      if (!rx_recent && direction_level == 0 && rx_level == 0 &&
          rx_activity.transitions == 0) {
        if (!this->warned_rx_held_low_) {
          ESP_LOGW(TAG,
                   "RS485 receiver output stayed LOW while DE was disabled and no UART "
                   "traffic was decoded; inspect bus bias, loading, wiring, and transceiver "
                   "state without treating this as an A/B polarity test");
          this->warned_rx_held_low_ = true;
        }
      } else if (rx_level == 1 || rx_activity.transitions > 0) {
        this->warned_rx_held_low_ = false;
      }

      if (rx_activity.transitions > 0 && uart_bytes_window == 0) {
        if (!this->warned_edges_without_uart_) {
          ESP_LOGW(TAG,
                   "RX GPIO changed level but the UART decoded no bytes in this health "
                   "window; physical activity exists but the active serial framing may not "
                   "match it");
          this->warned_edges_without_uart_ = true;
        }
      } else {
        this->warned_edges_without_uart_ = false;
      }
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

  void force_receive_mode_() {
#ifdef USE_ESP32
    if (this->direction_gpio_ < 0) return;

    const auto gpio = static_cast<gpio_num_t>(this->direction_gpio_);

    // Preload the output latch LOW before switching the pad to output. This
    // avoids a software-created HIGH pulse during direction changes.
    esp_err_t err = gpio_set_level(gpio, 0);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to preload RS485 DE LOW on GPIO%d: %s",
               this->direction_gpio_, esp_err_to_name(err));
      this->mark_failed();
      return;
    }

    err = gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to configure RS485 DE GPIO%d as output: %s",
               this->direction_gpio_, esp_err_to_name(err));
      this->mark_failed();
      return;
    }

    // The internal pull-down is not relied on for normal operation; it simply
    // reinforces the receive state while the application owns the pin.
    gpio_pulldown_en(gpio);
    gpio_pullup_dis(gpio);
    gpio_set_level(gpio, 0);

    ESP_LOGI(TAG, "RS485 direction guard active: GPIO%d forced LOW; UART is RX-only",
             this->direction_gpio_);
#endif
  }

  bool set_direction_level_(int level) {
#ifdef USE_ESP32
    if (this->direction_gpio_ < 0) return false;
    const auto gpio = static_cast<gpio_num_t>(this->direction_gpio_);
    const esp_err_t err = gpio_set_level(gpio, level ? 1 : 0);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to set RS485 DE GPIO%d=%d: %s",
               this->direction_gpio_, level ? 1 : 0, esp_err_to_name(err));
      return false;
    }
    return true;
#else
    return false;
#endif
  }

  static uint8_t xor_checksum_(const uint8_t *data, size_t len_without_checksum) {
    uint8_t value = 0;
    for (size_t i = 0; i < len_without_checksum; ++i) value ^= data[i];
    return value;
  }

  bool should_send_registration_(uint32_t now) const {
    if (!this->active_probe_ || this->registration_waiting_for_response_) return false;
    if (this->registration_established_) return false;
    if (this->registration_attempts_sent_ >= this->registration_attempt_limit_) return false;
    if (this->last_registration_at_ == 0) {
      return static_cast<uint32_t>(now - this->setup_started_at_) >=
             this->registration_start_delay_ms_;
    }
    // A completed response window already consumed active_probe_interval_ms_.
    // Once it is finalized, the next attempt may start immediately.
    return true;
  }

  void finish_registration_response_window_() {
    this->registration_waiting_for_response_ = false;
    const std::string response_hex = hex_(this->registration_response_capture_);
    ESP_LOGI(TAG, "REG response %u/%u bytes=%u: %s",
             static_cast<unsigned>(this->registration_attempts_sent_),
             static_cast<unsigned>(this->registration_attempt_limit_),
             static_cast<unsigned>(this->registration_response_capture_.size()),
             response_hex.empty() ? "-" : response_hex.c_str());

    if (this->valid_frames_ > this->registration_valid_frames_at_send_) {
      this->registration_established_ = true;
      ESP_LOGI(TAG, "REG accepted: valid wired frame observed during response window");
    }
    this->registration_response_capture_.clear();
  }

  void send_registration_() {
    // Recovered controller-side FF->00 registration/state frame from a real
    // Gree wired-controller capture. The line rate is only 1200 baud: this
    // 40-byte frame occupies roughly 333 ms on the wire. Do not schedule the
    // next registration from the pre-TX timestamp or the frames become
    // back-to-back and collide with the indoor unit's reply.
    static const uint8_t REGISTRATION_TEMPLATE[] = {
        0x7E, 0x7E, 0xFF, 0x00, 0x11, 0x22,
        0x09, 0x30, 0x83, 0x11, 0x1B, 0x00, 0x00, 0x10,
        0xE0, 0xE0, 0x08, 0x00, 0x28, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x30, 0x58};

    std::vector<uint8_t> frame(REGISTRATION_TEMPLATE,
                               REGISTRATION_TEMPLATE + sizeof(REGISTRATION_TEMPLATE));
    static const uint8_t COUNTERS[] = {
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x20};
    frame[26] = COUNTERS[this->registration_attempts_sent_ %
                         (sizeof(COUNTERS) / sizeof(COUNTERS[0]))];
    frame.back() = xor_checksum_(frame.data(), frame.size() - 1);

    ++this->registration_attempts_sent_;
    this->tx_in_progress_ = true;

    if (!this->set_direction_level_(1)) {
      ESP_LOGE(TAG, "Controller registration aborted: could not enable RS485 driver");
      this->tx_in_progress_ = false;
      this->force_receive_mode_();
      return;
    }

    ESP_LOGI(TAG, "TX controller registration %u/%u counter=0x%02X: %s",
             static_cast<unsigned>(this->registration_attempts_sent_),
             static_cast<unsigned>(this->registration_attempt_limit_),
             frame[26], hex_(frame).c_str());
    this->write_array(frame.data(), frame.size());
    const auto flush_result = this->flush();
    this->force_receive_mode_();
    this->tx_in_progress_ = false;

    // Start the receive window only after the UART has drained and DE is LOW.
    // At 1200 baud this is the critical distinction from the old burst logic.
    this->last_registration_at_ = millis();
    this->registration_valid_frames_at_send_ = this->valid_frames_;
    this->registration_response_capture_.clear();
    this->registration_waiting_for_response_ = true;

    if (flush_result != uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS) {
      ESP_LOGW(TAG, "Controller registration TX flush was not confirmed");
    }
  }

  int read_gpio_level_(int gpio) const {
#ifdef USE_ESP32
    if (gpio >= 0) return gpio_get_level(static_cast<gpio_num_t>(gpio));
#endif
    return -1;
  }

  void observe_line_activity_() {
    const int rx_level = this->read_gpio_level_(this->rx_line_gpio_);
    this->rx_line_activity_.observe(rx_level);

    const int direction_level = this->read_gpio_level_(this->direction_gpio_);
    if (direction_level > 0 && !this->tx_in_progress_ && !this->direction_high_seen_) {
      this->direction_high_seen_ = true;
      if (this->direction_high_seen_sensor_ != nullptr) {
        this->direction_high_seen_sensor_->publish_state(true);
      }
      ESP_LOGE(TAG,
               "RS485 DE was observed HIGH despite the software direction guard; forcing "
               "GPIO%d LOW again",
               this->direction_gpio_);
      this->force_receive_mode_();
    }
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
    // transmit anything on RS485. The deployment UART owns RX only; GPIO4 is
    // deliberately not registered as UART RTS/flow control.
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
  diagnostics::LineActivityTracker rx_line_activity_;
  std::map<uint16_t, std::vector<uint8_t>> previous_payloads_;
  std::vector<uint8_t> raw_rx_burst_;
  std::vector<uint8_t> startup_rx_capture_;
  std::vector<uint8_t> registration_response_capture_;
  std::string last_raw_rx_hex_;
  size_t last_raw_rx_size_{0};

  uint32_t frame_timeout_ms_{75};
  uint32_t setup_started_at_{0};
  uint32_t active_probe_interval_ms_{1200};
  uint32_t registration_start_delay_ms_{1500};
  uint32_t last_registration_at_{0};
  uint32_t raw_rx_burst_last_at_{0};
  uint32_t bus_idle_timeout_ms_{10000};
  uint32_t last_byte_at_{0};
  uint32_t last_valid_frame_at_{0};
  uint32_t last_health_log_at_{0};
  uint32_t last_health_byte_count_{0};
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
  bool active_probe_{false};
  uint8_t registration_attempt_limit_{4};
  uint8_t registration_attempts_sent_{0};
  bool registration_waiting_for_response_{false};
  bool registration_established_{false};
  uint32_t registration_valid_frames_at_send_{0};
  bool tx_in_progress_{false};
  bool bus_active_{false};
  bool passive_scan_{false};
  bool scan_locked_{false};
  bool warned_rx_held_low_{false};
  bool warned_edges_without_uart_{false};
  bool direction_high_seen_{false};

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
  sensor::Sensor *rx_transitions_sensor_{nullptr};
  sensor::Sensor *rx_high_percent_sensor_{nullptr};

  text_sensor::TextSensor *last_frame_sensor_{nullptr};
  text_sensor::TextSensor *last_payload_sensor_{nullptr};
  text_sensor::TextSensor *last_route_sensor_{nullptr};
  text_sensor::TextSensor *last_frame_class_sensor_{nullptr};
  text_sensor::TextSensor *last_changes_sensor_{nullptr};
  text_sensor::TextSensor *last_invalid_frame_sensor_{nullptr};
  text_sensor::TextSensor *protocol_sensor_{nullptr};
  text_sensor::TextSensor *serial_profile_sensor_{nullptr};
  text_sensor::TextSensor *last_raw_rx_sensor_{nullptr};

  binary_sensor::BinarySensor *bus_active_sensor_{nullptr};
  binary_sensor::BinarySensor *listen_only_sensor_{nullptr};
  binary_sensor::BinarySensor *rx_line_high_sensor_{nullptr};
  binary_sensor::BinarySensor *direction_high_sensor_{nullptr};
  binary_sensor::BinarySensor *electrical_activity_sensor_{nullptr};
  binary_sensor::BinarySensor *direction_high_seen_sensor_{nullptr};
};

}  // namespace gree_wired_rs485
}  // namespace esphome
