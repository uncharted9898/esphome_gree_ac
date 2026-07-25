#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "../sinclair_ac/esppac.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "report_decoder.h"

namespace esphome {
namespace gree_oem_report_sensors {

static const char *const TAG = "gree.oem_report_sensors";

class GreeOemReportSensors : public PollingComponent {
 public:
  void set_climate(sinclair_ac::SinclairAC *climate) { this->climate_ = climate; }

  void set_last_0x32_payload_sensor(text_sensor::TextSensor *s) { this->last_0x32_payload_sensor_ = s; }
  void set_last_0x33_payload_sensor(text_sensor::TextSensor *s) { this->last_0x33_payload_sensor_ = s; }
  void set_last_0x34_payload_sensor(text_sensor::TextSensor *s) { this->last_0x34_payload_sensor_ = s; }
  void set_last_0x35_payload_sensor(text_sensor::TextSensor *s) { this->last_0x35_payload_sensor_ = s; }
  void set_last_0x36_payload_sensor(text_sensor::TextSensor *s) { this->last_0x36_payload_sensor_ = s; }
  void set_last_0x3c_payload_sensor(text_sensor::TextSensor *s) { this->last_0x3c_payload_sensor_ = s; }
  void set_last_0x40_payload_sensor(text_sensor::TextSensor *s) { this->last_0x40_payload_sensor_ = s; }
  void set_last_0x41_payload_sensor(text_sensor::TextSensor *s) { this->last_0x41_payload_sensor_ = s; }
  void set_last_0x42_payload_sensor(text_sensor::TextSensor *s) { this->last_0x42_payload_sensor_ = s; }
  void set_last_0x53_payload_sensor(text_sensor::TextSensor *s) { this->last_0x53_payload_sensor_ = s; }
  void set_electrical_energy_capability_sensor(text_sensor::TextSensor *s) {
    this->electrical_energy_capability_sensor_ = s;
  }
  void set_power_discovery_summary_sensor(text_sensor::TextSensor *s) {
    this->power_discovery_summary_sensor_ = s;
  }

  void set_status_indoor_temperature_sensor(sensor::Sensor *s) {
    this->status_indoor_temperature_sensor_ = s;
  }
  void set_status_outdoor_ambient_temperature_sensor(sensor::Sensor *s) {
    this->status_outdoor_ambient_temperature_sensor_ = s;
  }
  void set_status_humidity_sensor_field_raw_sensor(sensor::Sensor *s) {
    this->status_humidity_sensor_field_raw_sensor_ = s;
  }
  void set_status_indoor_fan_port_raw_sensor(sensor::Sensor *s) {
    this->status_indoor_fan_port_raw_sensor_ = s;
  }
  void set_status_elc_erg_sensor(binary_sensor::BinarySensor *s) {
    this->status_elc_erg_sensor_ = s;
  }
  void set_status_elc_gear_raw_sensor(sensor::Sensor *s) {
    this->status_elc_gear_raw_sensor_ = s;
  }
  void set_status_elc_1kwh_raw_sensor(sensor::Sensor *s) {
    this->status_elc_1kwh_raw_sensor_ = s;
  }

