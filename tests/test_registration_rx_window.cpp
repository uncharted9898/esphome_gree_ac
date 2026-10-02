#include <cassert>
#include <cstdint>
#include <iostream>

#include "../components/gree_wired_rs485/registration_rx_window.h"

using esphome::gree_wired_rs485::diagnostics::RegistrationRxWindow;

int main() {
  RegistrationRxWindow window;

  window.open(1000, 1100, 1250, 2, 7);
  assert(window.active());
  assert(window.pending_at_probe() == 2);
  assert(window.de_released_at_us() == 1100);
  assert(window.de_release_delay_us() == 100);
  assert(window.pending_probe_delay_us() == 250);
  assert(window.release_to_probe_delay_us() == 150);
  assert(window.bytes_observed() == 0);
  assert(window.first_drain_us() == 0);
  assert(window.drain_span_us() == 0);
  assert(window.valid_frame_delta(7) == 0);

  window.observe_byte(1500);
  window.observe_byte(2400);
  assert(window.bytes_observed() == 2);
  assert(window.first_drain_us() == 500);
  assert(window.last_drain_us() == 1400);
  assert(window.drain_span_us() == 900);
  assert(window.drain_offsets_us().size() == 2);
  assert(window.drain_offsets_us()[0] == 500);
  assert(window.drain_offsets_us()[1] == 1400);
  assert(window.valid_frame_delta(9) == 2);

  window.close();
  assert(!window.active());
  window.observe_byte(3000);
  assert(window.bytes_observed() == 2);

  // uint32_t subtraction intentionally provides wrap-safe microsecond deltas.
  window.open(0xFFFFFF00u, 0xFFFFFF40u, 0xFFFFFF80u, 0, 11);
  window.observe_byte(0x00000020u);
  window.observe_byte(0x00000120u);
  assert(window.de_released_at_us() == 0xFFFFFF40u);
  assert(window.de_release_delay_us() == 0x40u);
  assert(window.pending_probe_delay_us() == 0x80u);
  assert(window.release_to_probe_delay_us() == 0x40u);
  assert(window.first_drain_us() == 0x120u);
  assert(window.last_drain_us() == 0x220u);
  assert(window.drain_span_us() == 0x100u);
  assert(window.valid_frame_delta(12) == 1);

  // Reopening a window must discard prior attempt timing.
  window.open(5000, 5002, 5005, 1, 20);
  assert(window.active());
  assert(window.pending_at_probe() == 1);
  assert(window.de_release_delay_us() == 2);
  assert(window.pending_probe_delay_us() == 5);
  assert(window.release_to_probe_delay_us() == 3);
  assert(window.bytes_observed() == 0);
  assert(window.drain_offsets_us().empty());
  assert(window.valid_frame_delta(20) == 0);

  for (size_t i = 0; i < RegistrationRxWindow::MAX_RECORDED_OFFSETS + 10; ++i) {
    window.observe_byte(static_cast<uint32_t>(6000 + i));
  }
  assert(window.bytes_observed() ==
         RegistrationRxWindow::MAX_RECORDED_OFFSETS + 10);
  assert(window.drain_offsets_us().size() ==
         RegistrationRxWindow::MAX_RECORDED_OFFSETS);

  std::cout << "registration RX window tests passed\n";
  return 0;
}
