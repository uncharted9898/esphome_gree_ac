#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome.h"
#include "esphome/components/sinclair_ac/esppac.h"

namespace esphome {
namespace gree_oem_probe {

// Replays only byte-for-byte requests captured from an original Gree CS532-series
// Wi-Fi module. The climate component must start in RECEIVE_ONLY mode so it cannot
// poll or control while this sequence owns the UART.
class GreeOemBootProbe : public Component {
 public:
  GreeOemBootProbe(uart::UARTComponent *uart, sinclair_ac::SinclairAC *climate)
      : uart_(uart), climate_(climate) {}

  float get_setup_priority() const override { return setup_priority::LATE; }

  void start() {
    if (started_) return;
    started_ = true;
    climate_->set_protocol_mode(sinclair_ac::ProtocolMode::RECEIVE_ONLY);
    ESP_LOGI(TAG, "Starting captured CS532 OEM boot sequence; climate TX locked");

    // The 450 ms spacing is intentionally conservative. Published captures show
    // each request followed by an AC response before the next request is sent.
    this->set_timeout("gree_oem_probe_0", 100, [this]() { this->send_(BOOT_IDENTITY, "boot identity 0x10/0x02"); });
    this->set_timeout("gree_oem_probe_1", 550, [this]() { this->send_(MAC_REPORT, "module report 0x05/0x04 #1"); });
    this->set_timeout("gree_oem_probe_2", 1000, [this]() { this->send_(MAC_REPORT, "module report 0x05/0x04 #2"); });
    this->set_timeout("gree_oem_probe_3", 1450, [this]() { this->send_(LINK_SYNC, "link sync 0x0E/0x03 #1"); });
    this->set_timeout("gree_oem_probe_4", 1900, [this]() { this->send_(LINK_SYNC, "link sync 0x0E/0x03 #2"); });
    this->set_timeout("gree_oem_probe_5", 2350, [this]() { this->send_(LINK_SYNC, "link sync 0x0E/0x03 #3"); });
    this->set_timeout("gree_oem_probe_6", 2800, [this]() { this->send_(LINK_SYNC, "link sync 0x0E/0x03 #4"); });
    this->set_timeout("gree_oem_probe_finish", 3600, [this]() {
      climate_->set_protocol_mode(sinclair_ac::ProtocolMode::CONTROL);
      ESP_LOGI(TAG, "Captured OEM boot sequence complete; normal climate control enabled");
      finished_ = true;
    });
  }

  bool finished() const { return finished_; }

 protected:
  template<size_t N> void send_(const std::array<uint8_t, N> &frame, const char *description) {
    uint8_t checksum = 0;
    for (size_t i = 2; i + 1 < N; ++i) checksum = static_cast<uint8_t>(checksum + frame[i]);
    if (frame[0] != 0x7E || frame[1] != 0x7E || static_cast<size_t>(frame[2]) + 3 != N || checksum != frame[N - 1]) {
      ESP_LOGE(TAG, "Refusing invalid captured frame: %s", description);
      return;
    }
    ESP_LOGI(TAG, "TX captured OEM request: %s", description);
    uart_->write_array(frame.data(), frame.size());
    uart_->flush();
  }

  static constexpr const char *TAG = "gree.oem_boot_probe";

  // 7E 7E 10 02 00 00 00 00 00 00 01 00 28 1E 19 23 23 00 B8
  static constexpr std::array<uint8_t, 19> BOOT_IDENTITY{
      0x7E, 0x7E, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x28, 0x1E, 0x19, 0x23, 0x23, 0x00, 0xB8};

  // 7E 7E 05 04 07 00 00 10
  static constexpr std::array<uint8_t, 8> MAC_REPORT{
      0x7E, 0x7E, 0x05, 0x04, 0x07, 0x00, 0x00, 0x10};

  // Captured connected-state 0x0E/0x03 request. The literal 0x7E before the
  // checksum is payload data, not a frame delimiter.
  // 7E 7E 0E 03 00 00 00 00 00 00 00 00 00 00 80 7E 0F
  static constexpr std::array<uint8_t, 17> LINK_SYNC{
      0x7E, 0x7E, 0x0E, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x7E, 0x0F};

  uart::UARTComponent *uart_;
  sinclair_ac::SinclairAC *climate_;
  bool started_{false};
  bool finished_{false};
};

}  // namespace gree_oem_probe
}  // namespace esphome
