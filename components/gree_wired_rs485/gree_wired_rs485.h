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
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "controller_registration.h"
#include "line_activity.h"
#include "registration_rx_window.h"
#include "wired_controller_state.h"
#include "wired_protocol.h"
#include "wired_status.h"

#ifdef USE_ESP32
#include "driver/gpio.h"
#include "esp_timer.h"
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
  void set_rx_idle_pullup(bool enabled) { this->rx_idle_pullup_ = enabled; }
  void set_active_probe(bool active_probe) { this->active_probe_ = active_probe; }
  void set_legacy_gkh_xk76_probe(bool enabled) {
    this->legacy_gkh_xk76_probe_ = enabled;
  }
  void set_active_probe_interval(uint32_t interval_ms) { this->active_probe_interval_ms_ = interval_ms; }
  void set_registration_attempts(uint8_t attempts) { this->registration_attempt_limit_ = attempts; }
  void set_silent_bootstrap_probe(bool enabled) { this->silent_bootstrap_probe_ = enabled; }
  void set_silent_bootstrap_delay(uint32_t delay_ms) { this->silent_bootstrap_delay_ms_ = delay_ms; }
  void set_hardware_half_duplex(bool enabled) { this->hardware_half_duplex_ = enabled; }
  void set_persistent_controller(bool enabled) { this->persistent_controller_ = enabled; }

  bool set_controller_setpoint_celsius(float value) {
    if (!controller::set_setpoint_celsius(this->controller_state_, value)) return false;
    this->publish_controller_state_();
    ESP_LOGI(TAG,
             "CTRL staged setpoint=%.1fC; will be encoded on the next eligible indoor poll",
             value);
    return true;
  }
  void set_controller_mode_power_raw(uint8_t value) {
    controller::set_mode_power_raw(this->controller_state_, value);
    this->publish_controller_state_();
    ESP_LOGI(TAG,
             "CTRL staged mode_power=0x%02X; will be encoded on the next eligible indoor poll",
             value);
  }
  void set_controller_power(bool enabled) {
    controller::set_power_enabled(this->controller_state_, enabled);
    this->publish_controller_state_();
    ESP_LOGI(TAG,
             "CTRL staged power=%s mode_power=0x%02X; will be encoded on the next eligible indoor poll",
             enabled ? "ON" : "OFF",
             controller::mode_power_raw(this->controller_state_));
  }
  bool set_controller_payload_byte(uint8_t index, uint8_t value) {
    if (!controller::set_payload_byte(this->controller_state_, index, value)) return false;
    this->publish_controller_state_();
    ESP_LOGW(TAG,
             "CTRL staged experimental payload[%u]=0x%02X; no direct TX occurs",
             static_cast<unsigned>(index), value);
    return true;
  }

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
  void set_controller_polls_seen_sensor(sensor::Sensor *s) { this->controller_polls_seen_sensor_ = s; }
  void set_controller_responses_sent_sensor(sensor::Sensor *s) { this->controller_responses_sent_sensor_ = s; }
  void set_registered_status_frames_sensor(sensor::Sensor *s) { this->registered_status_frames_sensor_ = s; }
  void set_registered_setpoint_candidate_sensor(sensor::Sensor *s) {
    this->registered_setpoint_candidate_sensor_ = s;
  }

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
  void set_ff40_appendix_sensor(text_sensor::TextSensor *s) { this->ff40_appendix_sensor_ = s; }
  void set_controller_state_sensor(text_sensor::TextSensor *s) { this->controller_state_sensor_ = s; }
  void set_ff40_payload_sensor(text_sensor::TextSensor *s) { this->ff40_payload_sensor_ = s; }
  void set_ff40_changes_sensor(text_sensor::TextSensor *s) { this->ff40_changes_sensor_ = s; }
  void set_last_frame_role_sensor(text_sensor::TextSensor *s) { this->last_frame_role_sensor_ = s; }
  void set_poll_payload_sensor(text_sensor::TextSensor *s) { this->poll_payload_sensor_ = s; }
  void set_poll_changes_sensor(text_sensor::TextSensor *s) { this->poll_changes_sensor_ = s; }
  void set_ff40_indexed_sensor(text_sensor::TextSensor *s) { this->ff40_indexed_sensor_ = s; }

  void set_bus_active_sensor(binary_sensor::BinarySensor *s) { this->bus_active_sensor_ = s; }
  void set_listen_only_sensor(binary_sensor::BinarySensor *s) { this->listen_only_sensor_ = s; }
  void set_rx_line_high_sensor(binary_sensor::BinarySensor *s) { this->rx_line_high_sensor_ = s; }
  void set_direction_high_sensor(binary_sensor::BinarySensor *s) { this->direction_high_sensor_ = s; }
  void set_electrical_activity_sensor(binary_sensor::BinarySensor *s) { this->electrical_activity_sensor_ = s; }
  void set_direction_high_seen_sensor(binary_sensor::BinarySensor *s) { this->direction_high_seen_sensor_ = s; }
  void set_registered_status_sensor(binary_sensor::BinarySensor *s) { this->registered_status_sensor_ = s; }

  // Hardware half-duplex must start after the UART bus so ESP-IDF owns RTS/DE.
  // The manual fallback retains the earlier pre-UART LOW guard.
  float get_setup_priority() const override {
    return this->hardware_half_duplex_ ? setup_priority::DATA
                                      : setup_priority::POWER - 1.0f;
  }

  void setup() override {
    this->setup_started_at_ = millis();
    if (!this->hardware_half_duplex_) {
      this->force_receive_mode_();
    }

    ESP_LOGI(TAG,
             "Starting Gree COM-MANUAL monitor direction=%s active_probe=%s "
             "legacy_gkh_xk76=%s",
             this->hardware_half_duplex_ ? "UART_RS485_HALF_DUPLEX" : "MANUAL_GPIO",
             YESNO(this->active_probe_),
             YESNO(this->legacy_gkh_xk76_probe_enabled_()));
    ESP_LOGI(TAG,
             "Legacy reference decode profile: 1200 baud 8N1, 7E 7E framing, "
             "type 0x11, XOR checksum; XE71/Vireo application protocol is unproven");
    if (this->passive_scan_) {
      ESP_LOGI(TAG, "Passive UART profile scan enabled; RS485 transmitter remains disabled");
      this->scan_profile_started_at_ = millis();
      this->scan_profile_byte_start_ = this->bytes_received_;
      this->publish_serial_profile_();
    } else {
      this->publish_serial_profile_();
    }

    if (this->listen_only_sensor_ != nullptr) {
      this->listen_only_sensor_->publish_state(!this->legacy_gkh_xk76_probe_enabled_());
    }
    if (this->bus_active_sensor_ != nullptr) this->bus_active_sensor_->publish_state(false);
    if (this->electrical_activity_sensor_ != nullptr) this->electrical_activity_sensor_->publish_state(false);
    if (this->direction_high_seen_sensor_ != nullptr) this->direction_high_seen_sensor_->publish_state(false);
    if (this->protocol_sensor_ != nullptr) {
      this->protocol_sensor_->publish_state(
          "candidate legacy wired profile: 1200-8N1; 7E7E; type11; xor=0; "
          "XE71/Vireo unproven");
    }
    if (this->registered_status_sensor_ != nullptr) this->registered_status_sensor_->publish_state(false);
    this->publish_controller_state_();
    this->publish_counters_();
    this->observe_line_activity_();
    this->publish_line_states_();

    // GPIO7 is also routed to the UART RX matrix. Install a passive any-edge
    // observer after component setup settles so short 1200-baud transitions
    // are not lost to main-loop sampling. This never changes the pin level.
    this->set_timeout("rx-edge-monitor", 250, [this]() {
      this->setup_rx_edge_monitor_();
    });
  }

  void on_shutdown() override {
#ifdef USE_ESP32
    if (this->rx_edge_monitor_installed_ && this->rx_line_gpio_ >= 0) {
      gpio_isr_handler_remove(static_cast<gpio_num_t>(this->rx_line_gpio_));
      this->rx_edge_monitor_installed_ = false;
    }
#endif
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Gree wired-controller RS485 monitor:");
    ESP_LOGCONFIG(TAG, "  Mode: %s",
                  this->legacy_gkh_xk76_probe_enabled_()
                      ? "legacy GKH/XK76 active registration experiment"
                      : "listen-only");
    ESP_LOGCONFIG(TAG, "  Legacy GKH/XK76 TX profile acknowledged: %s",
                  YESNO(this->legacy_gkh_xk76_probe_));
    ESP_LOGCONFIG(TAG, "  Direction control: %s",
                  this->hardware_half_duplex_ ? "ESP-IDF UART_MODE_RS485_HALF_DUPLEX"
                                              : "manual GPIO");
    if (this->legacy_gkh_xk76_probe_enabled_()) {
      ESP_LOGCONFIG(TAG, "  Legacy registration interval: %lu ms",
                    static_cast<unsigned long>(this->active_probe_interval_ms_));
      ESP_LOGCONFIG(TAG, "  Registration attempts: %u",
                    static_cast<unsigned>(this->registration_attempt_limit_));
      ESP_LOGCONFIG(TAG, "  Silent-bus bootstrap probe: %s after %lu ms",
                    YESNO(this->silent_bootstrap_probe_),
                    static_cast<unsigned long>(this->silent_bootstrap_delay_ms_));
      ESP_LOGCONFIG(TAG, "  Persistent controller runtime: %s",
                    YESNO(this->persistent_controller_));
    }
    ESP_LOGCONFIG(TAG, "  Frame gap timeout: %lu ms",
                  static_cast<unsigned long>(this->frame_timeout_ms_));
    ESP_LOGCONFIG(TAG, "  Bus idle timeout: %lu ms",
                  static_cast<unsigned long>(this->bus_idle_timeout_ms_));
    ESP_LOGCONFIG(TAG, "  Log valid frames: %s", YESNO(this->log_frames_));
    ESP_LOGCONFIG(TAG, "  Passive serial scan: %s", YESNO(this->passive_scan_));
    if (this->passive_scan_) {
      ESP_LOGCONFIG(TAG,
                    "  Passive scan profiles are decoder hypotheses only; "
                    "they never enable RS485 TX");
      ESP_LOGCONFIG(TAG, "  Passive scan window: %lu ms",
                    static_cast<unsigned long>(this->passive_scan_window_ms_));
    }
    ESP_LOGCONFIG(TAG, "  RX line GPIO: %d", this->rx_line_gpio_);
    ESP_LOGCONFIG(TAG, "  RX idle pull-up: %s", YESNO(this->rx_idle_pullup_));
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

    // Drain RX before making any decision to close a response window or
    // transmit the next registration. The old order could start the next TX
    // while reply bytes were already sitting in the UART FIFO, which is exactly
    // the wrong thing to do on a half-duplex bus.
    while (this->available()) {
      uint8_t byte = 0;
      if (!this->read_byte(&byte)) break;
      this->last_byte_at_ = millis();
      const uint32_t byte_observed_at_us = micros();
      if (this->registration_waiting_for_response_) {
        this->registration_rx_window_.observe_byte(byte_observed_at_us);
      }
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

    const uint32_t after_rx = millis();
    const bool response_window_elapsed =
        this->registration_waiting_for_response_ &&
        static_cast<uint32_t>(after_rx - this->last_registration_at_) >=
            this->active_probe_interval_ms_;
    const bool response_line_quiet =
        this->last_byte_at_ <= this->last_registration_at_ ||
        static_cast<uint32_t>(after_rx - this->last_byte_at_) >=
            this->registration_response_quiet_ms_;

    if (response_window_elapsed && response_line_quiet) {
      this->finish_registration_response_window_();
    }

    const uint32_t setup_elapsed =
        static_cast<uint32_t>(after_rx - this->setup_started_at_);
    if (registration::silent_bootstrap_ready(
            this->silent_bootstrap_probe_,
            this->legacy_gkh_xk76_probe_enabled_(),
            this->registration_armed_, this->registration_established_,
            this->registration_attempts_sent_, setup_elapsed,
            this->silent_bootstrap_delay_ms_, this->bytes_received_,
            this->rx_transition_total_())) {
      this->registration_armed_ = true;
      this->silent_bootstrap_armed_ = true;
      this->registration_unit_signature_ = registration::REFERENCE_UNIT_SIGNATURE;
      controller::set_unit_signature(
          this->controller_state_, this->registration_unit_signature_);
      this->publish_controller_state_();
      ESP_LOGW(TAG,
               "REG silent-bus bootstrap armed after %lums: no UART bytes and no RX "
               "transitions observed; using captured reference unit signature %02X%02X%02X",
               static_cast<unsigned long>(setup_elapsed),
               this->registration_unit_signature_[0],
               this->registration_unit_signature_[1],
               this->registration_unit_signature_[2]);
    }

    if (this->legacy_gkh_xk76_probe_enabled_() &&
        this->should_send_registration_(after_rx) && this->available() == 0) {
      this->send_registration_();
    }

    if (this->legacy_gkh_xk76_probe_enabled_() &&
        this->persistent_controller_ && this->registration_established_ &&
        this->runtime_response_pending_ && !this->registration_waiting_for_response_ &&
        this->available() == 0) {
      this->send_runtime_controller_response_();
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
      const uint32_t rx_isr_edges_total = this->rx_transition_total_();
      const uint32_t rx_isr_edges_window_raw =
          rx_isr_edges_total - this->last_health_isr_edge_count_;
      this->last_health_isr_edge_count_ = rx_isr_edges_total;
      const uint32_t tx_rx_edges_window =
          this->tx_rx_edges_total_ - this->last_health_tx_rx_edge_count_;
      this->last_health_tx_rx_edge_count_ = this->tx_rx_edges_total_;
      const uint32_t turnaround_edges_window =
          this->turnaround_rx_edges_total_ -
          this->last_health_turnaround_rx_edge_count_;
      this->last_health_turnaround_rx_edge_count_ =
          this->turnaround_rx_edges_total_;
      const uint32_t non_external_edges_window =
          tx_rx_edges_window + turnaround_edges_window;
      const uint32_t rx_isr_edges_window =
          rx_isr_edges_window_raw >= non_external_edges_window
              ? static_cast<uint32_t>(
                    rx_isr_edges_window_raw - non_external_edges_window)
              : 0;
      const uint32_t tx_residue_window =
          this->tx_residue_bytes_total_ - this->last_health_tx_residue_byte_count_;
      this->last_health_tx_residue_byte_count_ = this->tx_residue_bytes_total_;
      const uint32_t uart_bytes_window = this->bytes_received_ - this->last_health_byte_count_;
      this->last_health_byte_count_ = this->bytes_received_;
      const bool electrical_activity =
          rx_isr_edges_window > 0 || rx_activity.transitions > 0 ||
          uart_bytes_window > 0;

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
               "HEALTH mode=%s dir=%s reg=%u/%u armed=%s waiting=%s established=%s sig=%s unit=%02X%02X%02X "
               "runtime=%s polls=%lu replies=%lu status29=%lu pending=%s "
               "tx_ms=%lu tx_flush=%s tx_de=%d>%d "
               "profile=%s bytes=%lu uart_window=%lu "
               "valid=%lu xor_fail=%lu invalid_len=%lu timeouts=%lu rx_recent=%s "
               "valid_bus=%s rx_level=%d de_level=%d rx_edges_window=%lu "
               "tx_rx_edges_window=%lu turnaround_edges_window=%lu "
               "rx_edges_total=%lu tx_residue_window=%lu "
               "tx_residue_total=%lu rx_sampled_edges_window=%lu "
               "edge_cadence_samples=%lu edge_min_gap_us=%lu "
               "edge_max_gap_us=%lu edge_last_gap_us=%lu "
               "rx_high=%.1f%% rx_samples=%lu last_raw=%u:%s "
               "startup_rx=%u:%s",
               this->legacy_gkh_xk76_probe_enabled_()
                   ? "LEGACY_GKH_XK76"
                   : "PASSIVE",
               this->hardware_half_duplex_ ? "UART_RS485" : "MANUAL",
               static_cast<unsigned>(this->registration_attempts_sent_),
               static_cast<unsigned>(this->registration_attempt_limit_),
               YESNO(this->registration_armed_),
               YESNO(this->registration_waiting_for_response_),
               YESNO(this->registration_established_),
               this->registration_unit_signature_learned_ ? "LEARNED" : "WAITING",
               this->registration_unit_signature_[0],
               this->registration_unit_signature_[1],
               this->registration_unit_signature_[2],
               this->persistent_controller_ ? "ON" : "OFF",
               static_cast<unsigned long>(this->controller_polls_seen_),
               static_cast<unsigned long>(this->controller_responses_sent_),
               static_cast<unsigned long>(this->registered_status_frames_),
               YESNO(this->runtime_response_pending_),
               static_cast<unsigned long>(this->last_registration_tx_elapsed_ms_),
               this->last_registration_tx_seen_
                   ? (this->last_registration_tx_flush_ok_ ? "OK" : "FAIL")
                   : "N/A",
               this->last_registration_de_before_,
               this->last_registration_de_after_,
               this->scan_profile_(this->scan_profile_index_).name,
               static_cast<unsigned long>(this->bytes_received_),
               static_cast<unsigned long>(uart_bytes_window),
               static_cast<unsigned long>(this->valid_frames_),
               static_cast<unsigned long>(this->checksum_failures_),
               static_cast<unsigned long>(this->invalid_lengths_),
               static_cast<unsigned long>(this->frame_timeouts_),
               YESNO(rx_recent), YESNO(this->bus_active_), rx_level, direction_level,
               static_cast<unsigned long>(rx_isr_edges_window),
               static_cast<unsigned long>(tx_rx_edges_window),
               static_cast<unsigned long>(turnaround_edges_window),
               static_cast<unsigned long>(rx_isr_edges_total),
               static_cast<unsigned long>(tx_residue_window),
               static_cast<unsigned long>(this->tx_residue_bytes_total_),
               static_cast<unsigned long>(rx_activity.transitions),
               static_cast<unsigned long>(this->rx_isr_cadence_samples_total_),
               static_cast<unsigned long>(this->rx_isr_cadence_min_gap_us_),
               static_cast<unsigned long>(this->rx_isr_cadence_max_gap_us_),
               static_cast<unsigned long>(this->rx_isr_cadence_last_gap_us_),
               rx_activity.high_percent(),
               static_cast<unsigned long>(rx_activity.samples),
               static_cast<unsigned>(this->last_raw_rx_size_),
               this->last_raw_rx_hex_.empty() ? "-" : this->last_raw_rx_hex_.c_str(),
               static_cast<unsigned>(this->startup_rx_capture_.size()),
               this->startup_rx_capture_.empty() ? "-" : hex_(this->startup_rx_capture_).c_str());

      const bool startup_trace_window_complete =
          this->first_valid_frame_at_ != 0 &&
          static_cast<uint32_t>(now - this->first_valid_frame_at_) >=
              this->startup_trace_replay_delay_ms_;
      if (!this->startup_trace_replayed_ && !this->startup_frame_trace_.empty() &&
          startup_trace_window_complete) {
        ESP_LOGI(TAG, "STARTUP retained valid frame trace count=%u",
                 static_cast<unsigned>(this->startup_frame_trace_.size()));
        for (size_t i = 0; i < this->startup_frame_trace_.size(); ++i) {
          ESP_LOGI(TAG, "STARTUP retained[%u] %s",
                   static_cast<unsigned>(i + 1),
                   this->startup_frame_trace_[i].c_str());
        }
        this->startup_trace_replayed_ = true;
      }

      const bool registration_trace_complete =
          !this->registration_waiting_for_response_ &&
          !this->registration_attempt_trace_.empty() &&
          (this->registration_established_ ||
           this->registration_attempts_sent_ >= this->registration_attempt_limit_ ||
           !this->registration_armed_);
      const bool registration_trace_replay_ready =
          static_cast<uint32_t>(now - this->setup_started_at_) >=
              this->registration_trace_replay_delay_ms_;
      if (!this->registration_trace_replayed_ && registration_trace_complete &&
          registration_trace_replay_ready) {
        ESP_LOGI(TAG, "REG retained window trace count=%u",
                 static_cast<unsigned>(this->registration_attempt_trace_.size()));
        for (size_t i = 0; i < this->registration_attempt_trace_.size(); ++i) {
          ESP_LOGI(TAG, "REG retained[%u] %s",
                   static_cast<unsigned>(i + 1),
                   this->registration_attempt_trace_[i].c_str());
        }
        this->registration_trace_replayed_ = true;
      }

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

      if ((rx_isr_edges_window > 0 || rx_activity.transitions > 0) &&
          uart_bytes_window == 0) {
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

  bool legacy_gkh_xk76_probe_enabled_() const {
    return this->active_probe_ && this->legacy_gkh_xk76_probe_;
  }

  bool should_send_registration_(uint32_t now) const {
    (void) now;
    if (!this->legacy_gkh_xk76_probe_enabled_() || !this->registration_armed_) {
      return false;
    }
    if (!this->registration_unit_signature_learned_ && !this->silent_bootstrap_armed_) return false;
    if (this->registration_waiting_for_response_) return false;
    if (this->registration_established_) return false;
    if (this->registration_attempts_sent_ >= this->registration_attempt_limit_) return false;
    // A startup 00->FF poll, not ESP uptime, opens the registration window.
    // After each completed response window the next bounded attempt may start.
    return true;
  }

  void learn_registration_signature_(const protocol::ParsedFrame &frame) {
    if (frame.route != protocol::RouteKind::ROUTE_FF_40 || frame.payload.size() < 3) return;

    const registration::UnitSignature learned = {
        frame.payload[0], frame.payload[1], frame.payload[2]};
    if (!this->registration_unit_signature_learned_ ||
        learned != this->registration_unit_signature_) {
      this->registration_unit_signature_ = learned;
      this->registration_unit_signature_learned_ = true;
      this->silent_bootstrap_armed_ = false;
      controller::set_unit_signature(this->controller_state_, learned);
      this->publish_controller_state_();
      ESP_LOGI(TAG, "REG learned target unit signature=%02X %02X %02X from FF->40",
               learned[0], learned[1], learned[2]);
    }
  }

  void observe_startup_poll_(uint32_t now) {
    ++this->controller_polls_seen_;

    // Once the indoor unit has accepted controller registration, later 00->FF
    // packets are runtime polls, not a reason to restart the bootstrap state
    // machine. Persistent runtime responses are deliberately opt-in until the
    // target Vireo has field-proven the captured controller frame family.
    if (this->legacy_gkh_xk76_probe_enabled_() &&
        this->registration_established_) {
      if (this->persistent_controller_) {
        this->runtime_response_pending_ = true;
        ESP_LOGD(TAG, "CTRL runtime poll queued response poll=%lu",
                 static_cast<unsigned long>(this->controller_polls_seen_));
      }
      this->last_startup_poll_at_ = now;
      return;
    }

    const bool new_startup_sequence =
        this->last_startup_poll_at_ == 0 ||
        static_cast<uint32_t>(now - this->last_startup_poll_at_) >
            this->startup_poll_rearm_gap_ms_;
    this->last_startup_poll_at_ = now;

    if (!this->legacy_gkh_xk76_probe_enabled_() || !new_startup_sequence) return;

    this->registration_armed_ = true;
    this->registration_established_ = false;
    this->registration_waiting_for_response_ = false;
    this->registration_attempts_sent_ = 0;
    this->last_registration_at_ = 0;
    this->registration_response_capture_.clear();
    this->registration_attempt_trace_.clear();
    this->registration_trace_replayed_ = false;

    ESP_LOGI(TAG,
             "REG armed by target 00->FF startup poll signature=%s unit=%02X %02X %02X",
             this->registration_unit_signature_learned_ ? "LEARNED" : "WAITING",
             this->registration_unit_signature_[0],
             this->registration_unit_signature_[1],
             this->registration_unit_signature_[2]);
  }

  void finish_registration_response_window_() {
    this->registration_waiting_for_response_ = false;
    this->rx_edge_response_phase_ = false;
    this->rx_edge_window_active_ = false;
    this->registration_rx_window_.close();

    const uint32_t armed_edge_count = this->rx_edge_window_count_;
    const uint32_t edge_count = this->rx_edge_response_count_;
    const uint32_t edge_first_us = this->rx_edge_response_first_us_;
    const uint32_t edge_last_us = this->rx_edge_response_last_us_;
    const uint32_t edge_first_offset_us =
        edge_count == 0 ? 0 : static_cast<uint32_t>(
            edge_first_us - this->registration_rx_window_.de_released_at_us());
    const uint32_t edge_last_offset_us =
        edge_count == 0 ? 0 : static_cast<uint32_t>(
            edge_last_us - this->registration_rx_window_.de_released_at_us());
    const uint32_t edge_span_us =
        edge_count < 2 ? 0 : static_cast<uint32_t>(edge_last_us - edge_first_us);
    const uint32_t edge_min_gap_us =
        edge_count < 2 ? 0 : this->rx_edge_response_min_gap_us_;
    const uint32_t edge_max_gap_us =
        edge_count < 2 ? 0 : this->rx_edge_response_max_gap_us_;
    const uint32_t classified_edge_count =
        this->last_registration_turnaround_rx_edges_ + edge_count;
    const uint32_t phase_gap_edges =
        armed_edge_count > classified_edge_count
            ? static_cast<uint32_t>(armed_edge_count - classified_edge_count)
            : 0;
    const std::string response_hex = hex_(this->registration_response_capture_);
    const std::string tx_residue_hex =
        hex_(this->registration_tx_residue_capture_);
    char registration_meta[512];
    std::snprintf(
        registration_meta, sizeof(registration_meta),
        "attempt=%u/%u unvalidated_rx=%u pending_after_release=%u "
        "arm_to_de_us=%lu de_to_probe_us=%lu first_drain_us=%lu last_drain_us=%lu "
        "drain_span_us=%lu rx_edges=%lu first_edge_us=%lu last_edge_us=%lu "
        "edge_span_us=%lu edge_min_gap_us=%lu edge_max_gap_us=%lu "
        "turnaround_edges=%lu phase_gap_edges=%lu "
        "tx_rx_edges=%lu tx_residue=%u:",
        static_cast<unsigned>(this->registration_attempts_sent_),
        static_cast<unsigned>(this->registration_attempt_limit_),
        static_cast<unsigned>(this->registration_response_capture_.size()),
        static_cast<unsigned>(this->registration_rx_window_.pending_at_probe()),
        static_cast<unsigned long>(this->registration_rx_window_.de_release_delay_us()),
        static_cast<unsigned long>(this->registration_rx_window_.release_to_probe_delay_us()),
        static_cast<unsigned long>(this->registration_rx_window_.first_drain_us()),
        static_cast<unsigned long>(this->registration_rx_window_.last_drain_us()),
        static_cast<unsigned long>(this->registration_rx_window_.drain_span_us()),
        static_cast<unsigned long>(edge_count),
        static_cast<unsigned long>(edge_first_offset_us),
        static_cast<unsigned long>(edge_last_offset_us),
        static_cast<unsigned long>(edge_span_us),
        static_cast<unsigned long>(edge_min_gap_us),
        static_cast<unsigned long>(edge_max_gap_us),
        static_cast<unsigned long>(this->last_registration_turnaround_rx_edges_),
        static_cast<unsigned long>(phase_gap_edges),
        static_cast<unsigned long>(this->last_registration_tx_rx_edges_),
        static_cast<unsigned>(this->registration_tx_residue_capture_.size()));
    std::string registration_summary(registration_meta);
    registration_summary += tx_residue_hex.empty() ? "-" : tx_residue_hex;
    registration_summary += " valid_delta=";
    registration_summary += std::to_string(
        this->registration_rx_window_.valid_frame_delta(this->valid_frames_));
    registration_summary += " bytes=";
    registration_summary += response_hex.empty() ? "-" : response_hex;
    if (this->registration_attempt_trace_.size() < 10) {
      this->registration_attempt_trace_.push_back(registration_summary);
    }

    ESP_LOGI(TAG,
             "REG window %u/%u unvalidated_rx=%u pending_after_release=%u "
             "arm_to_de_us=%lu de_to_probe_us=%lu first_drain_us=%lu last_drain_us=%lu "
             "drain_span_us=%lu rx_edges=%lu first_edge_us=%lu last_edge_us=%lu "
             "edge_span_us=%lu edge_min_gap_us=%lu edge_max_gap_us=%lu "
             "turnaround_edges=%lu phase_gap_edges=%lu "
             "tx_rx_edges=%lu tx_residue=%u:%s valid_delta=%lu bytes=%s",
             static_cast<unsigned>(this->registration_attempts_sent_),
             static_cast<unsigned>(this->registration_attempt_limit_),
             static_cast<unsigned>(this->registration_response_capture_.size()),
             static_cast<unsigned>(this->registration_rx_window_.pending_at_probe()),
             static_cast<unsigned long>(this->registration_rx_window_.de_release_delay_us()),
             static_cast<unsigned long>(this->registration_rx_window_.release_to_probe_delay_us()),
             static_cast<unsigned long>(this->registration_rx_window_.first_drain_us()),
             static_cast<unsigned long>(this->registration_rx_window_.last_drain_us()),
             static_cast<unsigned long>(this->registration_rx_window_.drain_span_us()),
             static_cast<unsigned long>(edge_count),
             static_cast<unsigned long>(edge_first_offset_us),
             static_cast<unsigned long>(edge_last_offset_us),
             static_cast<unsigned long>(edge_span_us),
             static_cast<unsigned long>(edge_min_gap_us),
             static_cast<unsigned long>(edge_max_gap_us),
             static_cast<unsigned long>(this->last_registration_turnaround_rx_edges_),
             static_cast<unsigned long>(phase_gap_edges),
             static_cast<unsigned long>(this->last_registration_tx_rx_edges_),
             static_cast<unsigned>(this->registration_tx_residue_capture_.size()),
             tx_residue_hex.empty() ? "-" : tx_residue_hex.c_str(),
             static_cast<unsigned long>(
                 this->registration_rx_window_.valid_frame_delta(this->valid_frames_)),
             response_hex.empty() ? "-" : response_hex.c_str());

    if (this->registration_accept_evidence_) {
      this->registration_established_ = true;
      this->registration_armed_ = false;
      this->runtime_response_pending_ = false;
      ESP_LOGI(TAG,
               "REG accepted: FF->40 expanded beyond pre-registration layout");
    } else if (this->registration_attempts_sent_ >= this->registration_attempt_limit_) {
      this->registration_armed_ = false;
      ESP_LOGI(TAG, "REG window exhausted after %u target-gated attempts",
               static_cast<unsigned>(this->registration_attempts_sent_));
    }
    this->registration_response_capture_.clear();
  }

  void send_registration_() {
    // This is specifically the legacy GKH/XK76 packet family recovered from
    // public wired-controller captures. It is not an XE71/Vireo registration
    // claim. Transmission requires the explicit legacy_gkh_xk76_probe gate.
    // Registration is target-gated by observed startup traffic and a
    // learned FF->40 signature. A separately configured silent-bus bootstrap
    // may use the captured reference signature only after a quiet electrical
    // interval proves that no UART bytes or RX transitions are present.
    const uint8_t accept_counter =
        registration::counter_for_attempt(this->registration_attempts_sent_);
    controller::set_accept_counter(this->controller_state_, accept_counter);
    this->publish_controller_state_();
    std::vector<uint8_t> frame =
        controller::encode(this->controller_state_, accept_counter);

    ++this->registration_attempts_sent_;
    this->last_registration_de_before_ = this->read_gpio_level_(this->direction_gpio_);
    this->registration_tx_residue_capture_.clear();
    const uint32_t tx_rx_edges_before = this->rx_transition_total_();
    this->tx_in_progress_ = true;

    if (!this->hardware_half_duplex_ && !this->set_direction_level_(1)) {
      ESP_LOGE(TAG, "Controller registration aborted: could not enable RS485 driver");
      this->tx_in_progress_ = false;
      this->force_receive_mode_();
      return;
    }

    ESP_LOGI(TAG,
             "TX legacy GKH/XK76 registration %u/%u counter=0x%02X direction=%s: %s",
             static_cast<unsigned>(this->registration_attempts_sent_),
             static_cast<unsigned>(this->registration_attempt_limit_),
             frame[26],
             this->hardware_half_duplex_ ? "UART_RS485" : "MANUAL",
             hex_(frame).c_str());

    // Time write+flush, not just queueing. ESPHome's ESP-IDF UART backend uses
    // uart_wait_tx_done() for flush(), so this persists evidence that the UART
    // remained busy for the complete 40-byte / 1200-baud transmission.
    const uint32_t tx_started_at = millis();
    this->write_array(frame.data(), frame.size());
    const auto flush_result = this->flush();
    this->last_registration_tx_elapsed_ms_ =
        static_cast<uint32_t>(millis() - tx_started_at);
    this->last_registration_tx_seen_ = true;
    this->last_registration_tx_flush_ok_ =
        flush_result == uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS;

    // In manual-DE mode the driver is still asserted here. Any complete UART
    // bytes already pending therefore arrived while this controller owned the
    // wire; quarantine them before releasing the bus so they cannot masquerade
    // as a Vireo response or contaminate the frame assembler.
    if (!this->hardware_half_duplex_) {
      const size_t tx_residue_pending = this->available();
      for (size_t i = 0; i < tx_residue_pending; ++i) {
        uint8_t byte = 0;
        if (!this->read_byte(&byte)) break;
        if (this->registration_tx_residue_capture_.size() < 128) {
          this->registration_tx_residue_capture_.push_back(byte);
        }
      }
      const uint32_t tx_rx_edges_after = this->rx_transition_total_();
      this->last_registration_tx_rx_edges_ =
          static_cast<uint32_t>(tx_rx_edges_after - tx_rx_edges_before);
      this->tx_rx_edges_total_ += this->last_registration_tx_rx_edges_;
      this->tx_residue_bytes_total_ +=
          static_cast<uint32_t>(this->registration_tx_residue_capture_.size());
    } else {
      // ESP-IDF controls DE in hardware mode, so flush() may already have
      // released the wire. Do not discard pending bytes there: they could be
      // the beginning of a legitimate response.
      this->last_registration_tx_rx_edges_ = 0;
    }

    // Arm edge capture BEFORE manual DE is released. The previous ordering
    // lowered DE, logged twice, and only then opened this window, leaving a
    // multi-millisecond blind spot at 1200 baud.
    this->rx_edge_window_active_ = false;
    this->rx_edge_window_count_ = 0;
    this->rx_edge_window_first_us_ = 0;
    this->rx_edge_window_last_us_ = 0;
    this->rx_edge_window_previous_us_ = 0;
    this->rx_edge_window_min_gap_us_ = 0;
    this->rx_edge_window_max_gap_us_ = 0;
    this->rx_edge_response_phase_ = false;
    this->rx_edge_response_count_ = 0;
    this->rx_edge_response_first_us_ = 0;
    this->rx_edge_response_last_us_ = 0;
    this->rx_edge_response_previous_us_ = 0;
    this->rx_edge_response_min_gap_us_ = 0;
    this->rx_edge_response_max_gap_us_ = 0;
    const uint32_t rx_window_armed_at_us = micros();
    this->rx_edge_window_active_ = true;

    if (!this->hardware_half_duplex_ && !this->set_direction_level_(0)) {
      this->rx_edge_window_active_ = false;
      this->tx_in_progress_ = false;
      ESP_LOGE(TAG, "Controller registration aborted: could not release RS485 driver");
      this->force_receive_mode_();
      return;
    }

    // From here onward the target owns the opportunity to respond. Capture the
    // release timestamp and FIFO depth before any logging can consume several
    // milliseconds and hide the first 1200-baud character.
    const uint32_t de_released_at_us = micros();
    this->last_registration_turnaround_rx_edges_ =
        this->rx_edge_window_count_;
    this->turnaround_rx_edges_total_ +=
        this->last_registration_turnaround_rx_edges_;
    this->rx_edge_response_phase_ = true;
    this->tx_in_progress_ = false;
    this->last_registration_de_after_ = this->read_gpio_level_(this->direction_gpio_);
    const size_t pending_after_release = this->available();
    const uint32_t pending_probe_at_us = micros();
    this->last_registration_at_ = millis();
    this->registration_accept_evidence_ = false;
    this->registration_response_capture_.clear();
    this->registration_rx_window_.open(
        rx_window_armed_at_us, de_released_at_us, pending_probe_at_us,
        pending_after_release, this->valid_frames_);
    this->registration_waiting_for_response_ = true;

    const std::string tx_residue_hex =
        hex_(this->registration_tx_residue_capture_);
    ESP_LOGI(TAG,
             "TX complete registration %u/%u elapsed=%lums expected_wire=~333ms "
             "flush=%s de_idle=%d>%d tx_rx_edges=%lu tx_residue=%u:%s",
             static_cast<unsigned>(this->registration_attempts_sent_),
             static_cast<unsigned>(this->registration_attempt_limit_),
             static_cast<unsigned long>(this->last_registration_tx_elapsed_ms_),
             this->last_registration_tx_flush_ok_ ? "OK" : "FAIL",
             this->last_registration_de_before_,
             this->last_registration_de_after_,
             static_cast<unsigned long>(this->last_registration_tx_rx_edges_),
             static_cast<unsigned>(this->registration_tx_residue_capture_.size()),
             tx_residue_hex.empty() ? "-" : tx_residue_hex.c_str());

    ESP_LOGI(TAG,
             "REG RX window opened attempt=%u pending_after_release=%u "
             "arm_to_de_us=%lu de_to_probe_us=%lu de=%d",
             static_cast<unsigned>(this->registration_attempts_sent_),
             static_cast<unsigned>(pending_after_release),
             static_cast<unsigned long>(
                 this->registration_rx_window_.de_release_delay_us()),
             static_cast<unsigned long>(
                 this->registration_rx_window_.release_to_probe_delay_us()),
             this->last_registration_de_after_);

    if (flush_result != uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS) {
      ESP_LOGW(TAG, "Controller registration TX flush was not confirmed");
    }
  }

  void send_runtime_controller_response_() {
    // COM-MANUAL is master/slave inverted relative to the Livo UART module:
    // the indoor unit emits 00->FF polls and the wired controller answers with
    // FF->00 state. Keep this disabled by default until Vireo registration is
    // field-qualified; when enabled, answer only after registration succeeds.
    const size_t sequence =
        static_cast<size_t>(this->registration_attempts_sent_) +
        static_cast<size_t>(this->controller_responses_sent_);
    const uint8_t accept_counter = registration::counter_for_attempt(sequence);
    controller::set_accept_counter(this->controller_state_, accept_counter);
    this->publish_controller_state_();
    std::vector<uint8_t> frame =
        controller::encode(this->controller_state_, accept_counter);

    this->runtime_response_pending_ = false;
    this->tx_in_progress_ = true;
    if (!this->hardware_half_duplex_ && !this->set_direction_level_(1)) {
      ESP_LOGE(TAG, "Controller runtime response aborted: could not enable RS485 driver");
      this->tx_in_progress_ = false;
      this->force_receive_mode_();
      return;
    }

    ESP_LOGI(TAG,
             "TX controller runtime response poll=%lu reply=%lu counter=0x%02X direction=%s: %s",
             static_cast<unsigned long>(this->controller_polls_seen_),
             static_cast<unsigned long>(this->controller_responses_sent_ + 1),
             accept_counter,
             this->hardware_half_duplex_ ? "UART_RS485" : "MANUAL",
             hex_(frame).c_str());

    this->write_array(frame.data(), frame.size());
    const auto flush_result = this->flush();

    if (!this->hardware_half_duplex_) {
      this->force_receive_mode_();
    }
    this->tx_in_progress_ = false;

    ++this->controller_responses_sent_;
    if (flush_result != uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS) {
      ESP_LOGW(TAG, "Controller runtime response TX flush was not confirmed");
    }
  }

  void publish_controller_state_() {
    if (this->controller_state_sensor_ == nullptr) return;
    const auto signature = controller::unit_signature(this->controller_state_);
    char summary[128];
    std::snprintf(summary, sizeof(summary),
                  "unit=%02X%02X%02X mode_power=0x%02X power=%s secondary=0x%02X setpoint_x2=%u setpoint=%.1fC counter=0x%02X",
                  signature[0], signature[1], signature[2],
                  controller::mode_power_raw(this->controller_state_),
                  controller::power_enabled(this->controller_state_) ? "ON" : "OFF",
                  controller::secondary_control_raw(this->controller_state_),
                  static_cast<unsigned>(controller::setpoint_x2(this->controller_state_)),
                  controller::setpoint_celsius(this->controller_state_),
                  controller::accept_counter(this->controller_state_));
    this->controller_state_sensor_->publish_state(summary);
  }

  void observe_ff40_status_(const protocol::ParsedFrame &frame) {
    status::FF40Status decoded;
    if (!status::decode_ff40(frame, decoded)) return;

    if (this->registered_status_sensor_ != nullptr) {
      this->registered_status_sensor_->publish_state(decoded.registered_layout);
    }
    if (this->ff40_appendix_sensor_ != nullptr) {
      this->ff40_appendix_sensor_->publish_state(
          decoded.registered_layout && !decoded.appendix.empty()
              ? hex_(decoded.appendix)
              : "-");
    }
    if (decoded.has_registered_setpoint_candidate &&
        this->registered_setpoint_candidate_sensor_ != nullptr) {
      this->registered_setpoint_candidate_sensor_->publish_state(
          decoded.registered_setpoint_celsius_candidate);
    }
    if (this->ff40_indexed_sensor_ != nullptr) {
      std::string indexed;
      for (size_t i = 0; i < decoded.payload.size(); ++i) {
        char token[16];
        std::snprintf(token, sizeof(token), "%u:%02X",
                      static_cast<unsigned>(i), decoded.payload[i]);
        if (!indexed.empty()) indexed.push_back(' ');
        indexed += token;
      }
      this->ff40_indexed_sensor_->publish_state(indexed);
    }

    if (decoded.registered_layout) {
      ++this->registered_status_frames_;
      if (this->registration_waiting_for_response_) {
        this->registration_accept_evidence_ = true;
        ESP_LOGI(TAG,
                 "REG evidence: exact registered FF->40 body=0x%02X appendix=%s",
                 decoded.body_length,
                 decoded.appendix.empty() ? "-" : hex_(decoded.appendix).c_str());
      }
    }
  }

  int read_gpio_level_(int gpio) const {
#ifdef USE_ESP32
    if (gpio >= 0) return gpio_get_level(static_cast<gpio_num_t>(gpio));
#endif
    return -1;
  }

  static void IRAM_ATTR rx_edge_isr_(void *arg) {
#ifdef USE_ESP32
    auto *self = static_cast<GreeWiredRS485 *>(arg);
    const uint32_t now_us = static_cast<uint32_t>(esp_timer_get_time());
    ++self->rx_isr_edges_total_;

    // Preserve short inter-edge timing even when the UART profile is wrong.
    // Gaps above 20 ms are treated as inter-burst idle and excluded from the
    // cadence range; the raw edge total still includes them. On UART-like
    // signaling, the minimum observed gap is a useful bit-period clue without
    // asserting that any particular baud/profile is correct for XE71.
    if (self->rx_isr_previous_edge_us_ != 0) {
      const uint32_t cadence_gap_us =
          static_cast<uint32_t>(now_us - self->rx_isr_previous_edge_us_);
      if (cadence_gap_us <= 20000U) {
        ++self->rx_isr_cadence_samples_total_;
        self->rx_isr_cadence_last_gap_us_ = cadence_gap_us;
        if (self->rx_isr_cadence_min_gap_us_ == 0 ||
            cadence_gap_us < self->rx_isr_cadence_min_gap_us_) {
          self->rx_isr_cadence_min_gap_us_ = cadence_gap_us;
        }
        if (cadence_gap_us > self->rx_isr_cadence_max_gap_us_) {
          self->rx_isr_cadence_max_gap_us_ = cadence_gap_us;
        }
      }
    }
    self->rx_isr_previous_edge_us_ = now_us;

    if (self->rx_edge_window_active_) {
      if (self->rx_edge_window_count_ == 0) {
        self->rx_edge_window_first_us_ = now_us;
        self->rx_edge_window_previous_us_ = now_us;
      } else {
        const uint32_t gap_us =
            static_cast<uint32_t>(now_us - self->rx_edge_window_previous_us_);
        if (self->rx_edge_window_min_gap_us_ == 0 ||
            gap_us < self->rx_edge_window_min_gap_us_) {
          self->rx_edge_window_min_gap_us_ = gap_us;
        }
        if (gap_us > self->rx_edge_window_max_gap_us_) {
          self->rx_edge_window_max_gap_us_ = gap_us;
        }
        self->rx_edge_window_previous_us_ = now_us;
      }
      self->rx_edge_window_last_us_ = now_us;
      ++self->rx_edge_window_count_;

      if (self->rx_edge_response_phase_) {
        if (self->rx_edge_response_count_ == 0) {
          self->rx_edge_response_first_us_ = now_us;
          self->rx_edge_response_previous_us_ = now_us;
        } else {
          const uint32_t response_gap_us =
              static_cast<uint32_t>(
                  now_us - self->rx_edge_response_previous_us_);
          if (self->rx_edge_response_min_gap_us_ == 0 ||
              response_gap_us < self->rx_edge_response_min_gap_us_) {
            self->rx_edge_response_min_gap_us_ = response_gap_us;
          }
          if (response_gap_us > self->rx_edge_response_max_gap_us_) {
            self->rx_edge_response_max_gap_us_ = response_gap_us;
          }
          self->rx_edge_response_previous_us_ = now_us;
        }
        self->rx_edge_response_last_us_ = now_us;
        ++self->rx_edge_response_count_;
      }
    }
#else
    (void) arg;
#endif
  }

  void setup_rx_edge_monitor_() {
#ifdef USE_ESP32
    if (this->rx_line_gpio_ < 0 || this->rx_edge_monitor_installed_) return;
    const auto gpio = static_cast<gpio_num_t>(this->rx_line_gpio_);

    if (this->rx_idle_pullup_) {
      esp_err_t pull_err = gpio_pullup_en(gpio);
      if (pull_err == ESP_OK) {
        pull_err = gpio_pulldown_dis(gpio);
      }
      if (pull_err == ESP_OK) {
        this->rx_idle_pullup_active_ = true;
      } else {
        ESP_LOGW(TAG,
                 "RX idle pull-up could not be enabled on GPIO%d: %s",
                 this->rx_line_gpio_, esp_err_to_name(pull_err));
      }
    }

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "RX edge monitor ISR service unavailable on GPIO%d: %s",
               this->rx_line_gpio_, esp_err_to_name(err));
      return;
    }

    err = gpio_set_intr_type(gpio, GPIO_INTR_ANYEDGE);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "RX edge monitor could not set GPIO%d any-edge interrupt: %s",
               this->rx_line_gpio_, esp_err_to_name(err));
      return;
    }
    err = gpio_isr_handler_add(gpio, &GreeWiredRS485::rx_edge_isr_, this);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "RX edge monitor could not attach GPIO%d handler: %s",
               this->rx_line_gpio_, esp_err_to_name(err));
      return;
    }
    err = gpio_intr_enable(gpio);
    if (err != ESP_OK) {
      gpio_isr_handler_remove(gpio);
      ESP_LOGW(TAG, "RX edge monitor could not enable GPIO%d interrupt: %s",
               this->rx_line_gpio_, esp_err_to_name(err));
      return;
    }

    this->rx_edge_monitor_installed_ = true;
    ESP_LOGI(TAG,
             "RX edge monitor active on GPIO%d; UART RX remains unchanged; "
             "idle_pullup=%s",
             this->rx_line_gpio_, YESNO(this->rx_idle_pullup_active_));