  void set_indoor_report_target_temperature_sensor(sensor::Sensor *s) {
    this->indoor_report_target_temperature_sensor_ = s;
  }
  void set_indoor_report_current_temperature_sensor(sensor::Sensor *s) {
    this->indoor_report_current_temperature_sensor_ = s;
  }
  void set_indoor_coil_temperature_candidate_sensor(sensor::Sensor *s) {
    this->indoor_coil_temperature_candidate_sensor_ = s;
  }
  void set_indoor_secondary_temperature_candidate_sensor(sensor::Sensor *s) {
    this->indoor_secondary_temperature_candidate_sensor_ = s;
  }
  void set_indoor_report_byte_10_temperature_hypothesis_sensor(sensor::Sensor *s) {
    this->indoor_report_byte_10_temperature_hypothesis_sensor_ = s;
  }
  void set_indoor_report_byte_25_temperature_hypothesis_sensor(sensor::Sensor *s) {
    this->indoor_report_byte_25_temperature_hypothesis_sensor_ = s;
  }
  void set_indoor_report_byte_25_raw_sensor(sensor::Sensor *s) {
    this->indoor_report_byte_25_raw_sensor_ = s;
  }
  void set_indoor_report_byte_6_raw_sensor(sensor::Sensor *s) {
    this->indoor_report_byte_6_raw_sensor_ = s;
  }
  void set_indoor_report_byte_6_bit_3_sensor(binary_sensor::BinarySensor *s) {
    this->indoor_report_byte_6_bit_3_sensor_ = s;
  }
  void set_indoor_report_byte_6_bit_5_sensor(binary_sensor::BinarySensor *s) {
    this->indoor_report_byte_6_bit_5_sensor_ = s;
  }
  void set_indoor_report_df_point_raw_sensor(sensor::Sensor *s) {
    this->indoor_report_df_point_raw_sensor_ = s;
  }
  void set_indoor_report_cps_temperature_raw_sensor(sensor::Sensor *s) {
    this->indoor_report_cps_temperature_raw_sensor_ = s;
  }
  void set_indoor_report_byte_14_bit_5_sensor(binary_sensor::BinarySensor *s) {
    this->indoor_report_byte_14_bit_5_sensor_ = s;
  }
  void set_indoor_report_byte_20_raw_sensor(sensor::Sensor *s) { this->indoor_report_byte_20_raw_sensor_ = s; }
  void set_indoor_report_byte_21_raw_sensor(sensor::Sensor *s) { this->indoor_report_byte_21_raw_sensor_ = s; }
  void set_indoor_report_byte_36_raw_sensor(sensor::Sensor *s) { this->indoor_report_byte_36_raw_sensor_ = s; }
  void set_indoor_report_byte_37_raw_sensor(sensor::Sensor *s) { this->indoor_report_byte_37_raw_sensor_ = s; }
  void set_indoor_report_byte_38_raw_sensor(sensor::Sensor *s) { this->indoor_report_byte_38_raw_sensor_ = s; }

  // Legacy alias retained for existing YAML. RTL8720CF plus the matching
  // outdoor-controller page identify this byte as the EEV position/setting.
  void set_outdoor_operating_value_raw_sensor(sensor::Sensor *s) {
    this->outdoor_operating_value_raw_sensor_ = s;
  }
  void set_outdoor_ambient_temperature_candidate_sensor(sensor::Sensor *s) {
    this->outdoor_ambient_temperature_candidate_sensor_ = s;
  }
  void set_outdoor_coil_temperature_candidate_sensor(sensor::Sensor *s) {
    this->outdoor_coil_temperature_candidate_sensor_ = s;
  }
  void set_compressor_discharge_temperature_candidate_sensor(sensor::Sensor *s) {
    this->compressor_discharge_temperature_candidate_sensor_ = s;
  }
  void set_compressor_frequency_sensor(sensor::Sensor *s) { this->compressor_frequency_sensor_ = s; }
  void set_outdoor_fan_speed_raw_sensor(sensor::Sensor *s) { this->outdoor_fan_speed_raw_sensor_ = s; }
  void set_expansion_valve_position_sensor(sensor::Sensor *s) { this->expansion_valve_position_sensor_ = s; }
  void set_expansion_valve_closing_sensor(binary_sensor::BinarySensor *s) {
    this->expansion_valve_closing_sensor_ = s;
  }
  void set_outdoor_operating_state_raw_sensor(sensor::Sensor *s) {
    this->outdoor_operating_state_raw_sensor_ = s;
  }
  void set_outdoor_report_byte_4_raw_sensor(sensor::Sensor *s) { this->outdoor_report_byte_4_raw_sensor_ = s; }
  void set_outdoor_report_byte_4_bit_4_sensor(binary_sensor::BinarySensor *s) {
    this->outdoor_report_byte_4_bit_4_sensor_ = s;
  }
  void set_outdoor_report_byte_5_raw_sensor(sensor::Sensor *s) { this->outdoor_report_byte_5_raw_sensor_ = s; }
  void set_outdoor_report_byte_6_raw_sensor(sensor::Sensor *s) { this->outdoor_report_byte_6_raw_sensor_ = s; }
  void set_outdoor_report_byte_9_raw_sensor(sensor::Sensor *s) { this->outdoor_report_byte_9_raw_sensor_ = s; }
  void set_outdoor_report_byte_10_raw_sensor(sensor::Sensor *s) { this->outdoor_report_byte_10_raw_sensor_ = s; }
  void set_outdoor_report_byte_13_temperature_hypothesis_sensor(sensor::Sensor *s) {
    this->outdoor_report_byte_13_temperature_hypothesis_sensor_ = s;
  }
  void set_outdoor_report_byte_14_temperature_hypothesis_sensor(sensor::Sensor *s) {
    this->outdoor_report_byte_14_temperature_hypothesis_sensor_ = s;
  }
  void set_outdoor_report_byte_15_temperature_hypothesis_sensor(sensor::Sensor *s) {
    this->outdoor_report_byte_15_temperature_hypothesis_sensor_ = s;
  }
  void set_outdoor_report_byte_17_bit_2_sensor(binary_sensor::BinarySensor *s) {
    this->outdoor_report_byte_17_bit_2_sensor_ = s;
  }
  void set_outdoor_report_bytes_21_28_raw_sensor(text_sensor::TextSensor *s) {
    this->outdoor_report_bytes_21_28_raw_sensor_ = s;
  }
  void set_outdoor_report_byte_30_raw_sensor(sensor::Sensor *s) { this->outdoor_report_byte_30_raw_sensor_ = s; }
  void set_outdoor_report_byte_31_bit_6_sensor(binary_sensor::BinarySensor *s) {
    this->outdoor_report_byte_31_bit_6_sensor_ = s;
  }
  void set_outdoor_report_byte_36_raw_sensor(sensor::Sensor *s) { this->outdoor_report_byte_36_raw_sensor_ = s; }
  void set_energy_flow_raw_sensor(sensor::Sensor *s) { this->energy_flow_raw_sensor_ = s; }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Gree OEM report sensors:");
    ESP_LOGCONFIG(TAG, "  Update interval: %lu ms",
                  static_cast<unsigned long>(this->get_update_interval()));
  }

