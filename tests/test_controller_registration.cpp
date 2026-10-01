#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "../components/gree_wired_rs485/controller_registration.h"

using esphome::gree_wired_rs485::registration::ACCEPT_COUNTERS;
using esphome::gree_wired_rs485::registration::COUNTER_INDEX;
using esphome::gree_wired_rs485::registration::REFERENCE_UNIT_SIGNATURE;
using esphome::gree_wired_rs485::registration::UNIT_SIGNATURE_INDEX;
using esphome::gree_wired_rs485::registration::UnitSignature;
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
    assert(frame.size() == static_cast<size_t>(6U + frame[5]));
    for (size_t i = 0; i < REFERENCE_UNIT_SIGNATURE.size(); ++i) {
      assert(frame[UNIT_SIGNATURE_INDEX + i] == REFERENCE_UNIT_SIGNATURE[i]);
    }
    assert(frame[COUNTER_INDEX] == ACCEPT_COUNTERS[attempt]);
    assert(xor_all(frame) == 0);
  }

  const auto first = make_frame(0);
  assert(first[COUNTER_INDEX] == 0x21);
  assert(first.back() == 0x58);

  const auto wrapped = make_frame(ACCEPT_COUNTERS.size());
  assert(wrapped[COUNTER_INDEX] == 0x21);
  assert(xor_all(wrapped) == 0);

  const UnitSignature learned_signature = {0x5A, 0x30, 0x83};
  const auto learned = make_frame(0, learned_signature);
  assert(learned[UNIT_SIGNATURE_INDEX] == 0x5A);
  assert(learned[UNIT_SIGNATURE_INDEX + 1] == 0x30);
  assert(learned[UNIT_SIGNATURE_INDEX + 2] == 0x83);
  assert(learned[COUNTER_INDEX] == 0x21);
  assert(xor_all(learned) == 0);
  assert(learned.back() != first.back());

  using esphome::gree_wired_rs485::registration::silent_bootstrap_ready;
  assert(!silent_bootstrap_ready(false, true, false, false, 0, 5000, 5000, 0, 0));
  assert(!silent_bootstrap_ready(true, false, false, false, 0, 5000, 5000, 0, 0));
  assert(!silent_bootstrap_ready(true, true, false, false, 0, 4999, 5000, 0, 0));
  assert(silent_bootstrap_ready(true, true, false, false, 0, 5000, 5000, 0, 0));
  assert(!silent_bootstrap_ready(true, true, false, false, 0, 5000, 5000, 1, 0));
  assert(!silent_bootstrap_ready(true, true, false, false, 0, 5000, 5000, 0, 1));
  assert(!silent_bootstrap_ready(true, true, true, false, 0, 5000, 5000, 0, 0));
  assert(!silent_bootstrap_ready(true, true, false, true, 0, 5000, 5000, 0, 0));
  assert(!silent_bootstrap_ready(true, true, false, false, 1, 5000, 5000, 0, 0));

  std::cout << "controller registration tests passed\n";
  return 0;
}
