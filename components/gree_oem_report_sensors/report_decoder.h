#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace esphome {
namespace gree_oem_report_sensors {

constexpr float decode_offset_40_temperature(uint8_t raw) {
  return static_cast<float>(static_cast<int>(raw) - 40);
}

enum class ElectricalEnergyCapability : uint8_t {
  NO_SYNCHRONIZATION,
  PAGE_NOT_ADVERTISED,
  PAGE_ADVERTISED_NO_REPORT,
  MALFORMED_REPORT,
  REPORT_UNSUPPORTED,
  REPORT_SUPPORTED_ELC_EN_UNKNOWN,
  ELC_EN_DISABLED,
  ELC_EN_ENABLED,
};

enum class EnergyFlowCapability : uint8_t {
  NO_SYNCHRONIZATION,
  CAPABILITY_BYTE_MISSING,
  NOT_ADVERTISED,
  ADVERTISED,
};

// The audited RTL8720CF receive dispatcher separates two signals:
//   * 0x32 payload[0] bit 0 advertises whether the optional 0x40 page exists.
//   * After a supported 0x40 arrives (payload[44] bit 0), 0x32 payload[1]
//     bit 0 is copied into the ElcEn property.
// No appliance-UART command writes or enables ElcEn.
inline ElectricalEnergyCapability decode_electrical_energy_capability(
    const std::vector<uint8_t> *synchronization,
    const std::vector<uint8_t> *electrical) {
  if (synchronization == nullptr || synchronization->empty()) {
    return ElectricalEnergyCapability::NO_SYNCHRONIZATION;
  }
  if (((*synchronization)[0] & 0x01U) == 0) {
    return ElectricalEnergyCapability::PAGE_NOT_ADVERTISED;
  }
  if (electrical == nullptr) {
    return ElectricalEnergyCapability::PAGE_ADVERTISED_NO_REPORT;
  }
  if (electrical->size() <= 44) {
    return ElectricalEnergyCapability::MALFORMED_REPORT;
  }
  if (((*electrical)[44] & 0x01U) == 0) {
    return ElectricalEnergyCapability::REPORT_UNSUPPORTED;
  }
  if (synchronization->size() <= 1) {
    return ElectricalEnergyCapability::REPORT_SUPPORTED_ELC_EN_UNKNOWN;
  }
  return ((*synchronization)[1] & 0x01U) != 0
             ? ElectricalEnergyCapability::ELC_EN_ENABLED
             : ElectricalEnergyCapability::ELC_EN_DISABLED;
}

// The independent command-0x09 EnergyFlow poll is not part of the command-0x03
// selector family. Both audited RTL schedulers advertise it through command-0x32
// payload[1] bit 1, then expect command 0x53.
inline EnergyFlowCapability decode_energy_flow_capability(
    const std::vector<uint8_t> *synchronization) {
  if (synchronization == nullptr || synchronization->empty()) {
    return EnergyFlowCapability::NO_SYNCHRONIZATION;
  }
  if (synchronization->size() <= 1) {
    return EnergyFlowCapability::CAPABILITY_BYTE_MISSING;
  }
  return ((*synchronization)[1] & 0x02U) != 0
             ? EnergyFlowCapability::ADVERTISED
             : EnergyFlowCapability::NOT_ADVERTISED;
}

struct StatusReportFields {
  uint8_t humidity_sensor_field_raw{0};
  uint8_t indoor_fan_port_raw{0};
  bool elc_all_kwh_clear_flag{false};
  bool elc_erg_flag{false};
  uint8_t elc_gear_raw{0};
  uint8_t elc_1kwh_raw{0};
  float indoor_temperature_c{0.0f};
  float outdoor_ambient_temperature_c{0.0f};
};

struct IndoorReportFields {
  float target_temperature_c{0.0f};
  float current_temperature_c{0.0f};
  float evaporator_temperature_c{0.0f};

  // Compatibility alias for the earlier configuration key.
  float coil_temperature_candidate_c{0.0f};

  // Neither RTL8720CF image maps byte 25 to HumSen or a named temperature.
  uint8_t byte_25_raw{0};
  float secondary_temperature_candidate_c{
      std::numeric_limits<float>::quiet_NaN()};

  uint8_t byte_6_raw{0};
  bool byte_6_bit_3{false};
  bool byte_6_bit_5{false};
  uint8_t df_point_raw{0};
  uint8_t cps_temperature_raw{0};

  bool byte_14_bit_5{false};
  uint8_t byte_20_raw{0};
  uint8_t byte_21_raw{0};
  uint8_t byte_36_raw{0};
  uint8_t byte_37_raw{0};
  uint8_t byte_38_raw{0};
};

struct OutdoorReportFields {
  uint8_t byte_4_raw{0};
  bool byte_4_bit_4{false};

  // V2 maps this byte directly to CompressorFqy. V3 preserves the same frame
  // layout even though the cloud-property assignment was removed.
  uint8_t compressor_frequency_raw{0};

  // These three fields are aligned from a separate, documented GREE outdoor-
  // controller operating page. The RTL Wi-Fi parser itself only assigns
  // payload[5], [13], and [15] to named properties, so fan and valve semantics
  // remain cross-protocol candidates until this exact Livo changes them live.
  uint8_t outdoor_fan_speed_raw{0};
  bool expansion_valve_closing{false};
  uint8_t expansion_valve_position{0};