  void update() override {
    if (this->climate_ == nullptr) return;

    this->publish_payload_(0x32, this->last_0x32_payload_sensor_, this->last_0x32_payload_);
    this->publish_payload_(0x33, this->last_0x33_payload_sensor_, this->last_0x33_payload_);
    this->publish_payload_(0x34, this->last_0x34_payload_sensor_, this->last_0x34_payload_);
    this->publish_payload_(0x35, this->last_0x35_payload_sensor_, this->last_0x35_payload_);
    this->publish_payload_(0x36, this->last_0x36_payload_sensor_, this->last_0x36_payload_);
    this->publish_payload_(0x3C, this->last_0x3c_payload_sensor_, this->last_0x3c_payload_);
    this->publish_payload_(0x40, this->last_0x40_payload_sensor_, this->last_0x40_payload_);
    this->publish_payload_(0x41, this->last_0x41_payload_sensor_, this->last_0x41_payload_);
    this->publish_payload_(0x42, this->last_0x42_payload_sensor_, this->last_0x42_payload_);
    this->publish_payload_(0x53, this->last_0x53_payload_sensor_, this->last_0x53_payload_);

    this->publish_capability_();
    this->publish_status_report_();
    this->publish_indoor_report_();
    this->publish_outdoor_report_();
    this->publish_energy_flow_report_();
    this->publish_power_discovery_();
  }

 protected:
  static void publish_sensor_(sensor::Sensor *sensor, float value) {
    if (sensor != nullptr && (!sensor->has_state() || sensor->state != value)) {
      sensor->publish_state(value);
    }
  }
  static void publish_finite_sensor_(sensor::Sensor *sensor, float value) {
    if (sensor != nullptr && std::isfinite(value) &&
        (!sensor->has_state() || sensor->state != value)) {
      sensor->publish_state(value);
    }
  }
  static void publish_binary_sensor_(binary_sensor::BinarySensor *sensor, bool value) {
    if (sensor != nullptr && (!sensor->has_state() || sensor->state != value)) {
      sensor->publish_state(value);
    }
  }
  static void publish_text_sensor_(text_sensor::TextSensor *sensor, const std::string &value) {
    if (sensor != nullptr && (!sensor->has_state() || sensor->state != value)) {
      sensor->publish_state(value);
    }
  }

