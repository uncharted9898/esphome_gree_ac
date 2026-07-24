#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

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
    const uint32_t now = millis();

    if (!this->sequence_active_) {
      if (this->repeat_interval_ms_ == 0 || now - this->last_sequence_finished_at_ < this->repeat_interval_ms_) return;
      ESP_LOGI(TAG, "Starting scheduled serialized Wi-Fi-gated energy experiment");
      this->begin_scheduled_sequence_();
      return;
    }

    switch (this->phase_) {
      case Phase::BOOT_IDENTITY:
        if (this->due_(now)) this->send_static_and_advance_(BOOT_IDENTITY, "boot identity 0x10/0x02", Phase::BOOT_MAC_1);
        break;
      case Phase::BOOT_MAC_1:
        if (this->due_(now)) this->send_static_and_advance_(MAC_REPORT, "module report 0x05/0x04 #1", Phase::BOOT_MAC_2);
        break;
      case Phase::BOOT_MAC_2:
        if (this->due_(now)) this->send_static_and_advance_(MAC_REPORT, "module report 0x05/0x04 #2", Phase::BOOT_LINK_1);
        break;
      case Phase::BOOT_LINK_1:
        if (this->due_(now)) this->send_static_and_advance_(LINK_SYNC_CONNECTED, "connected link status 0x0E/0x03 #1", Phase::BOOT_LINK_2);
        break;
      case Phase::BOOT_LINK_2:
        if (this->due_(now)) this->send_static_and_advance_(LINK_SYNC_CONNECTED, "connected link status 0x0E/0x03 #2", Phase::BOOT_LINK_3);
        break;
      case Phase::BOOT_LINK_3:
        if (this->due_(now)) this->send_static_and_advance_(LINK_SYNC_CONNECTED, "connected link status 0x0E/0x03 #3", Phase::BOOT_LINK_4);
        break;
      case Phase::BOOT_LINK_4:
        if (this->due_(now)) {
          this->send_(LINK_SYNC_CONNECTED, "connected link status 0x0E/0x03 #4");
          if (!this->query_recovered_data_) {
            this->finish_sequence_();
            break;
          }
          this->phase_ = Phase::WAIT_FOR_STATUS;
          this->phase_started_at_ = now;
          this->next_action_at_ = now + this->serialized_gap_ms_();
        }
        break;
      case Phase::SCHEDULED_QUIESCE:
        if (this->due_(now)) {
          this->send_(LINK_SYNC_CONNECTED, "scheduled connected link status assertion: byte14=0x80");
          this->phase_ = Phase::WAIT_FOR_STATUS;
          this->phase_started_at_ = now;
          this->next_action_at_ = now + this->serialized_gap_ms_();
        }
        break;
      case Phase::WAIT_FOR_STATUS:
        if (this->has_live_status_() && this->due_(now)) {
          this->start_connected_dwell_(now);
        } else if (now - this->phase_started_at_ >= STATUS_WAIT_TIMEOUT_MS) {
          ESP_LOGE(TAG, "No usable 0x31 payload arrived; aborting serialized energy experiment");
          this->finish_sequence_();
        }
        break;
      case Phase::CONNECTED_DWELL:
        this->run_connected_dwell_(now);
        break;
      case Phase::QUIET_DRAIN:
        if (this->due_(now)) this->send_isolated_energy_query_(now);
        break;
      case Phase::ENERGY_RESPONSE_WINDOW:
        if (!this->energy_0x40_seen_ && this->climate_->get_retained_payload(0x40) != nullptr) {
          this->energy_0x40_seen_ = true;
          ESP_LOGI(TAG, "SERIALIZED PROBE RESULT: command 0x40 observed during isolated response window");
        }
        if (this->due_(now)) {
          ESP_LOGI(TAG, "Isolated energy response window closed after %u ms; no further probe TX occurred", ENERGY_RESPONSE_WINDOW_MS);
          this->finish_sequence_();
        }
        break;
      case Phase::IDLE:
      default:
        break;
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Gree OEM serialized Wi-Fi-gated energy probe:");
    ESP_LOGCONFIG(TAG, "  Start delay: %u ms", this->start_delay_ms_);
    ESP_LOGCONFIG(TAG, "  Serialized frame gap: %u ms", this->serialized_gap_ms_());
    ESP_LOGCONFIG(TAG, "  Connected-state dwell: %u ms", CONNECTED_DWELL_MS);
    ESP_LOGCONFIG(TAG, "  Quiet drain before energy request: %u ms", QUIET_DRAIN_MS);
    ESP_LOGCONFIG(TAG, "  Isolated energy response window: %u ms", ENERGY_RESPONSE_WINDOW_MS);
    ESP_LOGCONFIG(TAG, "  Connected normal-poll Wi-Fi field: 0x%02X", CONNECTED_WIFI_FIELD);
    ESP_LOGCONFIG(TAG, "  Energy query module-state: %u", ENERGY_MODULE_STATE);
    ESP_LOGCONFIG(TAG, "  Energy query RSSI magnitude fields: %u", QUERY_RSSI_MAGNITUDE);
    ESP_LOGCONFIG(TAG, "  Recovered telemetry queries: %s", YESNO(this->query_recovered_data_));
    ESP_LOGCONFIG(TAG, "  Repeat interval: %u ms", this->repeat_interval_ms_);
    ESP_LOGCONFIG(TAG, "  Restore control: %s", YESNO(this->restore_control_));
  }

 protected:
  enum class Phase : uint8_t {
    IDLE,
    BOOT_IDENTITY,
    BOOT_MAC_1,
    BOOT_MAC_2,
    BOOT_LINK_1,
    BOOT_LINK_2,
    BOOT_LINK_3,
    BOOT_LINK_4,
    SCHEDULED_QUIESCE,
    WAIT_FOR_STATUS,
    CONNECTED_DWELL,
    QUIET_DRAIN,
    ENERGY_RESPONSE_WINDOW,
  };

  static constexpr uint32_t MIN_SERIALIZED_GAP_MS = 900;
  static constexpr uint32_t STATUS_WAIT_TIMEOUT_MS = 5000;
  static constexpr uint32_t CONNECTED_DWELL_MS = 30000;
  static constexpr uint32_t LINK_REFRESH_MS = 5000;
  static constexpr uint32_t QUIET_DRAIN_MS = 2000;
  static constexpr uint32_t ENERGY_RESPONSE_WINDOW_MS = 3000;
  static constexpr uint8_t CONNECTED_WIFI_FIELD = 0x0C;
  static constexpr uint8_t ENERGY_MODULE_STATE = 0x01;
  static constexpr uint8_t QUERY_RSSI_MAGNITUDE = 0x3B;
  static constexpr size_t OUTGOING_PAYLOAD_LEN = 45;
  static constexpr size_t OUTGOING_WIFI_PAYLOAD_INDEX = 37;
  static constexpr size_t SET_AF_PAYLOAD_INDEX = 3;
  static constexpr size_t SET_CONST_BIT_PAYLOAD_INDEX = 7;
  static constexpr size_t SET_NOCHANGE_PAYLOAD_INDEX = 11;
  static constexpr size_t SET_CONST_02_PAYLOAD_INDEX = 39;

  uint32_t serialized_gap_ms_() const {
    return this->frame_spacing_ms_ < MIN_SERIALIZED_GAP_MS ? MIN_SERIALIZED_GAP_MS : this->frame_spacing_ms_;
  }

  bool due_(uint32_t now) const { return static_cast<int32_t>(now - this->next_action_at_) >= 0; }

  void begin_initial_sequence_() {
    this->sequence_active_ = true;
    this->energy_0x40_seen_ = this->climate_->get_retained_payload(0x40) != nullptr;
    this->tx_sequence_ = 0;
    this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::RECEIVE_ONLY);
    this->climate_->set_debug(true, false, true, true, 256);
    this->phase_ = Phase::BOOT_IDENTITY;
    this->phase_started_at_ = millis();
    this->next_action_at_ = this->phase_started_at_ + this->start_delay_ms_;
    ESP_LOGI(TAG, "Starting serialized CS532 boot followed by one coherent connected-state energy experiment");
  }

  void begin_scheduled_sequence_() {
    this->sequence_active_ = true;
    this->energy_0x40_seen_ = this->climate_->get_retained_payload(0x40) != nullptr;
    this->tx_sequence_ = 0;
    this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::RECEIVE_ONLY);
    this->climate_->set_debug(true, false, true, true, 256);
    this->phase_ = Phase::SCHEDULED_QUIESCE;
    this->phase_started_at_ = millis();
    this->next_action_at_ = this->phase_started_at_ + this->quiesce_delay_ms_;
  }

  bool has_live_status_() const {
    const auto *status = this->climate_->get_retained_payload(0x31);
    return status != nullptr && status->size() >= OUTGOING_PAYLOAD_LEN;
  }

  void start_connected_dwell_(uint32_t now) {
    this->phase_ = Phase::CONNECTED_DWELL;
    this->phase_started_at_ = now;
    this->dwell_started_at_ = now;
    this->last_link_refresh_at_ = now;
    this->next_action_at_ = now;
    ESP_LOGI(TAG, "CONNECTED EMULATION START: 30000 ms, link=0x80, poll Wi-Fi byte=0x0C, serialized gap=%u ms",
             this->serialized_gap_ms_());
  }

  void run_connected_dwell_(uint32_t now) {
    if (now - this->dwell_started_at_ >= CONNECTED_DWELL_MS) {
      this->phase_ = Phase::QUIET_DRAIN;
      this->phase_started_at_ = now;
      this->next_action_at_ = now + QUIET_DRAIN_MS;
      ESP_LOGI(TAG, "CONNECTED EMULATION COMPLETE: entering %u ms no-transmit quiet drain", QUIET_DRAIN_MS);
      return;
    }
    if (!this->due_(now)) return;

    if (now - this->last_link_refresh_at_ >= LINK_REFRESH_MS) {
      this->send_(LINK_SYNC_CONNECTED, "connected link refresh: byte14=0x80");
      this->last_link_refresh_at_ = now;
    } else if (!this->send_connected_poll_()) {
      ESP_LOGW(TAG, "Live 0x31 payload unavailable while dwelling; sending connected link refresh instead");
      this->send_(LINK_SYNC_CONNECTED, "connected link refresh while waiting for live status");
      this->last_link_refresh_at_ = now;
    }
    this->next_action_at_ = now + this->serialized_gap_ms_();
  }

  bool send_connected_poll_() {
    const auto *status = this->climate_->get_retained_payload(0x31);
    if (status == nullptr || status->size() < OUTGOING_PAYLOAD_LEN) return false;

    std::vector<uint8_t> payload(status->begin(), status->begin() + OUTGOING_PAYLOAD_LEN);
    payload[SET_AF_PAYLOAD_INDEX] = 0x00;
    payload[SET_CONST_BIT_PAYLOAD_INDEX] |= 0x02;
    payload[SET_NOCHANGE_PAYLOAD_INDEX] |= 0x08;
    payload[SET_CONST_02_PAYLOAD_INDEX] = 0x02;
    payload[OUTGOING_WIFI_PAYLOAD_INDEX] = CONNECTED_WIFI_FIELD;

    std::vector<uint8_t> frame;
    frame.reserve(payload.size() + 5);
    frame.push_back(0x7E);
    frame.push_back(0x7E);
    frame.push_back(static_cast<uint8_t>(payload.size() + 2));
    frame.push_back(0x01);
    frame.insert(frame.end(), payload.begin(), payload.end());
    uint8_t checksum = 0;
    for (size_t i = 2; i < frame.size(); ++i) checksum = static_cast<uint8_t>(checksum + frame[i]);
    frame.push_back(checksum);
    this->send_vector_(frame, "connected read poll generated from latest 0x31; full-frame byte41=0x0C");
    return true;
  }

  void send_isolated_energy_query_(uint32_t now) {
    std::array<uint8_t, 28> query = QUERY_ENERGY_TEMPLATE;
    query[10] = QUERY_RSSI_MAGNITUDE;
    query[13] = QUERY_RSSI_MAGNITUDE;
    query[26] = ENERGY_MODULE_STATE;
    query[27] = this->checksum_(query);

    ESP_LOGI(TAG, "ISOLATED ENERGY TEST: UART has been probe-silent for %u ms; sending exactly one selector-8 request", QUIET_DRAIN_MS);
    this->send_(query, "isolated monthly-energy query: Wi-Fi=0x0C dwell, RSSI=59, module-state=1");
    this->phase_ = Phase::ENERGY_RESPONSE_WINDOW;
    this->phase_started_at_ = now;
    this->next_action_at_ = now + ENERGY_RESPONSE_WINDOW_MS;
    ESP_LOGI(TAG, "ISOLATED RESPONSE WINDOW OPEN: %u ms with no additional probe transmissions", ENERGY_RESPONSE_WINDOW_MS);
  }

  void finish_sequence_() {
    this->climate_->set_debug(false, false, true, true, 256);
    if (this->restore_control_) {
      this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::CONTROL);
      ESP_LOGI(TAG, "Serialized Wi-Fi-gated energy experiment complete; normal climate control enabled");
    } else {
      ESP_LOGI(TAG, "Serialized Wi-Fi-gated energy experiment complete; climate remains receive-only");
    }
    this->phase_ = Phase::IDLE;
    this->sequence_active_ = false;
    this->finished_ = true;
    this->last_sequence_finished_at_ = millis();
  }

  template<size_t N>
  void send_static_and_advance_(const std::array<uint8_t, N> &frame, const char *description, Phase next) {
    this->send_(frame, description);
    this->phase_ = next;
    this->phase_started_at_ = millis();
    this->next_action_at_ = this->phase_started_at_ + this->serialized_gap_ms_();
  }

  template<size_t N> static uint8_t checksum_(const std::array<uint8_t, N> &frame) {
    uint8_t checksum = 0;
    for (size_t i = 2; i + 1 < N; ++i) checksum = static_cast<uint8_t>(checksum + frame[i]);
    return checksum;
  }

  template<size_t N> void send_(const std::array<uint8_t, N> &frame, const char *description) {
    if (frame[0] != 0x7E || frame[1] != 0x7E || static_cast<size_t>(frame[2]) + 3 != N ||
        checksum_(frame) != frame[N - 1]) {
      ESP_LOGE(TAG, "Refusing invalid probe frame: %s", description);
      return;
    }
    ++this->tx_sequence_;
    const uint32_t tx_started = millis();
    ESP_LOGI(TAG, "PROBE TX #%u start=%u cmd=0x%02X bytes=%u: %s", this->tx_sequence_, tx_started, frame[3],
             static_cast<unsigned>(N), description);
    this->uart_->write_array(frame.data(), frame.size());
    this->uart_->flush();
    ESP_LOGI(TAG, "PROBE TX #%u complete=%u", this->tx_sequence_, millis());
  }

  void send_vector_(const std::vector<uint8_t> &frame, const char *description) {
    if (frame.size() < 5 || frame[0] != 0x7E || frame[1] != 0x7E ||
        static_cast<size_t>(frame[2]) + 3 != frame.size()) {
      ESP_LOGE(TAG, "Refusing invalid generated probe frame: %s", description);
      return;
    }
    uint8_t checksum = 0;
    for (size_t i = 2; i + 1 < frame.size(); ++i) checksum = static_cast<uint8_t>(checksum + frame[i]);
    if (checksum != frame.back()) {
      ESP_LOGE(TAG, "Refusing generated probe frame with invalid checksum: %s", description);
      return;
    }
    ++this->tx_sequence_;
    const uint32_t tx_started = millis();
    ESP_LOGI(TAG, "PROBE TX #%u start=%u cmd=0x%02X bytes=%u: %s", this->tx_sequence_, tx_started, frame[3],
             static_cast<unsigned>(frame.size()), description);
    this->uart_->write_array(frame.data(), frame.size());
    this->uart_->flush();
    ESP_LOGI(TAG, "PROBE TX #%u complete=%u", this->tx_sequence_, millis());
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

  static constexpr std::array<uint8_t, 28> QUERY_ENERGY_TEMPLATE{
      0x7E, 0x7E, 0x19, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x3B, 0x00, 0x00, 0x3B, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x97};

  uart::UARTComponent *uart_{nullptr};
  sinclair_ac::SinclairAC *climate_{nullptr};
  Phase phase_{Phase::IDLE};
  uint32_t start_delay_ms_{100};
  uint32_t frame_spacing_ms_{450};
  uint32_t quiesce_delay_ms_{1800};
  uint32_t repeat_interval_ms_{0};
  uint32_t last_sequence_finished_at_{0};
  uint32_t phase_started_at_{0};
  uint32_t next_action_at_{0};
  uint32_t dwell_started_at_{0};
  uint32_t last_link_refresh_at_{0};
  uint32_t tx_sequence_{0};
  bool restore_control_{true};
  bool query_recovered_data_{true};
  bool sequence_active_{false};
  bool finished_{false};
  bool energy_0x40_seen_{false};
};

}  // namespace gree_oem_probe
}  // namespace esphome
