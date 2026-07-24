#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome {
namespace gree_oem_report_sensors {

constexpr float decode_offset_40_temperature(uint8_t raw) {
  return static_cast<float>(static_cast<int>(raw) - 40);
}

struct IndoorReportFields {
  float target_temperature_c{0.0f};
  float current_temperature_c{0.0f};
  float coil_temperature_candidate_c{0.0f};
  float secondary_temperature_candidate_c{0.0f};

  bool byte_14_bit_5{false};
  uint8_t byte_20_raw{0};
  uint8_t byte_21_raw{0};
  uint8_t byte_36_raw{0};
  uint8_t byte_37_raw{0};
  uint8_t byte_38_raw{0};
};

struct OutdoorReportFields {
  uint8_t operating_value_raw{0};
  float ambient_temperature_candidate_c{0.0f};
  float coil_temperature_candidate_c{0.0f};
  float compressor_discharge_temperature_candidate_c{0.0f};

  bool byte_17_bit_2{false};
  std::array<uint8_t, 8> bytes_21_28_raw{};
  uint8_t byte_30_raw{0};
  bool byte_31_bit_6{false};
  uint8_t byte_36_raw{0};
};

inline bool decode_indoor_report(const std::vector<uint8_t> &payload, IndoorReportFields &out) {
  // Highest decoded/observed field is payload byte 38.
  if (payload.size() <= 38) return false;

  out.target_temperature_c = decode_offset_40_temperature(payload[7]);
  out.current_temperature_c = decode_offset_40_temperature(payload[8]);
  out.coil_temperature_candidate_c = decode_offset_40_temperature(payload[10]);
  out.secondary_temperature_candidate_c = decode_offset_40_temperature(payload[25]);

  out.byte_14_bit_5 = (payload[14] & 0x20U) != 0;
  out.byte_20_raw = payload[20];
  out.byte_21_raw = payload[21];
  out.byte_36_raw = payload[36];
  out.byte_37_raw = payload[37];
  out.byte_38_raw = payload[38];
  return true;
}

inline bool decode_outdoor_report(const std::vector<uint8_t> &payload, OutdoorReportFields &out) {
  // Highest decoded/observed field is payload byte 36.
  if (payload.size() <= 36) return false;

  out.operating_value_raw = payload[10];
  out.ambient_temperature_candidate_c = decode_offset_40_temperature(payload[13]);
  out.coil_temperature_candidate_c = decode_offset_40_temperature(payload[14]);
  out.compressor_discharge_temperature_candidate_c = decode_offset_40_temperature(payload[15]);

  out.byte_17_bit_2 = (payload[17] & 0x04U) != 0;
  for (size_t i = 0; i < out.bytes_21_28_raw.size(); ++i) {
    out.bytes_21_28_raw[i] = payload[21 + i];
  }
  out.byte_30_raw = payload[30];
  out.byte_31_bit_6 = (payload[31] & 0x40U) != 0;
  out.byte_36_raw = payload[36];
  return true;
}

}  // namespace gree_oem_report_sensors
}  // namespace esphome