  void publish_stabilized_temperature_(sensor::Sensor *sensor,
                                       sinclair_ac::TemperatureStabilizer &stabilizer,
                                       float value) {
    if (sensor == nullptr) return;
    float accepted = value;
    if (stabilizer.process(value, millis(), this->climate_->temperature_stabilization_active(),
                           this->climate_->temperature_stabilization_settle_time_ms(),
                           this->climate_->temperature_stabilization_immediate_delta_c(), accepted)) {
      publish_sensor_(sensor, accepted);
    }
  }

  static const char *capability_state_(ElectricalEnergyCapability capability) {
    switch (capability) {
      case ElectricalEnergyCapability::NO_SYNCHRONIZATION: return "no_0x32";
      case ElectricalEnergyCapability::PAGE_NOT_ADVERTISED: return "page_not_advertised";
      case ElectricalEnergyCapability::PAGE_ADVERTISED_NO_REPORT: return "page_advertised_no_0x40";
      case ElectricalEnergyCapability::MALFORMED_REPORT: return "malformed_0x40";
      case ElectricalEnergyCapability::REPORT_UNSUPPORTED: return "unsupported_by_0x40";
      case ElectricalEnergyCapability::REPORT_SUPPORTED_ELC_EN_UNKNOWN: return "report_supported_elcen_unknown";
      case ElectricalEnergyCapability::ELC_EN_DISABLED: return "elcen_disabled";
      case ElectricalEnergyCapability::ELC_EN_ENABLED: return "elcen_enabled";
      default: return "unknown";
    }
  }

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

