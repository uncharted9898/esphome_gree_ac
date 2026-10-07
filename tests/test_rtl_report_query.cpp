#include "../components/gree_oem_boot_probe/rtl_query.h"

#include <cassert>
#include <cstddef>

using esphome::gree_oem_probe::build_rtl_energy_flow_query;
using esphome::gree_oem_probe::build_rtl_report_query;
using esphome::gree_oem_probe::build_rtl_startup_sync;
using esphome::gree_oem_probe::RtlDateTimeContext;
using esphome::gree_oem_probe::should_query_rtl_energy_flow;
using esphome::gree_oem_probe::should_query_rtl_electrical_page;
using esphome::gree_oem_probe::valid_rtl_energy_flow_query;
using esphome::gree_oem_probe::valid_rtl_report_query;

int main() {
  const auto combined = build_rtl_report_query(0x01, 0x00);
  const auto indoor = build_rtl_report_query(0x02, 0x00);
  const auto outdoor = build_rtl_report_query(0x04, 0x00);
  const auto status = build_rtl_report_query(0x00, 0x00);
  const auto page_42 = build_rtl_report_query(0x00, 0x01);
  const auto page_41 = build_rtl_report_query(0x00, 0x02);
  const auto electrical = build_rtl_report_query(0x00, 0x04);

  for (const auto *frame : {&combined, &indoor, &outdoor, &status, &page_42, &page_41,
                            &electrical}) {
    assert(frame->size() == 29);
    assert((*frame)[2] == 0x1A);
    assert((*frame)[3] == 0x03);
    assert((*frame)[27] == 0x00);
    assert(valid_rtl_report_query(*frame));
    for (size_t index = 8; index <= 13; ++index) {
      assert((*frame)[index] == 0x00);
    }
  }

  assert(combined[4] == 0x01 && combined[14] == 0x00 && combined[28] == 0x1F);
  assert(indoor[4] == 0x02 && indoor[28] == 0x20);
  assert(outdoor[4] == 0x04 && outdoor[28] == 0x22);
  assert(status[4] == 0x00 && status[14] == 0x00 && status[28] == 0x1E);
  assert(page_42[14] == 0x01 && page_42[28] == 0x1F);
  assert(page_41[14] == 0x02 && page_41[28] == 0x20);
  assert(electrical[14] == 0x04 && electrical[28] == 0x22);

  // A repaired checksum is not sufficient. The old experiment placed an
  // isolated 0x3B in the seconds slot while the corresponding context-enable
  // bit was clear; the audited firmware never emits that shape.
  auto malformed_context = status;
  malformed_context[10] = 0x3B;
  malformed_context[28] =
      esphome::gree_oem_probe::rtl_additive_checksum(malformed_context);
  assert(!valid_rtl_report_query(malformed_context));

  auto enabled_context = status;
  enabled_context[4] = 0x80;
  enabled_context[8] = 0x17;
  enabled_context[9] = 0x3B;
  enabled_context[10] = 0x3B;
  enabled_context[28] =
      esphome::gree_oem_probe::rtl_additive_checksum(enabled_context);
  assert(valid_rtl_report_query(enabled_context));

  // Both audited RTL versions send this complete command-0x03 envelope four
  // times after command 0x44. State 1 is assigned after the OEM cloud login succeeds; an
  // unsynchronized clock is all zero.
  const auto startup = build_rtl_startup_sync();
  assert(startup == status);
  assert(startup[15] == 0x00 && startup[16] == 0x00);
  assert(startup[17] == 0x00 && startup[18] == 0x00);
  assert(startup[19] == 0x00 && startup[20] == 0x00);

  constexpr RtlDateTimeContext date_time{
      2026, 7, 25, 16, 42, 59,
  };
  const auto dated_startup = build_rtl_startup_sync(0x01, date_time);
  assert(dated_startup[15] == 0x7E);
  assert(dated_startup[16] == 0xA7);
  assert(dated_startup[17] == 25);
  assert(dated_startup[18] == 16);
  assert(dated_startup[19] == 42);
  assert(dated_startup[20] == 59);
  assert(dated_startup[26] == 0x01);
  assert(valid_rtl_report_query(dated_startup));

  // This is an independent OEM command family, not another selector value.
  const auto energy_flow = build_rtl_energy_flow_query();
  assert(energy_flow.size() == 53);
  assert(energy_flow[0] == 0x7E && energy_flow[1] == 0x7E);
  assert(energy_flow[2] == 0x32);
  assert(energy_flow[3] == 0x09);
  for (size_t index = 4; index + 1 < energy_flow.size(); ++index) {
    assert(energy_flow[index] == 0x00);
  }
  assert(energy_flow.back() == 0x3B);
  assert(valid_rtl_energy_flow_query(energy_flow));

  auto invalid_energy_flow = energy_flow;
  invalid_energy_flow[10] = 0x01;
  invalid_energy_flow.back() = static_cast<uint8_t>(invalid_energy_flow.back() + 1U);
  assert(!valid_rtl_energy_flow_query(invalid_energy_flow));
  invalid_energy_flow = energy_flow;
  invalid_energy_flow.back() ^= 0x01;
  assert(!valid_rtl_energy_flow_query(invalid_energy_flow));

  // Production policy: advertised or previously proven support may refresh;
  // forcing an unadvertised page is restricted to one initial discovery try.
  assert(!should_query_rtl_energy_flow(false, true, true, true, false, true));
  assert(should_query_rtl_energy_flow(true, true, false, false, true, false));
  assert(should_query_rtl_energy_flow(true, false, true, false, true, false));
  assert(should_query_rtl_energy_flow(true, false, false, true, false, true));
  assert(!should_query_rtl_energy_flow(true, false, false, true, true, true));
  assert(!should_query_rtl_energy_flow(true, false, false, true, false, false));
  assert(!should_query_rtl_energy_flow(true, false, false, false, false, true));

  // 0x40 may refresh when advertised/proven. With no 0x32 at all, preserve the
  // legacy one-shot discovery behavior. A known clear capability requires an
  // explicit one-shot force in the full discovery cycle.
  assert(should_query_rtl_electrical_page(false, false, false, false, false, true));
  assert(!should_query_rtl_electrical_page(false, false, false, false, true, true));
  assert(!should_query_rtl_electrical_page(true, false, false, false, false, true));
  assert(should_query_rtl_electrical_page(true, false, false, true, false, true));
  assert(!should_query_rtl_electrical_page(true, false, false, true, false, false));
  assert(should_query_rtl_electrical_page(true, true, false, false, true, false));
  assert(should_query_rtl_electrical_page(true, false, true, false, true, false));
  return 0;
}
