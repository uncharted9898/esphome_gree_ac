#pragma once

#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "../sinclair_ac/esppac.h"
#include "report_decoder.h"

namespace esphome {
namespace gree_oem_report_sensors {

static const char *const TAG = "gree.oem_report_sensors";

class GreeOemReportSensors : public PollingComponent {
 public:
  void set_climate(sinclair_ac::SinclairAC *climate) { this->climate_ = climate; }

  void set_last_0x32_payload_sensor(text_sensor::TextSensor *sensor) { this->last_0x32_payload_sensor_ = sensor; }
  void set_last_0x33_payload_sensor(text_sensor::TextSensor *sensor) { this->last_0x33_payload_sensor_ = sensor; }
  void set_last_0x34_payload_sensor(text_sensor::TextSensor *sensor) { this->last_0x34_payload_sensor_ = sensor; }
  void set_last_0x35_payload_sensor(text_sensor::TextSensor *sensor) { this->last_0x35_payload_sensor_ = sensor; }
  void set_last_0x40_payload_sensor(text_sensor::TextSensor *sensor) { this->last_0x40_payload_sensor_ = sensor; }
  void set_power_discovery_summary_sensor(text_sensor::TextSensor *sensor) { this->power_discovery_summary_sensor_ = sensor; }

  void set_indoor_report_target_temperature_sensor(sensor::Sensor *sensor) { this->indoor_report_target_temperature_sensor_ = sensor; }
  void set_indoor_report_current_temperature_sensor(sensor::Sensor *sensor) { this->indoor_report_current_temperature_sensor_ = sensor; }
  void set_indoor_coil_temperature_candidate_sensor(sensor::Sensor *sensor) { this->indoor_coil_temperature_candidate_sensor_ = sensor; }
  void set_indoor_secondary_temperature_candidate_sensor(sensor::Sensor *sensor) { this->indoor_secondary_temperature_candidate_sensor_ = sensor; }
  void set_indoor_report_byte_10_temperature_hypothesis_sensor(sensor::Sensor *sensor) { this->indoor_report_byte_10_temperature_hypothesis_sensor_ = sensor; }
  void set_indoor_report_byte_25_temperature_hypothesis_sensor(sensor::Sensor *sensor) { this->indoor_report_byte_25_temperature_hypothesis_sensor_ = sensor; }

  void set_outdoor_operating_value_raw_sensor(sensor::Sensor *sensor) { this->outdoor_operating_value_raw_sensor_ = sensor; }
  void set_outdoor_ambient_temperature_candidate_sensor(sensor::Sensor *sensor) { this->outdoor_ambient_temperature_candidate_sensor_ = sensor; }
  void set_outdoor_coil_temperature_candidate_sensor(sensor::Sensor *sensor) { this->outdoor_coil_temperature_candidate_sensor_ = sensor; }
  void set_compressor_discharge_temperature_candidate_sensor(sensor::Sensor *sensor) { this->compressor_discharge_temperature_candidate_sensor_ = sensor; }
  void set_outdoor_report_byte_10_raw_sensor(sensor::Sensor *sensor) { this->outdoor_report_byte_10_raw_sensor_ = sensor; }
  void set_outdoor_report_byte_13_temperature_hypothesis_sensor(sensor::Sensor *sensor) { this->outdoor_report_byte_13_temperature_hypothesis_sensor_ = sensor; }
  void set_outdoor_report_byte_14_temperature_hypothesis_sensor(sensor::Sensor *sensor) { this->outdoor_report_byte_14_temperature_hypothesis_sensor_ = sensor; }
  void set_outdoor_report_byte_15_temperature_hypothesis_sensor(sensor::Sensor *sensor) { this->outdoor_report_byte_15_temperature_hypothesis_sensor_ = sensor; }