#endif
  }

  uint32_t rx_transition_total_() const {
#ifdef USE_ESP32
    if (this->rx_edge_monitor_installed_) return this->rx_isr_edges_total_;
#endif
    return this->rx_line_activity_.total_transitions();
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
      if (this->hardware_half_duplex_) {
        ESP_LOGW(TAG,
                 "RS485 DE observed HIGH while UART half-duplex was idle on GPIO%d",
                 this->direction_gpio_);
      } else {
        ESP_LOGE(TAG,
                 "RS485 DE was observed HIGH despite the software direction guard; forcing "
                 "GPIO%d LOW again",
                 this->direction_gpio_);
        this->force_receive_mode_();
      }
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

    // Changing UART decode settings does not transmit anything on RS485.
    // Hardware-half-duplex deployments may keep GPIO4 registered as UART
    // RTS/DE; this profile-change path itself never writes bus data.
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
    if (this->first_valid_frame_at_ == 0) {
      this->first_valid_frame_at_ = this->last_valid_frame_at_;
    }

    // Registration is synchronized to the target's startup traffic. FF->40
    // provides the target-specific three-byte signature used in controller
    // state frames; 00->FF opens the short controller-registration window.
    this->learn_registration_signature_(frame);
    if (frame.route == protocol::RouteKind::ROUTE_00_FF) {
      this->observe_startup_poll_(this->last_valid_frame_at_);
    }
    if (frame.route == protocol::RouteKind::ROUTE_FF_40) {
      this->observe_ff40_status_(frame);
    }
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
    if (frame.route == protocol::RouteKind::ROUTE_FF_40) {
      if (this->ff40_payload_sensor_ != nullptr) this->ff40_payload_sensor_->publish_state(payload_hex);
      if (this->ff40_changes_sensor_ != nullptr) this->ff40_changes_sensor_->publish_state(changes);
    }
    if (frame.route == protocol::RouteKind::ROUTE_00_FF) {
      if (this->poll_payload_sensor_ != nullptr) this->poll_payload_sensor_->publish_state(payload_hex);
      if (this->poll_changes_sensor_ != nullptr) this->poll_changes_sensor_->publish_state(changes);
    }
    const std::string route = route_text_(frame.source, frame.destination);
    const char *frame_class = protocol::frame_class_name(frame.frame_class);
    const char *frame_role = protocol::frame_role_name(frame.role);

    if (this->startup_frame_trace_.size() < this->startup_frame_trace_limit_) {
      char prefix[64];
      std::snprintf(prefix, sizeof(prefix), "t=%lums role=%s ",
                    static_cast<unsigned long>(this->last_valid_frame_at_),
                    frame_role);
      this->startup_frame_trace_.emplace_back(std::string(prefix) + raw_hex);
    }

    // Publish on every valid source frame, including byte-for-byte duplicates,
    // so Home Assistant timestamps reflect actual bus freshness.
    if (this->last_frame_sensor_ != nullptr) this->last_frame_sensor_->publish_state(raw_hex);
    if (this->last_payload_sensor_ != nullptr) this->last_payload_sensor_->publish_state(payload_hex);
    if (this->last_route_sensor_ != nullptr) this->last_route_sensor_->publish_state(route);
    if (this->last_frame_class_sensor_ != nullptr) {
      this->last_frame_class_sensor_->publish_state(frame_class);
    }
    if (this->last_frame_role_sensor_ != nullptr) {
      this->last_frame_role_sensor_->publish_state(frame_role);
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
               "class=%s role=%s changes=[%s] raw=%s",
               route.c_str(), frame.message_type, static_cast<unsigned>(frame.body_length),
               static_cast<unsigned>(frame.payload.size()), frame.checksum, frame_class,
               frame_role, changes.c_str(), raw_hex.c_str());
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
    if (this->controller_polls_seen_sensor_ != nullptr) {
      this->controller_polls_seen_sensor_->publish_state(this->controller_polls_seen_);
    }
    if (this->controller_responses_sent_sensor_ != nullptr) {
      this->controller_responses_sent_sensor_->publish_state(this->controller_responses_sent_);
    }
    if (this->registered_status_frames_sensor_ != nullptr) {
      this->registered_status_frames_sensor_->publish_state(this->registered_status_frames_);
    }
  }

  protocol::FrameAssembler assembler_;
  diagnostics::LineActivityTracker rx_line_activity_;
  diagnostics::RegistrationRxWindow registration_rx_window_;
  std::map<uint16_t, std::vector<uint8_t>> previous_payloads_;
  std::vector<uint8_t> raw_rx_burst_;
  std::vector<uint8_t> startup_rx_capture_;
  std::vector<uint8_t> registration_response_capture_;
  std::vector<uint8_t> registration_tx_residue_capture_;
  std::vector<std::string> startup_frame_trace_;
  std::vector<std::string> registration_attempt_trace_;
  std::string last_raw_rx_hex_;
  size_t last_raw_rx_size_{0};

  uint32_t frame_timeout_ms_{75};
  uint32_t active_probe_interval_ms_{1200};
  uint32_t silent_bootstrap_delay_ms_{5000};
  uint32_t setup_started_at_{0};
  uint32_t registration_response_quiet_ms_{100};
  uint32_t startup_poll_rearm_gap_ms_{5000};
  uint32_t last_startup_poll_at_{0};
  uint32_t last_registration_at_{0};
  uint32_t last_registration_tx_elapsed_ms_{0};
  uint32_t raw_rx_burst_last_at_{0};
  uint32_t bus_idle_timeout_ms_{10000};
  uint32_t last_byte_at_{0};
  uint32_t last_valid_frame_at_{0};
  uint32_t first_valid_frame_at_{0};
  uint32_t startup_trace_replay_delay_ms_{5000};
  uint32_t registration_trace_replay_delay_ms_{30000};
  uint32_t last_health_log_at_{0};
  uint32_t last_health_byte_count_{0};
  uint32_t last_health_isr_edge_count_{0};
  uint32_t last_health_tx_rx_edge_count_{0};
  uint32_t last_health_turnaround_rx_edge_count_{0};
  uint32_t last_health_tx_residue_byte_count_{0};
  uint32_t health_log_interval_ms_{10000};
  uint32_t passive_scan_window_ms_{2000};
  uint32_t scan_profile_started_at_{0};
  uint32_t scan_profile_byte_start_{0};
  size_t scan_profile_index_{0};
  size_t startup_frame_trace_limit_{16};
  int rx_line_gpio_{-1};
  int direction_gpio_{-1};
  int last_registration_de_before_{-1};
  int last_registration_de_after_{-1};

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
  uint32_t controller_polls_seen_{0};
  uint32_t controller_responses_sent_{0};
  uint32_t registered_status_frames_{0};
  uint32_t last_registration_tx_rx_edges_{0};
  uint32_t last_registration_turnaround_rx_edges_{0};
  uint32_t tx_rx_edges_total_{0};
  uint32_t turnaround_rx_edges_total_{0};
  uint32_t tx_residue_bytes_total_{0};

  bool log_frames_{true};
  bool active_probe_{false};
  bool legacy_gkh_xk76_probe_{false};
  bool silent_bootstrap_probe_{false};
  bool silent_bootstrap_armed_{false};
  bool hardware_half_duplex_{false};
  bool persistent_controller_{false};
  bool rx_idle_pullup_{false};
  bool rx_idle_pullup_active_{false};
  uint8_t registration_attempt_limit_{4};
  uint8_t registration_attempts_sent_{0};
  bool registration_armed_{false};
  bool registration_waiting_for_response_{false};
  bool registration_established_{false};
  bool registration_unit_signature_learned_{false};
  registration::UnitSignature registration_unit_signature_{
      registration::REFERENCE_UNIT_SIGNATURE};
  controller::ControllerState controller_state_{controller::reference_state()};
  bool registration_accept_evidence_{false};
  bool runtime_response_pending_{false};
  bool last_registration_tx_seen_{false};
  bool last_registration_tx_flush_ok_{false};
  bool tx_in_progress_{false};
  bool bus_active_{false};
  bool passive_scan_{false};
  bool scan_locked_{false};
  bool warned_rx_held_low_{false};
  bool warned_edges_without_uart_{false};
  bool direction_high_seen_{false};
  bool startup_trace_replayed_{false};
  bool registration_trace_replayed_{false};
  bool rx_edge_monitor_installed_{false};