  template<size_t N>
  static std::string format_bytes_(const std::array<uint8_t, N> &bytes) {
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

  void publish_payload_(uint8_t command, text_sensor::TextSensor *sensor,
                        std::vector<uint8_t> &last) {
    if (sensor == nullptr) return;
    const auto *payload = this->climate_->get_retained_payload(command);
    if (payload == nullptr || *payload == last) return;
    last = *payload;
    sensor->publish_state(format_payload_(*payload));
  }

  void publish_capability_() {
    if (this->electrical_energy_capability_sensor_ == nullptr) return;
    const auto capability = decode_electrical_energy_capability(
        this->climate_->get_retained_payload(0x32),
        this->climate_->get_retained_payload(0x40));
    if (this->has_published_capability_ && capability == this->last_capability_) return;
    this->last_capability_ = capability;
    this->has_published_capability_ = true;
    this->electrical_energy_capability_sensor_->publish_state(
        capability_state_(capability));
  }

  void publish_status_report_() {
    const uint32_t generation = this->climate_->get_retained_payload_generation(0x31);
    if (generation == 0 || generation == this->last_status_generation_) return;
    const auto *payload = this->climate_->get_retained_payload(0x31);
    if (payload == nullptr) return;
    StatusReportFields fields;
    if (!decode_status_report(*payload, fields)) return;
    this->last_status_generation_ = generation;

    publish_sensor_(this->status_humidity_sensor_field_raw_sensor_,
                    fields.humidity_sensor_field_raw);
    publish_sensor_(this->status_indoor_fan_port_raw_sensor_,
                    fields.indoor_fan_port_raw);
    publish_binary_sensor_(this->status_elc_erg_sensor_, fields.elc_erg_flag);
    publish_sensor_(this->status_elc_gear_raw_sensor_, fields.elc_gear_raw);
    publish_stabilized_temperature_(this->status_indoor_temperature_sensor_,
                                    this->status_indoor_temperature_stabilizer_,
                                    fields.indoor_temperature_c);
    publish_stabilized_temperature_(this->status_outdoor_ambient_temperature_sensor_,
                                    this->status_outdoor_temperature_stabilizer_,
                                    fields.outdoor_ambient_temperature_c);
    publish_sensor_(this->status_elc_1kwh_raw_sensor_, fields.elc_1kwh_raw);
  }

  void publish_indoor_report_() {
    const auto *payload = this->climate_->get_retained_payload(0x34);
    if (payload == nullptr || *payload == this->last_indoor_decoded_payload_) return;
    IndoorReportFields fields;
    if (!decode_indoor_report(*payload, fields)) return;
    this->last_indoor_decoded_payload_ = *payload;

    publish_sensor_(this->indoor_report_target_temperature_sensor_,
                    fields.target_temperature_c);
    publish_sensor_(this->indoor_report_current_temperature_sensor_,
                    fields.current_temperature_c);
    publish_sensor_(this->indoor_coil_temperature_candidate_sensor_,
                    fields.evaporator_temperature_c);
    publish_finite_sensor_(this->indoor_secondary_temperature_candidate_sensor_,
                           fields.secondary_temperature_candidate_c);
    publish_sensor_(this->indoor_report_byte_10_temperature_hypothesis_sensor_,
                    fields.evaporator_temperature_c);
    publish_finite_sensor_(this->indoor_report_byte_25_temperature_hypothesis_sensor_,
                           fields.secondary_temperature_candidate_c);
    publish_sensor_(this->indoor_report_byte_25_raw_sensor_, fields.byte_25_raw);
    publish_sensor_(this->indoor_report_byte_6_raw_sensor_, fields.byte_6_raw);
    publish_binary_sensor_(this->indoor_report_byte_6_bit_3_sensor_,
                           fields.byte_6_bit_3);
    publish_binary_sensor_(this->indoor_report_byte_6_bit_5_sensor_,
                           fields.byte_6_bit_5);
    publish_sensor_(this->indoor_report_df_point_raw_sensor_, fields.df_point_raw);
    publish_sensor_(this->indoor_report_cps_temperature_raw_sensor_,
                    fields.cps_temperature_raw);
    publish_binary_sensor_(this->indoor_report_byte_14_bit_5_sensor_,
                           fields.byte_14_bit_5);
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

    publish_sensor_(this->outdoor_operating_value_raw_sensor_, fields.byte_10_raw);
    publish_sensor_(this->outdoor_ambient_temperature_candidate_sensor_,
                    fields.ambient_temperature_candidate_c);
    publish_sensor_(this->outdoor_coil_temperature_candidate_sensor_,
                    fields.coil_temperature_candidate_c);
    publish_sensor_(this->compressor_discharge_temperature_candidate_sensor_,
                    fields.compressor_discharge_temperature_candidate_c);
    publish_sensor_(this->compressor_frequency_sensor_,
                    fields.compressor_frequency_raw);
    publish_sensor_(this->outdoor_fan_speed_raw_sensor_,
                    fields.outdoor_fan_speed_raw);
    publish_sensor_(this->expansion_valve_position_sensor_,
                    fields.expansion_valve_position);
    publish_binary_sensor_(this->expansion_valve_closing_sensor_,
                           fields.expansion_valve_closing);
    publish_sensor_(this->outdoor_operating_state_raw_sensor_, fields.byte_4_raw);
    publish_sensor_(this->outdoor_report_byte_4_raw_sensor_, fields.byte_4_raw);
    publish_binary_sensor_(this->outdoor_report_byte_4_bit_4_sensor_,
                           fields.byte_4_bit_4);
    publish_sensor_(this->outdoor_report_byte_5_raw_sensor_,
                    fields.compressor_frequency_raw);
    publish_sensor_(this->outdoor_report_byte_6_raw_sensor_, fields.byte_6_raw);
    publish_sensor_(this->outdoor_report_byte_9_raw_sensor_, fields.byte_9_raw);
    publish_sensor_(this->outdoor_report_byte_10_raw_sensor_, fields.byte_10_raw);
    publish_sensor_(this->outdoor_report_byte_13_temperature_hypothesis_sensor_,
                    fields.ambient_temperature_candidate_c);
    publish_sensor_(this->outdoor_report_byte_14_temperature_hypothesis_sensor_,
                    fields.coil_temperature_candidate_c);
    publish_sensor_(this->outdoor_report_byte_15_temperature_hypothesis_sensor_,
                    fields.compressor_discharge_temperature_candidate_c);
    publish_binary_sensor_(this->outdoor_report_byte_17_bit_2_sensor_,
                           fields.byte_17_bit_2);
    publish_text_sensor_(this->outdoor_report_bytes_21_28_raw_sensor_,
                         format_bytes_(fields.bytes_21_28_raw));
    publish_sensor_(this->outdoor_report_byte_30_raw_sensor_, fields.byte_30_raw);
    publish_binary_sensor_(this->outdoor_report_byte_31_bit_6_sensor_,
                           fields.byte_31_bit_6);
    publish_sensor_(this->outdoor_report_byte_36_raw_sensor_, fields.byte_36_raw);
  }

  void publish_energy_flow_report_() {
    const auto *payload = this->climate_->get_retained_payload(0x53);
    if (payload == nullptr || *payload == this->last_energy_flow_decoded_payload_) return;
    EnergyFlowReportFields fields;
    if (!decode_energy_flow_report(*payload, fields)) return;
    this->last_energy_flow_decoded_payload_ = *payload;
    publish_sensor_(this->energy_flow_raw_sensor_, fields.energy_flow_raw);
  }

  void publish_power_discovery_() {
    if (this->power_discovery_summary_sensor_ == nullptr) return;

    const auto *sync = this->climate_->get_retained_payload(0x32);
    const auto *electrical = this->climate_->get_retained_payload(0x40);
    const auto capability =
        decode_electrical_energy_capability(sync, electrical);
    std::ostringstream out;
    out << "ElectricalPage=" << capability_state_(capability);

    const auto *status = this->climate_->get_retained_payload(0x31);
    StatusReportFields status_fields;
    if (status != nullptr && decode_status_report(*status, status_fields)) {
      out << "; 0x31 ElcErg=" << unsigned(status_fields.elc_erg_flag)
          << " ElcGear=" << unsigned(status_fields.elc_gear_raw)
          << " Elc1KwhRaw=" << unsigned(status_fields.elc_1kwh_raw)
          << " UDFanPort=" << unsigned(status_fields.indoor_fan_port_raw);
    }

    const auto *outdoor = this->climate_->get_retained_payload(0x35);
    OutdoorReportFields outdoor_fields;
    if (outdoor != nullptr && decode_outdoor_report(*outdoor, outdoor_fields)) {
      out << "; 0x35 CompressorFqy="
          << unsigned(outdoor_fields.compressor_frequency_raw)
          << "Hz OutdoorFanRaw=" << unsigned(outdoor_fields.outdoor_fan_speed_raw)
          << " ValveClosing=" << unsigned(outdoor_fields.expansion_valve_closing)
          << " EEV=" << unsigned(outdoor_fields.expansion_valve_position);
    }

    if (electrical != nullptr) {
      out << "; 0x40 len=" << electrical->size()
          << " raw=" << format_payload_(*electrical);
    } else {
      out << "; 0x40=no-response";
    }

    const std::string state = out.str();
    if (state == this->last_power_discovery_summary_) return;
    this->last_power_discovery_summary_ = state;
    this->power_discovery_summary_sensor_->publish_state(state);
  }

  sinclair_ac::SinclairAC *climate_{nullptr};
  sinclair_ac::TemperatureStabilizer status_indoor_temperature_stabilizer_;
  sinclair_ac::TemperatureStabilizer status_outdoor_temperature_stabilizer_;
  uint32_t last_status_generation_{0};

  text_sensor::TextSensor *last_0x32_payload_sensor_{nullptr};
  text_sensor::TextSensor *last_0x33_payload_sensor_{nullptr};
  text_sensor::TextSensor *last_0x34_payload_sensor_{nullptr};
  text_sensor::TextSensor *last_0x35_payload_sensor_{nullptr};
  text_sensor::TextSensor *last_0x36_payload_sensor_{nullptr};
  text_sensor::TextSensor *last_0x3c_payload_sensor_{nullptr};
  text_sensor::TextSensor *last_0x40_payload_sensor_{nullptr};
  text_sensor::TextSensor *last_0x41_payload_sensor_{nullptr};
  text_sensor::TextSensor *last_0x42_payload_sensor_{nullptr};
  text_sensor::TextSensor *last_0x53_payload_sensor_{nullptr};
  text_sensor::TextSensor *electrical_energy_capability_sensor_{nullptr};
  text_sensor::TextSensor *power_discovery_summary_sensor_{nullptr};
  text_sensor::TextSensor *outdoor_report_bytes_21_28_raw_sensor_{nullptr};

  sensor::Sensor *status_indoor_temperature_sensor_{nullptr};
  sensor::Sensor *status_outdoor_ambient_temperature_sensor_{nullptr};
  sensor::Sensor *status_humidity_sensor_field_raw_sensor_{nullptr};
  sensor::Sensor *status_indoor_fan_port_raw_sensor_{nullptr};
  sensor::Sensor *status_elc_gear_raw_sensor_{nullptr};
  sensor::Sensor *status_elc_1kwh_raw_sensor_{nullptr};
  sensor::Sensor *indoor_report_target_temperature_sensor_{nullptr};
  sensor::Sensor *indoor_report_current_temperature_sensor_{nullptr};
  sensor::Sensor *indoor_coil_temperature_candidate_sensor_{nullptr};
  sensor::Sensor *indoor_secondary_temperature_candidate_sensor_{nullptr};
  sensor::Sensor *indoor_report_byte_10_temperature_hypothesis_sensor_{nullptr};
  sensor::Sensor *indoor_report_byte_25_temperature_hypothesis_sensor_{nullptr};
  sensor::Sensor *indoor_report_byte_25_raw_sensor_{nullptr};
  sensor::Sensor *indoor_report_byte_6_raw_sensor_{nullptr};
  sensor::Sensor *indoor_report_df_point_raw_sensor_{nullptr};
  sensor::Sensor *indoor_report_cps_temperature_raw_sensor_{nullptr};
  sensor::Sensor *indoor_report_byte_20_raw_sensor_{nullptr};
  sensor::Sensor *indoor_report_byte_21_raw_sensor_{nullptr};
  sensor::Sensor *indoor_report_byte_36_raw_sensor_{nullptr};
  sensor::Sensor *indoor_report_byte_37_raw_sensor_{nullptr};
  sensor::Sensor *indoor_report_byte_38_raw_sensor_{nullptr};

  sensor::Sensor *outdoor_operating_value_raw_sensor_{nullptr};
  sensor::Sensor *outdoor_ambient_temperature_candidate_sensor_{nullptr};
  sensor::Sensor *outdoor_coil_temperature_candidate_sensor_{nullptr};
  sensor::Sensor *compressor_discharge_temperature_candidate_sensor_{nullptr};
  sensor::Sensor *compressor_frequency_sensor_{nullptr};
  sensor::Sensor *outdoor_fan_speed_raw_sensor_{nullptr};
  sensor::Sensor *expansion_valve_position_sensor_{nullptr};
  sensor::Sensor *outdoor_operating_state_raw_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_4_raw_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_5_raw_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_6_raw_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_9_raw_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_10_raw_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_13_temperature_hypothesis_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_14_temperature_hypothesis_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_15_temperature_hypothesis_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_30_raw_sensor_{nullptr};
  sensor::Sensor *outdoor_report_byte_36_raw_sensor_{nullptr};
  sensor::Sensor *energy_flow_raw_sensor_{nullptr};

  binary_sensor::BinarySensor *status_elc_erg_sensor_{nullptr};
  binary_sensor::BinarySensor *expansion_valve_closing_sensor_{nullptr};
  binary_sensor::BinarySensor *indoor_report_byte_6_bit_3_sensor_{nullptr};
  binary_sensor::BinarySensor *indoor_report_byte_6_bit_5_sensor_{nullptr};
  binary_sensor::BinarySensor *indoor_report_byte_14_bit_5_sensor_{nullptr};
  binary_sensor::BinarySensor *outdoor_report_byte_4_bit_4_sensor_{nullptr};
  binary_sensor::BinarySensor *outdoor_report_byte_17_bit_2_sensor_{nullptr};
  binary_sensor::BinarySensor *outdoor_report_byte_31_bit_6_sensor_{nullptr};

  std::vector<uint8_t> last_0x32_payload_;
  std::vector<uint8_t> last_0x33_payload_;
  std::vector<uint8_t> last_0x34_payload_;
  std::vector<uint8_t> last_0x35_payload_;
  std::vector<uint8_t> last_0x36_payload_;
  std::vector<uint8_t> last_0x3c_payload_;
  std::vector<uint8_t> last_0x40_payload_;
  std::vector<uint8_t> last_0x41_payload_;
  std::vector<uint8_t> last_0x42_payload_;
  std::vector<uint8_t> last_0x53_payload_;
  std::vector<uint8_t> last_status_decoded_payload_;
  std::vector<uint8_t> last_indoor_decoded_payload_;
  std::vector<uint8_t> last_outdoor_decoded_payload_;
  std::vector<uint8_t> last_energy_flow_decoded_payload_;
  ElectricalEnergyCapability last_capability_{
      ElectricalEnergyCapability::NO_SYNCHRONIZATION};
  bool has_published_capability_{false};
  std::string last_power_discovery_summary_;
};

}  // namespace gree_oem_report_sensors
}  // namespace esphome
