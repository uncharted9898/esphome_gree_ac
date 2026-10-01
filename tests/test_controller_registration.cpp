#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "../components/gree_wired_rs485/controller_registration.h"

using esphome::gree_wired_rs485::registration::ACCEPT_COUNTERS;
using esphome::gree_wired_rs485::registration::COUNTER_INDEX;
using esphome::gree_wired_rs485::registration::make_frame;

static uint8_t xor_all(const std::vector<uint8_t> &frame) {
  uint8_t value = 0;
  for (const auto byte : frame) value ^= byte;
  return value;
}

int main() {
  for (size_t attempt = 0; attempt < ACCEPT_COUNTERS.size(); ++attempt) {
    const auto frame = make_frame(attempt);
    assert(frame.size() == 40);
    assert(frame[0] == 0x7E && frame[1] == 0x7E);
    assert(frame[2] == 0xFF && frame[3] == 0x00);
    assert(frame[4] == 0x11);
    assert(frame[5] == 0x22);
    assert(frame.size() == 6 + frame[5]);
    assert(frame[COUNTER_INDEX] == ACCEPT_COUNTERS[attempt]);
    assert(xor_all(frame) == 0);
  }

  const auto first = make_frame(0);
  assert(first[COUNTER_INDEX] == 0x21);
  assert(first.back() == 0x58);

  const auto wrapped = make_frame(ACCEPT_COUNTERS.size());
  assert(wrapped[COUNTER_INDEX] == 0x21);
  assert(xor_all(wrapped) == 0);

  std::cout << "controller registration tests passed\n";
  return 0;
}