#ifdef USE_ESP32
  volatile uint32_t rx_isr_edges_total_{0};
  volatile uint32_t rx_isr_previous_edge_us_{0};
  volatile uint32_t rx_isr_cadence_samples_total_{0};
  volatile uint32_t rx_isr_cadence_min_gap_us_{0};
  volatile uint32_t rx_isr_cadence_max_gap_us_{0};
  volatile uint32_t rx_isr_cadence_last_gap_us_{0};
  volatile bool rx_edge_window_active_{false};
  volatile uint32_t rx_edge_window_count_{0};
  volatile uint32_t rx_edge_window_first_us_{0};
  volatile uint32_t rx_edge_window_last_us_{0};
  volatile uint32_t rx_edge_window_previous_us_{0};
  volatile uint32_t rx_edge_window_min_gap_us_{0};
  volatile uint32_t rx_edge_window_max_gap_us_{0};
  volatile bool rx_edge_response_phase_{false};
  volatile uint32_t rx_edge_response_count_{0};
  volatile uint32_t rx_edge_response_first_us_{0};
  volatile uint32_t rx_edge_response_last_us_{0};
  volatile uint32_t rx_edge_response_previous_us_{0};
  volatile uint32_t rx_edge_response_min_gap_us_{0};
  volatile uint32_t rx_edge_response_max_gap_us_{0};