  // Raw aliases retained for capture-oriented and legacy YAML keys.
  uint8_t byte_6_raw{0};
  uint8_t byte_9_raw{0};
  uint8_t byte_10_raw{0};
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

struct EnergyFlowReportFields {
  uint8_t energy_flow_raw{0};
};

inline bool decode_status_report(const std::vector<uint8_t> &payload,
                                  StatusReportFields &out) {
  // Highest recovered field is Elc1Kwh at payload byte 45.
  if (payload.size() <= 45) return false;

  out.humidity_sensor_field_raw = payload[0];
  out.indoor_fan_port_raw = static_cast<uint8_t>((payload[23] >> 2) & 0x03U);
  // V2/V3 map the normalized status image payload[7] bit 3 to the named
  // ElcAllKwhClr property. This is a clear/request flag, not an energy value.
  out.elc_all_kwh_clear_flag = (payload[7] & 0x08U) != 0;
  out.elc_erg_flag = (payload[35] & 0x80U) != 0;
  out.elc_gear_raw = static_cast<uint8_t>((payload[35] >> 3) & 0x0FU);
  out.indoor_temperature_c = decode_offset_40_temperature(payload[42]);
  out.outdoor_ambient_temperature_c = decode_offset_40_temperature(payload[44]);
  out.elc_1kwh_raw = payload[45];
  return true;
}

inline bool decode_indoor_report(const std::vector<uint8_t> &payload,
                                  IndoorReportFields &out) {
  // Highest retained OEM change-detection field is payload byte 38.
  if (payload.size() <= 38) return false;

  out.byte_6_raw = payload[6];
  out.byte_6_bit_3 = (payload[6] & 0x08U) != 0;
  out.byte_6_bit_5 = (payload[6] & 0x20U) != 0;

  out.target_temperature_c = decode_offset_40_temperature(payload[7]);
  out.current_temperature_c =
      decode_offset_40_temperature(payload[8]) +
      (out.byte_6_bit_3 ? 0.5f : 0.0f);
  out.evaporator_temperature_c =
      decode_offset_40_temperature(payload[10]) +
      (out.byte_6_bit_5 ? 0.5f : 0.0f);
  out.coil_temperature_candidate_c = out.evaporator_temperature_c;
  out.byte_25_raw = payload[25];
  out.secondary_temperature_candidate_c =
      std::numeric_limits<float>::quiet_NaN();

  // Exact property bit extractions recovered from v1.21.
  out.df_point_raw = static_cast<uint8_t>(((payload[6] >> 4) & 0x0CU) |
                                           ((payload[6] >> 2) & 0x03U));
  out.cps_temperature_raw =
      static_cast<uint8_t>((payload[11] >> 1) & 0x0FU);

  out.byte_14_bit_5 = (payload[14] & 0x20U) != 0;
  out.byte_20_raw = payload[20];
  out.byte_21_raw = payload[21];
  out.byte_36_raw = payload[36];
  out.byte_37_raw = payload[37];
  out.byte_38_raw = payload[38];
  return true;
}

inline bool decode_outdoor_report(const std::vector<uint8_t> &payload,
                                   OutdoorReportFields &out) {
  // Highest retained OEM change-detection field is payload byte 36.
  if (payload.size() <= 36) return false;

  out.byte_4_raw = payload[4];
  out.byte_4_bit_4 = (payload[4] & 0x10U) != 0;
  out.compressor_frequency_raw = payload[5];
  out.outdoor_fan_speed_raw = payload[6];
  out.expansion_valve_closing = payload[9] != 0;
  out.expansion_valve_position = payload[10];
  out.byte_6_raw = out.outdoor_fan_speed_raw;
  out.byte_9_raw = payload[9];
  out.byte_10_raw = out.expansion_valve_position;
  out.operating_value_raw = out.expansion_valve_position;

  out.ambient_temperature_candidate_c =
      decode_offset_40_temperature(payload[13]);
  out.coil_temperature_candidate_c =
      decode_offset_40_temperature(payload[14]);
  out.compressor_discharge_temperature_candidate_c =
      decode_offset_40_temperature(payload[15]);

  out.byte_17_bit_2 = (payload[17] & 0x04U) != 0;
  for (size_t i = 0; i < out.bytes_21_28_raw.size(); ++i) {
    out.bytes_21_28_raw[i] = payload[21 + i];
  }
  out.byte_30_raw = payload[30];
  out.byte_31_bit_6 = (payload[31] & 0x40U) != 0;
  out.byte_36_raw = payload[36];
  return true;
}

inline bool decode_energy_flow_report(const std::vector<uint8_t> &payload,
                                       EnergyFlowReportFields &out) {
  // Response 0x53 copies full-frame byte 0x1C (payload[24]) to the named
  // EnergyFlow property. Its physical meaning and scale are not recovered; the
  // property sits beside air-quality/ventilation fields, not in the Elc* block.
  if (payload.size() <= 24) return false;
  out.energy_flow_raw = payload[24];
  return true;
}

}  // namespace gree_oem_report_sensors
}  // namespace esphome
