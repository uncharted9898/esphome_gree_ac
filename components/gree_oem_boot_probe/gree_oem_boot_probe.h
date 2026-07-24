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
  void set_query_recovered_data(bool enabled) { this->query_recovered_data_ = enabled; }
  void set_repeat_interval(uint32_t interval_ms) { this->repeat_interval_ms_ = interval_ms; }
  void set_quiesce_delay(uint32_t delay_ms) { this->quiesce_delay_ms_ = delay_ms; }

  float get_setup_priority() const override { return setup_priority::LATE; }

  void setup() override {
    if (this->uart_ == nullptr || this->climate_ == nullptr) {
      ESP_LOGE(TAG, "UART and climate references are required");
      this->mark_failed();
      return;
    }
    this->begin_initial_sequence_();
  }

  void loop() override {
    if (this->repeat_interval_ms_ == 0 || this->sequence_active_) return;
    if (millis() - this->last_sequence_finished_at_ < this->repeat_interval_ms_) return;
    ESP_LOGI(TAG, "Starting scheduled recovered telemetry query cycle");
    this->begin_query_cycle_(true);
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Gree OEM boot/telemetry probe:");
    ESP_LOGCONFIG(TAG, "  Start delay: %u ms", this->start_delay_ms_);
    ESP_LOGCONFIG(TAG, "  Frame spacing: %u ms", this->frame_spacing_ms_);
    ESP_LOGCONFIG(TAG, "  Probe response window: %u ms", this->query_spacing_ms_());
    ESP_LOGCONFIG(TAG, "  Wi-Fi status consistency sweep: enabled");
    ESP_LOGCONFIG(TAG, "  Recovered telemetry queries: %s", YESNO(this->query_recovered_data_));
    ESP_LOGCONFIG(TAG, "  Query quiesce delay: %u ms", this->quiesce_delay_ms_);
    ESP_LOGCONFIG(TAG, "  Repeat interval: %u ms", this->repeat_interval_ms_);
    ESP_LOGCONFIG(TAG, "  Restore control: %s", YESNO(this->restore_control_));
  }

 protected:
  uint32_t query_spacing_ms_() const { return this->frame_spacing_ms_ < 900 ? 900 : this->frame_spacing_ms_; }

  void begin_initial_sequence_() {
    this->sequence_active_ = true;
    this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::RECEIVE_ONLY);
    ESP_LOGI(TAG, "Starting captured CS532 OEM boot sequence; normal climate TX is locked");

    const uint32_t t0 = this->start_delay_ms_;
    this->set_timeout("gree_oem_boot_0", t0,
                      [this]() { this->send_(BOOT_IDENTITY, "boot identity 0x10/0x02"); });
    this->set_timeout("gree_oem_boot_1", t0 + this->frame_spacing_ms_,
                      [this]() { this->send_(MAC_REPORT, "module report 0x05/0x04 #1"); });
    this->set_timeout("gree_oem_boot_2", t0 + this->frame_spacing_ms_ * 2,
                      [this]() { this->send_(MAC_REPORT, "module report 0x05/0x04 #2"); });
    this->set_timeout("gree_oem_boot_3", t0 + this->frame_spacing_ms_ * 3,
                      [this]() { this->send_(LINK_SYNC_CONNECTED, "connected link status 0x0E/0x03 #1"); });
    this->set_timeout("gree_oem_boot_4", t0 + this->frame_spacing_ms_ * 4,
                      [this]() { this->send_(LINK_SYNC_CONNECTED, "connected link status 0x0E/0x03 #2"); });
    this->set_timeout("gree_oem_boot_5", t0 + this->frame_spacing_ms_ * 5,
                      [this]() { this->send_(LINK_SYNC_CONNECTED, "connected link status 0x0E/0x03 #3"); });
    this->set_timeout("gree_oem_boot_6", t0 + this->frame_spacing_ms_ * 6,
                      [this]() { this->send_(LINK_SYNC_CONNECTED, "connected link status 0x0E/0x03 #4"); });

    const uint32_t after_boot = t0 + this->frame_spacing_ms_ * 7;
    if (this->query_recovered_data_) {
      this->set_timeout("gree_oem_recovered_start", after_boot + this->quiesce_delay_ms_,
                        [this]() { this->run_recovered_queries_(); });
    } else {
      this->set_timeout("gree_oem_boot_finish", after_boot + 350, [this]() { this->finish_sequence_(); });
    }
  }

  void begin_query_cycle_(bool quiesce) {
    if (this->sequence_active_) return;
    this->sequence_active_ = true;
    this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::RECEIVE_ONLY);
    const uint32_t delay = quiesce ? this->quiesce_delay_ms_ : 0;
    this->set_timeout("gree_oem_query_cycle_start", delay, [this]() { this->run_recovered_queries_(); });
  }

  void run_recovered_queries_() {
    const uint32_t spacing = this->query_spacing_ms_();
    const uint32_t short_gap = 300;

    ESP_LOGI(TAG, "Running Wi-Fi status/module-state consistency sweep for the extended energy page");
    ESP_LOGI(TAG, "Raw RX logging enabled; all synthetic 0x01 frames are read-only polls (control selector 0x00)");

    this->climate_->set_debug(true, false, true, true, 256);

    this->send_(QUERY_FAULT_COMBINED, "combined/general fault query -> 0x33");
    this->set_timeout("gree_oem_query_indoor", spacing,
                      [this]() { this->send_(QUERY_FAULT_INDOOR, "indoor fault/data query -> 0x34"); });
    this->set_timeout("gree_oem_query_outdoor", spacing * 2,
                      [this]() { this->send_(QUERY_FAULT_OUTDOOR, "outdoor-unit query -> 0x35"); });

    const uint32_t sweep = spacing * 3;
    this->set_timeout("gree_wifi_link_connected", sweep,
                      [this]() { this->send_(LINK_SYNC_CONNECTED, "assert connected link status: byte14=0x80"); });

    this->set_timeout("gree_wifi04_poll1", sweep + short_gap,
                      [this]() { this->send_(READ_POLL_WIFI_04, "Wi-Fi field 0x04 read poll 1/3"); });
    this->set_timeout("gree_wifi04_poll2", sweep + short_gap * 2,
                      [this]() { this->send_(READ_POLL_WIFI_04, "Wi-Fi field 0x04 read poll 2/3"); });
    this->set_timeout("gree_wifi04_poll3", sweep + short_gap * 3,
                      [this]() { this->send_(READ_POLL_WIFI_04, "Wi-Fi field 0x04 read poll 3/3"); });
    this->set_timeout("gree_wifi04_energy", sweep + short_gap * 4,
                      [this]() { this->send_(QUERY_ENERGY_STATE_1, "energy query after Wi-Fi=0x04, module-state=1"); });

    const uint32_t profile_08 = sweep + short_gap * 4 + spacing;
    this->set_timeout("gree_wifi08_poll1", profile_08,
                      [this]() { this->send_(READ_POLL_WIFI_08, "Wi-Fi field 0x08 read poll 1/3"); });
    this->set_timeout("gree_wifi08_poll2", profile_08 + short_gap,
                      [this]() { this->send_(READ_POLL_WIFI_08, "Wi-Fi field 0x08 read poll 2/3"); });
    this->set_timeout("gree_wifi08_poll3", profile_08 + short_gap * 2,
                      [this]() { this->send_(READ_POLL_WIFI_08, "Wi-Fi field 0x08 read poll 3/3"); });
    this->set_timeout("gree_wifi08_energy", profile_08 + short_gap * 3,
                      [this]() { this->send_(QUERY_ENERGY_STATE_1, "energy query after Wi-Fi=0x08, module-state=1"); });

    const uint32_t profile_0c = profile_08 + short_gap * 3 + spacing;
    this->set_timeout("gree_wifi0c_poll1", profile_0c,
                      [this]() { this->send_(READ_POLL_WIFI_0C, "Wi-Fi field 0x0C read poll 1/3"); });
    this->set_timeout("gree_wifi0c_poll2", profile_0c + short_gap,
                      [this]() { this->send_(READ_POLL_WIFI_0C, "Wi-Fi field 0x0C read poll 2/3"); });
    this->set_timeout("gree_wifi0c_poll3", profile_0c + short_gap * 2,
                      [this]() { this->send_(READ_POLL_WIFI_0C, "Wi-Fi field 0x0C read poll 3/3"); });
    this->set_timeout("gree_wifi0c_energy1", profile_0c + short_gap * 3,
                      [this]() { this->send_(QUERY_ENERGY_STATE_1, "energy query after Wi-Fi=0x0C, module-state=1"); });

    const uint32_t state_0 = profile_0c + short_gap * 3 + spacing;
    this->set_timeout("gree_state0_poll1", state_0,
                      [this]() { this->send_(READ_POLL_WIFI_0C, "Wi-Fi 0x0C reinforcement poll 1/3 before module-state=0"); });
    this->set_timeout("gree_state0_poll2", state_0 + short_gap,
                      [this]() { this->send_(READ_POLL_WIFI_0C, "Wi-Fi 0x0C reinforcement poll 2/3 before module-state=0"); });
    this->set_timeout("gree_state0_poll3", state_0 + short_gap * 2,
                      [this]() { this->send_(READ_POLL_WIFI_0C, "Wi-Fi 0x0C reinforcement poll 3/3 before module-state=0"); });
    this->set_timeout("gree_state0_energy", state_0 + short_gap * 3,
                      [this]() { this->send_(QUERY_ENERGY_STATE_0, "energy query after Wi-Fi=0x0C, module-state=0"); });

    const uint32_t state_2 = state_0 + short_gap * 3 + spacing;
    this->set_timeout("gree_state2_poll1", state_2,
                      [this]() { this->send_(READ_POLL_WIFI_0C, "Wi-Fi 0x0C reinforcement poll 1/3 before module-state=2"); });
    this->set_timeout("gree_state2_poll2", state_2 + short_gap,
                      [this]() { this->send_(READ_POLL_WIFI_0C, "Wi-Fi 0x0C reinforcement poll 2/3 before module-state=2"); });
    this->set_timeout("gree_state2_poll3", state_2 + short_gap * 2,
                      [this]() { this->send_(READ_POLL_WIFI_0C, "Wi-Fi 0x0C reinforcement poll 3/3 before module-state=2"); });
    this->set_timeout("gree_state2_energy", state_2 + short_gap * 3,
                      [this]() { this->send_(QUERY_ENERGY_STATE_2, "energy query after Wi-Fi=0x0C, module-state=2"); });

    const uint32_t state_3 = state_2 + short_gap * 3 + spacing;
    this->set_timeout("gree_state3_poll1", state_3,
                      [this]() { this->send_(READ_POLL_WIFI_0C, "Wi-Fi 0x0C reinforcement poll 1/3 before module-state=3"); });
    this->set_timeout("gree_state3_poll2", state_3 + short_gap,
                      [this]() { this->send_(READ_POLL_WIFI_0C, "Wi-Fi 0x0C reinforcement poll 2/3 before module-state=3"); });
    this->set_timeout("gree_state3_poll3", state_3 + short_gap * 2,
                      [this]() { this->send_(READ_POLL_WIFI_0C, "Wi-Fi 0x0C reinforcement poll 3/3 before module-state=3"); });
    this->set_timeout("gree_state3_energy", state_3 + short_gap * 3,
                      [this]() { this->send_(QUERY_ENERGY_STATE_3, "energy query after Wi-Fi=0x0C, module-state=3"); });

    this->set_timeout("gree_oem_query_finish", state_3 + short_gap * 3 + spacing,
                      [this]() { this->finish_sequence_(); });
  }

  void finish_sequence_() {
    this->climate_->set_debug(false, false, true, true, 256);
    if (this->restore_control_) {
      this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::CONTROL);
      ESP_LOGI(TAG, "OEM probe sequence complete; raw RX logging disabled and normal climate control enabled");
    } else {
      ESP_LOGI(TAG, "OEM probe sequence complete; raw RX logging disabled; climate remains receive-only");
    }
    this->sequence_active_ = false;
    this->finished_ = true;
    this->last_sequence_finished_at_ = millis();
  }

  template<size_t N> void send_(const std::array<uint8_t, N> &frame, const char *description) {
    uint8_t checksum = 0;
    for (size_t i = 2; i + 1 < N; ++i) checksum = static_cast<uint8_t>(checksum + frame[i]);
    if (frame[0] != 0x7E || frame[1] != 0x7E || static_cast<size_t>(frame[2]) + 3 != N ||
        checksum != frame[N - 1]) {
      ESP_LOGE(TAG, "Refusing invalid recovered frame: %s", description);
      return;
    }
    ESP_LOGI(TAG, "TX OEM request: %s", description);
    this->uart_->write_array(frame.data(), frame.size());
    this->uart_->flush();
  }

  static constexpr const char *TAG = "gree.oem_boot_probe";

  static constexpr std::array<uint8_t, 19> BOOT_IDENTITY{
      0x7E, 0x7E, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x28, 0x1E, 0x19, 0x23, 0x23, 0x00, 0xB8};
  static constexpr std::array<uint8_t, 8> MAC_REPORT{
      0x7E, 0x7E, 0x05, 0x04, 0x07, 0x00, 0x00, 0x10};
  static constexpr std::array<uint8_t, 17> LINK_SYNC_CONNECTED{
      0x7E, 0x7E, 0x0E, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x7E, 0x0F};

  // Captured Livo Gen3 read-only poll shape. Full-frame byte 7 is 0x00,
  // therefore the contained climate fields are informational and not applied.
  // Only full-frame byte 41 changes across these three vectors.
  static constexpr std::array<uint8_t, 50> READ_POLL_WIFI_04{
      0x7E, 0x7E, 0x2F, 0x01, 0x04, 0x00, 0x40, 0x00, 0x91, 0x90,
      0x06, 0xC2, 0x44, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x40, 0x00, 0x3E, 0x35};
  static constexpr std::array<uint8_t, 50> READ_POLL_WIFI_08{
      0x7E, 0x7E, 0x2F, 0x01, 0x04, 0x00, 0x40, 0x00, 0x91, 0x90,
      0x06, 0xC2, 0x44, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x40, 0x00, 0x3E, 0x39};
  static constexpr std::array<uint8_t, 50> READ_POLL_WIFI_0C{
      0x7E, 0x7E, 0x2F, 0x01, 0x04, 0x00, 0x40, 0x00, 0x91, 0x90,
      0x06, 0xC2, 0x44, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x0C, 0x00, 0x02, 0x00, 0x00, 0x40, 0x00, 0x3E, 0x3D};

  static constexpr std::array<uint8_t, 28> QUERY_FAULT_COMBINED{
      0x7E, 0x7E, 0x19, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x3B,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x94};
  static constexpr std::array<uint8_t, 28> QUERY_FAULT_INDOOR{
      0x7E, 0x7E, 0x19, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x3B,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x95};
  static constexpr std::array<uint8_t, 28> QUERY_FAULT_OUTDOOR{
      0x7E, 0x7E, 0x19, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x3B,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x97};

  static constexpr std::array<uint8_t, 28> QUERY_ENERGY_STATE_0{
      0x7E, 0x7E, 0x19, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x3B,
      0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x96};
  static constexpr std::array<uint8_t, 28> QUERY_ENERGY_STATE_1{
      0x7E, 0x7E, 0x19, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x3B,
      0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x97};
  static constexpr std::array<uint8_t, 28> QUERY_ENERGY_STATE_2{
      0x7E, 0x7E, 0x19, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x3B,
      0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x98};
  static constexpr std::array<uint8_t, 28> QUERY_ENERGY_STATE_3{
      0x7E, 0x7E, 0x19, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x3B,
      0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x99};

  uart::UARTComponent *uart_{nullptr};
  sinclair_ac::SinclairAC *climate_{nullptr};
  uint32_t start_delay_ms_{100};
  uint32_t frame_spacing_ms_{450};
  uint32_t quiesce_delay_ms_{1800};
  uint32_t repeat_interval_ms_{0};
  uint32_t last_sequence_finished_at_{0};
  bool restore_control_{true};
  bool query_recovered_data_{true};
  bool sequence_active_{false};
  bool finished_{false};
};

}  // namespace gree_oem_probe
}  // namespace esphome