#else
  uint32_t rx_isr_edges_total_{0};
  uint32_t rx_isr_previous_edge_us_{0};
  uint32_t rx_isr_cadence_samples_total_{0};
  uint32_t rx_isr_cadence_min_gap_us_{0};
  uint32_t rx_isr_cadence_max_gap_us_{0};
  uint32_t rx_isr_cadence_last_gap_us_{0};
  bool rx_edge_window_active_{false};
  uint32_t rx_edge_window_count_{0};
  uint32_t rx_edge_window_first_us_{0};
  uint32_t rx_edge_window_last_us_{0};
  uint32_t rx_edge_window_previous_us_{0};
  uint32_t rx_edge_window_min_gap_us_{0};
  uint32_t rx_edge_window_max_gap_us_{0};
  bool rx_edge_response_phase_{false};
  uint32_t rx_edge_response_count_{0};
  uint32_t rx_edge_response_first_us_{0};
  uint32_t rx_edge_response_last_us_{0};
  uint32_t rx_edge_response_previous_us_{0};
  uint32_t rx_edge_response_min_gap_us_{0};
  uint32_t rx_edge_response_max_gap_us_{0};
#endif

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
  sensor::Sensor *controller_polls_seen_sensor_{nullptr};
  sensor::Sensor *controller_responses_sent_sensor_{nullptr};
  sensor::Sensor *registered_status_frames_sensor_{nullptr};
  sensor::Sensor *registered_setpoint_candidate_sensor_{nullptr};

  text_sensor::TextSensor *last_frame_sensor_{nullptr};
  text_sensor::TextSensor *last_payload_sensor_{nullptr};
  text_sensor::TextSensor *last_route_sensor_{nullptr};
  text_sensor::TextSensor *last_frame_class_sensor_{nullptr};
  text_sensor::TextSensor *last_changes_sensor_{nullptr};
  text_sensor::TextSensor *last_invalid_frame_sensor_{nullptr};
  text_sensor::TextSensor *protocol_sensor_{nullptr};
  text_sensor::TextSensor *serial_profile_sensor_{nullptr};
  text_sensor::TextSensor *last_raw_rx_sensor_{nullptr};
  text_sensor::TextSensor *ff40_appendix_sensor_{nullptr};
  text_sensor::TextSensor *controller_state_sensor_{nullptr};
  text_sensor::TextSensor *ff40_payload_sensor_{nullptr};
  text_sensor::TextSensor *ff40_changes_sensor_{nullptr};
  text_sensor::TextSensor *last_frame_role_sensor_{nullptr};
  text_sensor::TextSensor *poll_payload_sensor_{nullptr};
  text_sensor::TextSensor *poll_changes_sensor_{nullptr};
  text_sensor::TextSensor *ff40_indexed_sensor_{nullptr};

  binary_sensor::BinarySensor *bus_active_sensor_{nullptr};
  binary_sensor::BinarySensor *listen_only_sensor_{nullptr};
  binary_sensor::BinarySensor *rx_line_high_sensor_{nullptr};
  binary_sensor::BinarySensor *direction_high_sensor_{nullptr};
  binary_sensor::BinarySensor *electrical_activity_sensor_{nullptr};
  binary_sensor::BinarySensor *direction_high_seen_sensor_{nullptr};
  binary_sensor::BinarySensor *registered_status_sensor_{nullptr};
};