  void set_indoor_report_byte_6_raw_sensor(sensor::Sensor *sensor) { this->indoor_report_byte_6_raw_sensor_ = sensor; }
  void set_indoor_report_byte_6_bit_5_sensor(binary_sensor::BinarySensor *sensor) { this->indoor_report_byte_6_bit_5_sensor_ = sensor; }
  void set_indoor_report_byte_14_bit_5_sensor(binary_sensor::BinarySensor *sensor) { this->indoor_report_byte_14_bit_5_sensor_ = sensor; }
  void set_indoor_report_byte_20_raw_sensor(sensor::Sensor *sensor) { this->indoor_report_byte_20_raw_sensor_ = sensor; }
  void set_indoor_report_byte_21_raw_sensor(sensor::Sensor *sensor) { this->indoor_report_byte_21_raw_sensor_ = sensor; }
  void set_indoor_report_byte_36_raw_sensor(sensor::Sensor *sensor) { this->indoor_report_byte_36_raw_sensor_ = sensor; }
  void set_indoor_report_byte_37_raw_sensor(sensor::Sensor *sensor) { this->indoor_report_byte_37_raw_sensor_ = sensor; }
  void set_indoor_report_byte_38_raw_sensor(sensor::Sensor *sensor) { this->indoor_report_byte_38_raw_sensor_ = sensor; }

  void set_outdoor_report_byte_17_bit_2_sensor(binary_sensor::BinarySensor *sensor) { this->outdoor_report_byte_17_bit_2_sensor_ = sensor; }
  void set_outdoor_report_bytes_21_28_raw_sensor(text_sensor::TextSensor *sensor) { this->outdoor_report_bytes_21_28_raw_sensor_ = sensor; }
  void set_outdoor_report_byte_30_raw_sensor(sensor::Sensor *sensor) { this->outdoor_report_byte_30_raw_sensor_ = sensor; }
  void set_outdoor_report_byte_31_bit_6_sensor(binary_sensor::BinarySensor *sensor) { this->outdoor_report_byte_31_bit_6_sensor_ = sensor; }
  void set_outdoor_report_byte_36_raw_sensor(sensor::Sensor *sensor) { this->outdoor_report_byte_36_raw_sensor_ = sensor; }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Gree OEM report sensors:");
    ESP_LOGCONFIG(TAG, "  Update interval: %lu ms", static_cast<unsigned long>(this->get_update_interval()));
  }

  void update() override {
    if (this->climate_ == nullptr) return;
    this->publish_payload_(0x32, this->last_0x32_payload_sensor_, this->last_0x32_payload_);
    this->publish_payload_(0x33, this->last_0x33_payload_sensor_, this->last_0x33_payload_);
    this->publish_payload_(0x34, this->last_0x34_payload_sensor_, this->last_0x34_payload_);
    this->publish_payload_(0x35, this->last_0x35_payload_sensor_, this->last_0x35_payload_);
    this->publish_payload_(0x40, this->last_0x40_payload_sensor_, this->last_0x40_payload_);
    this->publish_indoor_report_();
    this->publish_outdoor_report_();
    this->publish_power_discovery_();
  }

 protected:
  static void publish_sensor_(sensor::Sensor *sensor, float value) { if (sensor != nullptr) sensor->publish_state(value); }
  static void publish_binary_sensor_(binary_sensor::BinarySensor *sensor, bool value) { if (sensor != nullptr) sensor->publish_state(value); }

