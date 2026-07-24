#include "../components/gree_oem_report_sensors/report_decoder.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

using esphome::gree_oem_report_sensors::ElectricalEnergyCapability;
using esphome::gree_oem_report_sensors::EnergyFlowReportFields;
using esphome::gree_oem_report_sensors::IndoorReportFields;
using esphome::gree_oem_report_sensors::OutdoorReportFields;
using esphome::gree_oem_report_sensors::StatusReportFields;
using esphome::gree_oem_report_sensors::decode_electrical_energy_capability;
using esphome::gree_oem_report_sensors::decode_energy_flow_report;
using esphome::gree_oem_report_sensors::decode_indoor_report;
using esphome::gree_oem_report_sensors::decode_outdoor_report;
using esphome::gree_oem_report_sensors::decode_status_report;

int main() {
  std::vector<uint8_t> synchronization_page_disabled{0x00};
  std::vector<uint8_t> synchronization_page_advertised{0x01};
  std::vector<uint8_t> synchronization_elcen_disabled{0x01, 0x00};
  std::vector<uint8_t> synchronization_elcen_enabled{0x01, 0x01};
  std::vector<uint8_t> electrical(45, 0);

  assert(decode_electrical_energy_capability(nullptr, nullptr) ==
         ElectricalEnergyCapability::NO_SYNCHRONIZATION);
  assert(decode_electrical_energy_capability(&synchronization_page_disabled,
                                              nullptr) ==
         ElectricalEnergyCapability::PAGE_NOT_ADVERTISED);
  assert(decode_electrical_energy_capability(&synchronization_page_advertised,
                                              nullptr) ==
         ElectricalEnergyCapability::PAGE_ADVERTISED_NO_REPORT);
  std::vector<uint8_t> electrical_short{0x00};
  assert(decode_electrical_energy_capability(&synchronization_page_advertised,
                                              &electrical_short) ==
         ElectricalEnergyCapability::MALFORMED_REPORT);
  assert(decode_electrical_energy_capability(&synchronization_page_advertised,
                                              &electrical) ==
         ElectricalEnergyCapability::REPORT_UNSUPPORTED);
  electrical[44] = 0x01;
  assert(decode_electrical_energy_capability(&synchronization_page_advertised,
                                              &electrical) ==
         ElectricalEnergyCapability::REPORT_SUPPORTED_ELC_EN_UNKNOWN);
  assert(decode_electrical_energy_capability(&synchronization_elcen_disabled,
                                              &electrical) ==
         ElectricalEnergyCapability::ELC_EN_DISABLED);
  assert(decode_electrical_energy_capability(&synchronization_elcen_enabled,
                                              &electrical) ==
         ElectricalEnergyCapability::ELC_EN_ENABLED);

  std::vector<uint8_t> status(47, 0);
  status[0] = 0x04;
  status[23] = 0x0C;
  status[35] = 0xA8;  // ElcErg=1, ElcGear=5.
  status[42] = 0x41;
  status[44] = 0x3F;
  status[45] = 0x17;
  StatusReportFields status_fields;
  assert(decode_status_report(status, status_fields));
  assert(status_fields.humidity_sensor_field_raw == 4);
  assert(status_fields.indoor_fan_port_raw == 3);
  assert(status_fields.elc_erg_flag);
  assert(status_fields.elc_gear_raw == 5);
  assert(status_fields.indoor_temperature_c == 25.0f);
  assert(status_fields.outdoor_ambient_temperature_c == 23.0f);
  assert(status_fields.elc_1kwh_raw == 0x17);

  // July 24 Livo capture: only the two offset-40 temperature bytes vary.
  const std::vector<uint8_t> live_status{
      0x04, 0x00, 0x40, 0x00, 0x91, 0x90, 0x06, 0xC2, 0x44, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x42, 0x00, 0x00,
  };
  assert(decode_status_report(live_status, status_fields));
  assert(status_fields.indoor_temperature_c == 23.0f);
  assert(status_fields.outdoor_ambient_temperature_c == 26.0f);
  assert(status_fields.humidity_sensor_field_raw == 4);
  assert(status_fields.elc_1kwh_raw == 0);
  assert(!decode_status_report(std::vector<uint8_t>(45, 0), status_fields));

  const std::vector<uint8_t> indoor{
      0x04, 0x00, 0x40, 0x00, 0x11, 0x01, 0x28, 0x41, 0x40, 0x00, 0x3C, 0x1E,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  IndoorReportFields indoor_fields;
  assert(decode_indoor_report(indoor, indoor_fields));
  assert(indoor_fields.target_temperature_c == 25.0f);
  assert(indoor_fields.current_temperature_c == 24.0f);
  assert(indoor_fields.evaporator_temperature_c == 20.0f);
  assert(indoor_fields.coil_temperature_candidate_c == 20.0f);
  assert(indoor_fields.byte_25_raw == 0x44);
  assert(std::isnan(indoor_fields.secondary_temperature_candidate_c));
  assert(indoor_fields.byte_6_raw == 0x28);
  assert(indoor_fields.byte_6_bit_3);
  assert(indoor_fields.byte_6_bit_5);
  assert(indoor_fields.df_point_raw == 2);
  assert(indoor_fields.cps_temperature_raw == 15);
  assert(!indoor_fields.byte_14_bit_5);

  auto indoor_fault_variant = indoor;
  indoor_fault_variant[14] = 0x20;
  indoor_fault_variant[20] = 0x12;
  indoor_fault_variant[21] = 0x34;
  indoor_fault_variant[36] = 0x56;
  indoor_fault_variant[37] = 0x78;
  indoor_fault_variant[38] = 0x9A;
  assert(decode_indoor_report(indoor_fault_variant, indoor_fields));
  assert(indoor_fields.byte_14_bit_5);
  assert(indoor_fields.byte_20_raw == 0x12);
  assert(indoor_fields.byte_21_raw == 0x34);
  assert(indoor_fields.byte_36_raw == 0x56);
  assert(indoor_fields.byte_37_raw == 0x78);
  assert(indoor_fields.byte_38_raw == 0x9A);

  const std::vector<uint8_t> outdoor{
      0x04, 0x00, 0x40, 0x00, 0x11, 0x37, 0x03, 0x00, 0x00, 0x01, 0xC8, 0x00,
      0x00, 0x40, 0x46, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  OutdoorReportFields outdoor_fields;
  assert(decode_outdoor_report(outdoor, outdoor_fields));
  assert(outdoor_fields.byte_4_raw == 0x11);
  assert(outdoor_fields.byte_4_bit_4);
  assert(outdoor_fields.compressor_frequency_raw == 55);
  assert(outdoor_fields.outdoor_fan_speed_raw == 3);
  assert(outdoor_fields.expansion_valve_closing);
  assert(outdoor_fields.expansion_valve_position == 200);
  assert(outdoor_fields.byte_6_raw == 3);
  assert(outdoor_fields.byte_9_raw == 1);
  assert(outdoor_fields.byte_10_raw == 200);
  assert(outdoor_fields.operating_value_raw == 200);
  assert(outdoor_fields.ambient_temperature_candidate_c == 24.0f);
  assert(outdoor_fields.coil_temperature_candidate_c == 30.0f);
  assert(outdoor_fields.compressor_discharge_temperature_candidate_c == 41.0f);
  assert(!outdoor_fields.byte_17_bit_2);
  for (const auto value : outdoor_fields.bytes_21_28_raw) assert(value == 0x00);
  assert(outdoor_fields.byte_30_raw == 0x00);
  assert(!outdoor_fields.byte_31_bit_6);
  assert(outdoor_fields.byte_36_raw == 0x00);

  auto outdoor_fault_variant = outdoor;
  outdoor_fault_variant[17] = 0x04;
  for (size_t i = 0; i < 8; ++i) {
    outdoor_fault_variant[21 + i] = static_cast<uint8_t>(i + 1);
  }
  outdoor_fault_variant[30] = 0xAA;
  outdoor_fault_variant[31] = 0x40;
  outdoor_fault_variant[36] = 0x55;
  assert(decode_outdoor_report(outdoor_fault_variant, outdoor_fields));
  assert(outdoor_fields.byte_17_bit_2);
  for (size_t i = 0; i < 8; ++i) {
    assert(outdoor_fields.bytes_21_28_raw[i] == i + 1);
  }
  assert(outdoor_fields.byte_30_raw == 0xAA);
  assert(outdoor_fields.byte_31_bit_6);
  assert(outdoor_fields.byte_36_raw == 0x55);

  std::vector<uint8_t> energy_flow(25, 0);
  energy_flow[24] = 0x09;
  EnergyFlowReportFields energy_flow_fields;
  assert(decode_energy_flow_report(energy_flow, energy_flow_fields));
  assert(energy_flow_fields.energy_flow_raw == 9);
  assert(!decode_energy_flow_report(std::vector<uint8_t>(24, 0),
                                    energy_flow_fields));

  assert(!decode_indoor_report(std::vector<uint8_t>(38, 0), indoor_fields));
  assert(!decode_outdoor_report(std::vector<uint8_t>(36, 0), outdoor_fields));
  return 0;
}