template<typename... Ts> class SetControllerSetpointAction final : public Action<Ts...> {
 public:
  explicit SetControllerSetpointAction(GreeWiredRS485 *parent) : parent_(parent) {}

  template<typename V> void set_value(V value) { this->value_ = value; }

  void play(const Ts &...x) override {
    const float value = this->value_.value(x...);
    if (!this->parent_->set_controller_setpoint_celsius(value)) {
      ESP_LOGW(TAG, "Rejected staged controller setpoint %.2fC", value);
    }
  }

 protected:
  GreeWiredRS485 *parent_;
  TemplatableValue<float, Ts...> value_{};
};

template<typename... Ts> class SetControllerPowerAction final : public Action<Ts...> {
 public:
  explicit SetControllerPowerAction(GreeWiredRS485 *parent) : parent_(parent) {}

  template<typename V> void set_value(V value) { this->value_ = value; }

  void play(const Ts &...x) override {
    this->parent_->set_controller_power(this->value_.value(x...));
  }

 protected:
  GreeWiredRS485 *parent_;
  TemplatableValue<bool, Ts...> value_{};
};

template<typename... Ts> class SetControllerPayloadByteAction final : public Action<Ts...> {
 public:
  explicit SetControllerPayloadByteAction(GreeWiredRS485 *parent) : parent_(parent) {}

  template<typename V> void set_index(V value) { this->index_ = value; }
  template<typename V> void set_value(V value) { this->value_ = value; }

  void play(const Ts &...x) override {
    const uint8_t index = this->index_.value(x...);
    const uint8_t value = this->value_.value(x...);
    if (!this->parent_->set_controller_payload_byte(index, value)) {
      ESP_LOGW(TAG, "Rejected staged controller payload index %u",
               static_cast<unsigned>(index));
    }
  }

 protected:
  GreeWiredRS485 *parent_;
  TemplatableValue<uint8_t, Ts...> index_{};
  TemplatableValue<uint8_t, Ts...> value_{};
};

template<typename... Ts> class SetControllerModePowerRawAction final : public Action<Ts...> {
 public:
  explicit SetControllerModePowerRawAction(GreeWiredRS485 *parent) : parent_(parent) {}

  template<typename V> void set_value(V value) { this->value_ = value; }

  void play(const Ts &...x) override {
    this->parent_->set_controller_mode_power_raw(this->value_.value(x...));
  }

 protected:
  GreeWiredRS485 *parent_;
  TemplatableValue<uint8_t, Ts...> value_{};
};

}  // namespace gree_wired_rs485
}  // namespace esphome