  static std::string format_payload_(const std::vector<uint8_t> &payload) {
    static const char digits[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(payload.size() * 3 + 8);
    for (size_t i = 0; i < payload.size(); ++i) {
      if (i != 0) out.push_back('.');
      out.push_back(digits[payload[i] >> 4]);
      out.push_back(digits[payload[i] & 0x0F]);
    }
    out += " (" + std::to_string(payload.size()) + ")";
    return out;
  }

  template<size_t N> static std::string format_bytes_(const std::array<uint8_t, N> &bytes) {
    static const char digits[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(bytes.size() * 3);
    for (size_t i = 0; i < bytes.size(); ++i) {
      if (i != 0) out.push_back('.');
      out.push_back(digits[bytes[i] >> 4]);
      out.push_back(digits[bytes[i] & 0x0F]);
    }
    return out;
  }

  static std::string power_summary_(const std::vector<uint8_t> *energy, const std::vector<uint8_t> *outdoor) {
    std::ostringstream out;
    if (energy == nullptr) {
      out << "0x40=no-response";
    } else {
      out << "0x40 len=" << energy->size() << " bytes=" << format_payload_(*energy);
      out << " u16le=";
      const size_t limit = energy->size() > 20 ? 20 : energy->size();
      for (size_t i = 0; i + 1 < limit; i += 2) {
        if (i != 0) out << ',';
        out << i << ':' << (static_cast<uint16_t>((*energy)[i]) | (static_cast<uint16_t>((*energy)[i + 1]) << 8));
      }
      out << " u16be=";
      for (size_t i = 0; i + 1 < limit; i += 2) {
        if (i != 0) out << ',';
        out << i << ':' << ((static_cast<uint16_t>((*energy)[i]) << 8) | (*energy)[i + 1]);
      }
    }
    if (outdoor != nullptr && outdoor->size() > 15) {
      out << "; 0x35 b10=" << unsigned((*outdoor)[10])
          << " b13=" << unsigned((*outdoor)[13])
          << " b14=" << unsigned((*outdoor)[14])
          << " b15=" << unsigned((*outdoor)[15]);
    }
    return out.str();
  }

  void publish_payload_(uint8_t command, text_sensor::TextSensor *sensor, std::vector<uint8_t> &last) {
    if (sensor == nullptr) return;
    const auto *payload = this->climate_->get_retained_payload(command);
    if (payload == nullptr || *payload == last) return;
    last = *payload;
    sensor->publish_state(format_payload_(*payload));
  }

  void publish_power_discovery_() {
    if (this->power_discovery_summary_sensor_ == nullptr) return;
    const auto *energy = this->climate_->get_retained_payload(0x40);
    const auto *outdoor = this->climate_->get_retained_payload(0x35);
    const std::string state = power_summary_(energy, outdoor);
    if (state == this->last_power_discovery_summary_) return;
    this->last_power_discovery_summary_ = state;
    this->power_discovery_summary_sensor_->publish_state(state);
  }

  void publish_indoor_report_() {
    const auto *payload = this->climate_->get_retained_payload(0x34);
    if (payload == nullptr || *payload == this->last_indoor_decoded_payload_) return;
    IndoorReportFields fields;
    if (!decode_indoor_report(*payload, fields)) return;
    this->last_indoor_decoded_payload_ = *payload;

    publish_sensor_(this->indoor_report_target_temperature_sensor_, fields.target_temperature_c);
    publish_sensor_(this->indoor_report_current_temperature_sensor_, fields.current_temperature_c);
    publish_sensor_(this->indoor_coil_temperature_candidate_sensor_, fields.coil_temperature_candidate_c);
    publish_sensor_(this->indoor_secondary_temperature_candidate_sensor_, fields.secondary_temperature_candidate_c);
    publish_sensor_(this->indoor_report_byte_10_temperature_hypothesis_sensor_, fields.coil_temperature_candidate_c);
    publish_sensor_(this->indoor_report_byte_25_temperature_hypothesis_sensor_, fields.secondary_temperature_candidate_c);
    publish_sensor_(this->indoor_report_byte_6_raw_sensor_, fields.byte_6_raw);
    publish_binary_sensor_(this->indoor_report_byte_6_bit_5_sensor_, fields.byte_6_bit_5);
    publish_binary_sensor_(this->indoor_report_byte_14_bit_5_sensor_, fields.byte_14_bit_5);
    publish_sensor_(this->indoor_report_byte_20_raw_sensor_, fields.byte_20_raw);
    publish_sensor_(this->indoor_report_byte_21_raw_sensor_, fields.byte_21_raw);
    publish_sensor_(this->indoor_report_byte_36_raw_sensor_, fields.byte_36_raw);
    publish_sensor_(this->indoor_report_byte_37_raw_sensor_, fields.byte_37_raw);
    publish_sensor_(this->indoor_report_byte_38_raw_sensor_, fields.byte_38_raw);
  }

  void publish_outdoor_report_() {
    const auto *payload = this->climate_->get_retained_payload(0x35);
    if (payload == nullptr || *payload == this->last_outdoor_decoded_payload_) return;
    OutdoorReportFields fields;
    if (!decode_outdoor_report(*payload, fields)) return;
    this->last_outdoor_decoded_payload_ = *payload;

    publish_sensor_(this->outdoor_operating_value_raw_sensor_, fields.operating_value_raw);
    publish_sensor_(this->outdoor_ambient_temperature_candidate_sensor_, fields.ambient_temperature_candidate_c);
    publish_sensor_(this->outdoor_coil_temperature_candidate_sensor_, fields.coil_temperature_candidate_c);
    publish_sensor_(this->compressor_discharge_temperature_candidate_sensor_, fields.compressor_discharge_temperature_candidate_c);
    publish_sensor_(this->outdoor_report_byte_10_raw_sensor_, fields.operating_value_raw);
    publish_sensor_(this->outdoor_report_byte_13_temperature_hypothesis_sensor_, fields.ambient_temperature_candidate_c);
    publish_sensor_(this->outdoor_report_byte_14_temperature_hypothesis_sensor_, fields.coil_temperature_candidate_c);
    publish_sensor_(this->outdoor_report_byte_15_temperature_hypothesis_sensor_, fields.compressor_discharge_temperature_candidate_c);
    publish_binary_sensor_(this->outdoor_report_byte_17_bit_2_sensor_, fields.byte_17_bit_2);
    if (this->outdoor_report_bytes_21_28_raw_sensor_ != nullptr) this->outdoor_report_bytes_21_28_raw_sensor_->publish_state(format_bytes_(fields.bytes_21_28_raw));
    publish_sensor_(this->outdoor_report_byte_30_raw_sensor_, fields.byte_30_raw);
    publish_binary_sensor_(this->outdoor_report_byte_31_bit_6_sensor_, fields.byte_31_bit_6);
    publish_sensor_(this->outdoor_report_byte_36_raw_sensor_, fields.byte_36_raw);
  }

  sinclair_ac::SinclairAC *climate_{nullptr};
  text_sensor::TextSensor *last_0x32_payload_sensor_{nullptr}, *last_0x33_payload_sensor_{nullptr}, *last_0x34_payload_sensor_{nullptr}, *last_0x35_payload_sensor_{nullptr}, *last_0x40_payload_sensor_{nullptr};
  text_sensor::TextSensor *power_discovery_summary_sensor_{nullptr};
  text_sensor::TextSensor *outdoor_report_bytes_21_28_raw_sensor_{nullptr};

  sensor::Sensor *indoor_report_target_temperature_sensor_{nullptr}, *indoor_report_current_temperature_sensor_{nullptr};
  sensor::Sensor *indoor_coil_temperature_candidate_sensor_{nullptr}, *indoor_secondary_temperature_candidate_sensor_{nullptr};
  sensor::Sensor *indoor_report_byte_10_temperature_hypothesis_sensor_{nullptr}, *indoor_report_byte_25_temperature_hypothesis_sensor_{nullptr};
  sensor::Sensor *outdoor_operating_value_raw_sensor_{nullptr}, *outdoor_ambient_temperature_candidate_sensor_{nullptr};
  sensor::Sensor *outdoor_coil_temperature_candidate_sensor_{nullptr}, *compressor_discharge_temperature_candidate_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_10_raw_sensor_{nullptr}, *outdoor_report_byte_13_temperature_hypothesis_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_14_temperature_hypothesis_sensor_{nullptr}, *outdoor_report_byte_15_temperature_hypothesis_sensor_{nullptr};
  sensor::Sensor *indoor_report_byte_6_raw_sensor_{nullptr}, *indoor_report_byte_20_raw_sensor_{nullptr}, *indoor_report_byte_21_raw_sensor_{nullptr};
  sensor::Sensor *indoor_report_byte_36_raw_sensor_{nullptr}, *indoor_report_byte_37_raw_sensor_{nullptr}, *indoor_report_byte_38_raw_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_30_raw_sensor_{nullptr}, *outdoor_report_byte_36_raw_sensor_{nullptr};

  binary_sensor::BinarySensor *indoor_report_byte_6_bit_5_sensor_{nullptr}, *indoor_report_byte_14_bit_5_sensor_{nullptr};
  binary_sensor::BinarySensor *outdoor_report_byte_17_bit_2_sensor_{nullptr}, *outdoor_report_byte_31_bit_6_sensor_{nullptr};

  std::vector<uint8_t> last_0x32_payload_, last_0x33_payload_, last_0x34_payload_, last_0x35_payload_, last_0x40_payload_;
  std::vector<uint8_t> last_indoor_decoded_payload_, last_outdoor_decoded_payload_;
  std::string last_power_discovery_summary_;
};

}  // namespace gree_oem_report_sensors
}  // namespace esphome
