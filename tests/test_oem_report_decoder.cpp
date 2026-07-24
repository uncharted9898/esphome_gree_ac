#include "../components/gree_oem_report_sensors/report_decoder.h"

#include <cassert>
#include <cstdint>
#include <vector>

using esphome::gree_oem_report_sensors::IndoorReportFields;
using esphome::gree_oem_report_sensors::OutdoorReportFields;
using esphome::gree_oem_report_sensors::decode_indoor_report;
using esphome::gree_oem_report_sensors::decode_outdoor_report;

int main() {
  const std::vector<uint8_t> indoor{
      0x04, 0x00, 0x40, 0x00, 0x11, 0x01, 0x20, 0x41, 0x40, 0x00, 0x3C, 0x01,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  IndoorReportFields indoor_fields;
  assert(decode_indoor_report(indoor, indoor_fields));
  assert(indoor_fields.target_temperature_c == 25.0f);
  assert(indoor_fields.current_temperature_c == 24.0f);
  assert(indoor_fields.coil_temperature_candidate_c == 20.0f);
  assert(indoor_fields.secondary_temperature_candidate_c == 28.0f);
  assert(!indoor_fields.byte_14_bit_5);
  assert(indoor_fields.byte_20_raw == 0x00);
  assert(indoor_fields.byte_21_raw == 0x00);
  assert(indoor_fields.byte_36_raw == 0x00);
  assert(indoor_fields.byte_37_raw == 0x00);
  assert(indoor_fields.byte_38_raw == 0x00);

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
      0x04, 0x00, 0x40, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC8, 0x00,
      0x00, 0x40, 0x46, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  OutdoorReportFields outdoor_fields;
  assert(decode_outdoor_report(outdoor, outdoor_fields));
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
  for (size_t i = 0; i < 8; ++i) outdoor_fault_variant[21 + i] = static_cast<uint8_t>(i + 1);
  outdoor_fault_variant[30] = 0xAA;
  outdoor_fault_variant[31] = 0x40;
  outdoor_fault_variant[36] = 0x55;
  assert(decode_outdoor_report(outdoor_fault_variant, outdoor_fields));
  assert(outdoor_fields.byte_17_bit_2);
  for (size_t i = 0; i < 8; ++i) assert(outdoor_fields.bytes_21_28_raw[i] == i + 1);
  assert(outdoor_fields.byte_30_raw == 0xAA);
  assert(outdoor_fields.byte_31_bit_6);
  assert(outdoor_fields.byte_36_raw == 0x55);

  assert(!decode_indoor_report(std::vector<uint8_t>(38, 0), indoor_fields));
  assert(!decode_outdoor_report(std::vector<uint8_t>(36, 0), outdoor_fields));
  return 0;
}
