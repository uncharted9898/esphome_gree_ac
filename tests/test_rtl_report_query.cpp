#include "../components/gree_oem_boot_probe/rtl_query.h"

#include <cassert>

using esphome::gree_oem_probe::build_rtl_report_query;
using esphome::gree_oem_probe::valid_rtl_report_query;

int main() {
  const auto combined = build_rtl_report_query(0x01, 0x00);
  const auto indoor = build_rtl_report_query(0x02, 0x00);
  const auto outdoor = build_rtl_report_query(0x04, 0x00);
  const auto status = build_rtl_report_query(0x00, 0x00);
  const auto page_42 = build_rtl_report_query(0x00, 0x01);
  const auto page_41 = build_rtl_report_query(0x00, 0x02);
  const auto electrical = build_rtl_report_query(0x00, 0x04);

  for (const auto *frame : {&combined, &indoor, &outdoor, &status, &page_42, &page_41, &electrical}) {
    assert(frame->size() == 29);
    assert((*frame)[2] == 0x1A);
    assert((*frame)[3] == 0x03);
    assert((*frame)[27] == 0x00);
    assert(valid_rtl_report_query(*frame));
  }

  assert(combined[4] == 0x01 && combined[14] == 0x00 && combined[28] == 0x95);
  assert(indoor[4] == 0x02 && indoor[28] == 0x96);
  assert(outdoor[4] == 0x04 && outdoor[28] == 0x98);
  assert(status[4] == 0x00 && status[14] == 0x00 && status[28] == 0x94);
  assert(page_42[14] == 0x01 && page_42[28] == 0x95);
  assert(page_41[14] == 0x02 && page_41[28] == 0x96);
  assert(electrical[14] == 0x04 && electrical[28] == 0x98);
  return 0;
}
