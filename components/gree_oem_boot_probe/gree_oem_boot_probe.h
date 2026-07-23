#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/sinclair_ac/esppac.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gree_oem_probe {

class GreeOemBootProbe : public Component {
 public:
  void set_uart(uart::UARTComponent *uart) { this->uart_ = uart; }
  void set_climate(sinclair_ac::SinclairAC *climate) { this->climate_ = climate; }
  void set_restore_control(bool restore_control) { this->restore_control_ = restore_control; }
  void set_start_delay(uint32_t start_delay_ms) { this->start_delay_ms_ = start_delay_ms; }
  void set_frame_spacing(uint32_t frame_spacing_ms) { this->frame_spacing_ms_ = frame_spacing_ms; }

  float get_setup_priority() const override { return setup_priority::LATE; }

  void setup() override {
    if (this->uart_ == nullptr || this->climate_ == nullptr) {
      ESP_LOGE(TAG, "UART and climate references are required");
      this->mark_failed();
      return;
    }

    this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::RECEIVE_ONLY);
    ESP_LOGI(TAG, "Starting captured CS532 OEM boot sequence; normal climate TX is locked");

    const uint32_t t0 = this->start_delay_ms_;
    this->set_timeout("gree_oem_boot_0", t0, [this]() { this->send_(BOOT_IDENTITY, "boot identity 0x10/0x02"); });
    this->set_timeout("gree_oem_boot_1", t0 + this->frame_spacing_ms_, [this]() { this->send_(MAC_REPORT, "module report 0x05/0x04 #1"); });
    this->set_timeout("gree_oem_boot_2", t0 + this->frame_spacing_ms_ * 2, [this]() { this->send_(MAC_REPORT, "module report 0x05/0x04 #2"); });
    this->set_timeout("gree_oem_boot_3", t0 + this->frame_spacing_ms_ * 3, [this]() { this->send_(LINK_SYNC, "link sync 0x0E/0x03 #1"); });
    this->set_timeout("gree_oem_boot_4", t0 + this->frame_spacing_ms_ * 4, [this]() { this->send_(LINK_SYNC, "link sync 0x0E/0x03 #2"); });
    this->set_timeout("gree_oem_boot_5", t0 + this->frame_spacing_ms_ * 5, [this]() { this->send_(LINK_SYNC, "link sync 0x0E/0x03 #3"); });
    this->set_timeout("gree_oem_boot_6", t0 + this->frame_spacing_ms_ * 6, [this]() { this->send_(LINK_SYNC, "link sync 0x0E/0x03 #4"); });
    this->set_timeout("gree_oem_boot_finish", t0 + this->frame_spacing_ms_ * 7 + 350, [this]() {
      if (this->restore_control_) {
        this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::CONTROL);
        ESP_LOGI(TAG, "Captured OEM boot sequence complete; normal climate control enabled");
      } else {
        ESP_LOGI(TAG, "Captured OEM boot sequence complete; climate remains receive-only");
      }
      this->finished_ = true;
    });
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Gree OEM boot probe:");
    ESP_LOGCONFIG(TAG, "  Start delay: %u ms", this->start_delay_ms_);
    ESP_LOGCONFIG(TAG, "  Frame spacing: %u ms", this->frame_spacing_ms_);
    ESP_LOGCONFIG(TAG, "  Restore control: %s", YESNO(this->restore_control_));
  }

 protected:
  template<size_t N> void send_(const std::array<uint8_t, N> &frame, const char *description) {
    uint8_t checksum = 0;
    for (size_t i = 2; i + 1 < N; ++i) checksum = static_cast<uint8_t>(checksum + frame[i]);
    if (frame[0] != 0x7E || frame[1] != 0x7E || static_cast<size_t>(frame[2]) + 3 != N || checksum != frame[N - 1]) {
      ESP_LOGE(TAG, "Refusing invalid captured frame: %s", description);
      return;
    }
    ESP_LOGI(TAG, "TX captured OEM request: %s", description);
    this->uart_->write_array(frame.data(), frame.size());
    this->uart_->flush();
  }

  static constexpr const char *TAG = "gree.oem_boot_probe";

  // Captured from the public CS532-family boot exchange. These are complete
  // checksum-valid UART frames, not synthesized requests.
  static constexpr std::array<uint8_t, 19> BOOT_IDENTITY{
      0x7E, 0x7E, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x28, 0x1E, 0x19, 0x23, 0x23, 0x00, 0xB8};

  static constexpr std::array<uint8_t, 8> MAC_REPORT{
      0x7E, 0x7E, 0x05, 0x04, 0x07, 0x00, 0x00, 0x10};

  // The 0x7E immediately before 0x0F is payload data, not a sync marker.
  static constexpr std::array<uint8_t, 17> LINK_SYNC{
      0x7E, 0x7E, 0x0E, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x7E, 0x0F};

  uart::UARTComponent *uart_{nullptr};
  sinclair_ac::SinclairAC *climate_{nullptr};
  uint32_t start_delay_ms_{100};
  uint32_t frame_spacing_ms_{450};
  bool restore_control_{true};
  bool finished_{false};
};

}  // namespace gree_oem_probe
}  // namespace esphome
