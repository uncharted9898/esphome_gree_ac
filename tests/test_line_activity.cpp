#include <cassert>
#include <cmath>
#include <iostream>

#include "../components/gree_wired_rs485/line_activity.h"

using esphome::gree_wired_rs485::diagnostics::LineActivityTracker;

int main() {
  LineActivityTracker tracker;

  tracker.observe(-1);
  const auto empty = tracker.take_window();
  assert(empty.samples == 0);
  assert(empty.high_samples == 0);
  assert(empty.transitions == 0);
  assert(empty.high_percent() == 0.0f);
  assert(!tracker.has_last_level());

  tracker.observe(1);
  tracker.observe(1);
  tracker.observe(0);
  tracker.observe(0);
  tracker.observe(1);

  const auto first = tracker.take_window();
  assert(first.samples == 5);
  assert(first.high_samples == 3);
  assert(first.transitions == 2);
  assert(std::fabs(first.high_percent() - 60.0f) < 0.001f);
  assert(tracker.total_transitions() == 2);
  assert(tracker.has_last_level());
  assert(tracker.last_high());

  // The last level carries across windows so a transition at the start of
  // the next health interval is not lost.
  tracker.observe(1);
  tracker.observe(0);

  const auto second = tracker.take_window();
  assert(second.samples == 2);
  assert(second.high_samples == 1);
  assert(second.transitions == 1);
  assert(std::fabs(second.high_percent() - 50.0f) < 0.001f);
  assert(tracker.total_transitions() == 3);
  assert(!tracker.last_high());

  std::cout << "line activity tracker tests passed\n";
  return 0;
}
