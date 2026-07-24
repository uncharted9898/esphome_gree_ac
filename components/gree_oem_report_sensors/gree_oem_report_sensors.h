#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "../sinclair_ac/esppac.h"

namespace esphome {
namespace gree_oem_report_sensors {

static const char *const TAG = "gree.oem_report_sensors";

class GreeOemReportSensors : public PollingComponent {
 public:
  void set_climate(sinclair_ac::SinclairAC *climate) { this->climate_ = climate; }

  void set_last_0x32_payload_sensor(text_sensor::TextSensor *sensor) { this->last_0x32_payload_sensor_ = sensor; }
  void set_last_0x34_payload_sensor(text_sensor::TextSensor *sensor) { this->last_0x34_payload_sensor_ = sensor; }
  void set_last_0x35_payload_sensor(text_sensor::TextSensor *sensor) { this->last_0x35_payload_sensor_ = sensor; }

  void set_indoor_report_target_temperature_sensor(sensor::Sensor *sensor) { this->indoor_report_target_temperature_sensor_ = sensor; }
  void set_indoor_report_current_temperature_sensor(sensor::Sensor *sensor) { this->indoor_report_current_temperature_sensor_ = sensor; }
  void set_indoor_report_byte_10_temperature_hypothesis_sensor(sensor::Sensor *sensor) { this->indoor_report_byte_10_temperature_hypothesis_sensor_ = sensor; }
  void set_indoor_report_byte_25_temperature_hypothesis_sensor(sensor::Sensor *sensor) { this->indoor_report_byte_25_temperature_hypothesis_sensor_ = sensor; }

  void set_outdoor_report_byte_10_raw_sensor(sensor::Sensor *sensor) { this->outdoor_report_byte_10_raw_sensor_ = sensor; }
  void set_outdoor_report_byte_13_temperature_hypothesis_sensor(sensor::Sensor *sensor) { this->outdoor_report_byte_13_temperature_hypothesis_sensor_ = sensor; }
  void set_outdoor_report_byte_14_temperature_hypothesis_sensor(sensor::Sensor *sensor) { this->outdoor_report_byte_14_temperature_hypothesis_sensor_ = sensor; }
  void set_outdoor_report_byte_15_temperature_hypothesis_sensor(sensor::Sensor *sensor) { this->outdoor_report_byte_15_temperature_hypothesis_sensor_ = sensor; }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Gree OEM report sensors:");
    ESP_LOGCONFIG(TAG, "  Update interval: %lu ms", static_cast<unsigned long>(this->get_update_interval()));
  }

  void update() override {
    if (this->climate_ == nullptr) return;
    this->publish_payload_(0x32, this->last_0x32_payload_sensor_, this->last_0x32_payload_);
    this->publish_payload_(0x34, this->last_0x34_payload_sensor_, this->last_0x34_payload_);
    this->publish_payload_(0x35, this->last_0x35_payload_sensor_, this->last_0x35_payload_);
    this->publish_indoor_report_();
    this->publish_outdoor_report_();
  }

 protected:
  static float raw_minus_40_(uint8_t raw) { return static_cast<float>(static_cast<int>(raw) - 40); }

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

  void publish_payload_(uint8_t command, text_sensor::TextSensor *sensor, std::vector<uint8_t> &last) {
    if (sensor == nullptr) return;
    const auto *payload = this->climate_->get_retained_payload(command);
    if (payload == nullptr || *payload == last) return;
    last = *payload;
    sensor->publish_state(format_payload_(*payload));
  }

  void publish_indoor_report_() {
    const auto *payload = this->climate_->get_retained_payload(0x34);
    if (payload == nullptr || payload->size() <= 25 || *payload == this->last_indoor_decoded_payload_) return;
    this->last_indoor_decoded_payload_ = *payload;

    if (this->indoor_report_target_temperature_sensor_ != nullptr)
      this->indoor_report_target_temperature_sensor_->publish_state(raw_minus_40_((*payload)[7]));
    if (this->indoor_report_current_temperature_sensor_ != nullptr)
      this->indoor_report_current_temperature_sensor_->publish_state(raw_minus_40_((*payload)[8]));
    if (this->indoor_report_byte_10_temperature_hypothesis_sensor_ != nullptr)
      this->indoor_report_byte_10_temperature_hypothesis_sensor_->publish_state(raw_minus_40_((*payload)[10]));
    if (this->indoor_report_byte_25_temperature_hypothesis_sensor_ != nullptr)
      this->indoor_report_byte_25_temperature_hypothesis_sensor_->publish_state(raw_minus_40_((*payload)[25]));
  }

  void publish_outdoor_report_() {
    const auto *payload = this->climate_->get_retained_payload(0x35);
    if (payload == nullptr || payload->size() <= 15 || *payload == this->last_outdoor_decoded_payload_) return;
    this->last_outdoor_decoded_payload_ = *payload;

    if (this->outdoor_report_byte_10_raw_sensor_ != nullptr)
      this->outdoor_report_byte_10_raw_sensor_->publish_state((*payload)[10]);
    if (this->outdoor_report_byte_13_temperature_hypothesis_sensor_ != nullptr)
      this->outdoor_report_byte_13_temperature_hypothesis_sensor_->publish_state(raw_minus_40_((*payload)[13]));
    if (this->outdoor_report_byte_14_temperature_hypothesis_sensor_ != nullptr)
      this->outdoor_report_byte_14_temperature_hypothesis_sensor_->publish_state(raw_minus_40_((*payload)[14]));
    if (this->outdoor_report_byte_15_temperature_hypothesis_sensor_ != nullptr)
      this->outdoor_report_byte_15_temperature_hypothesis_sensor_->publish_state(raw_minus_40_((*payload)[15]));
  }

  sinclair_ac::SinclairAC *climate_{nullptr};

  text_sensor::TextSensor *last_0x32_payload_sensor_{nullptr};
  text_sensor::TextSensor *last_0x34_payload_sensor_{nullptr};
  text_sensor::TextSensor *last_0x35_payload_sensor_{nullptr};

  sensor::Sensor *indoor_report_target_temperature_sensor_{nullptr};
  sensor::Sensor *indoor_report_current_temperature_sensor_{nullptr};
  sensor::Sensor *indoor_report_byte_10_temperature_hypothesis_sensor_{nullptr};
  sensor::Sensor *indoor_report_byte_25_temperature_hypothesis_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_10_raw_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_13_temperature_hypothesis_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_14_temperature_hypothesis_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_15_temperature_hypothesis_sensor_{nullptr};

  std::vector<uint8_t> last_0x32_payload_;
  std::vector<uint8_t> last_0x34_payload_;
  std::vector<uint8_t> last_0x35_payload_;
  std::vector<uint8_t> last_indoor_decoded_payload_;
  std::vector<uint8_t> last_outdoor_decoded_payload_;
};

}  // namespace gree_oem_report_sensors
}  // namespace esphome
