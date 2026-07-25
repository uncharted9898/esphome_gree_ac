#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/sinclair_ac/esppac.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "rtl_query.h"

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
  void set_scheduled_quiesce_delay(uint32_t delay_ms) { this->scheduled_quiesce_delay_ms_ = delay_ms; }

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
      if (!this->query_recovered_data_) return;
      if (this->repeat_interval_ms_ == 0 ||
          now - this->last_sequence_finished_at_ < this->repeat_interval_ms_) {
        return;
      }
      if (!this->climate_->supplemental_query_may_start()) return;
      ESP_LOGD(TAG, "Starting scheduled RTL8720CF outdoor operating query");
      this->begin_query_sequence_(true, QueryCycle::OUTDOOR_OPERATING);
      return;
    }

    if (!this->due_(now)) return;

    switch (this->phase_) {
      case Phase::BOOT_IDENTITY:
        this->send_and_advance_(BOOT_IDENTITY, "captured boot identity 0x10/0x02",
                                Phase::BOOT_MAC_1, this->frame_spacing_ms_);
        break;
      case Phase::BOOT_MAC_1:
        this->send_and_advance_(MAC_REPORT, "captured module report 0x05/0x04 #1",
                                Phase::BOOT_MAC_2, this->frame_spacing_ms_);
        break;
      case Phase::BOOT_MAC_2:
        this->send_and_advance_(MAC_REPORT, "captured module report 0x05/0x04 #2",
                                Phase::BOOT_LINK_1, this->frame_spacing_ms_);
        break;
      case Phase::BOOT_LINK_1:
        this->send_and_advance_(LINK_SYNC_CONNECTED, "captured link synchronization #1",
                                Phase::BOOT_LINK_2, this->frame_spacing_ms_);
        break;
      case Phase::BOOT_LINK_2:
        this->send_and_advance_(LINK_SYNC_CONNECTED, "captured link synchronization #2",
                                Phase::BOOT_LINK_3, this->frame_spacing_ms_);
        break;
      case Phase::BOOT_LINK_3:
        this->send_and_advance_(LINK_SYNC_CONNECTED, "captured link synchronization #3",
                                Phase::BOOT_LINK_4, this->frame_spacing_ms_);
        break;
      case Phase::BOOT_LINK_4:
        this->send_(LINK_SYNC_CONNECTED, "captured link synchronization #4");
        if (this->query_recovered_data_) {
          this->phase_ = Phase::QUERY_QUIESCE;
          this->next_action_at_ = now + this->quiesce_delay_ms_;
        } else {
          this->phase_ = Phase::RESPONSE_DRAIN;
          this->next_action_at_ = now + RESPONSE_DRAIN_MS;
        }
        break;
      case Phase::QUERY_QUIESCE:
        this->phase_ = this->query_cycle_ == QueryCycle::OUTDOOR_OPERATING
                           ? Phase::QUERY_OUTDOOR
                           : Phase::QUERY_COMBINED;
        this->next_action_at_ = now;
        break;
      case Phase::QUERY_COMBINED:
        this->send_query_and_advance_(QUERY_REPORT_COMBINED, 0x33,
                                      "combined/general report selector -> 0x33",
                                      Phase::QUERY_INDOOR);
        break;
      case Phase::QUERY_INDOOR:
        this->send_query_and_advance_(QUERY_REPORT_INDOOR, 0x34,
                                      "indoor report selector -> 0x34",
                                      Phase::QUERY_OUTDOOR);
        break;
      case Phase::QUERY_OUTDOOR:
        this->send_query_and_advance_(
            QUERY_REPORT_OUTDOOR, 0x35,
            "outdoor operating report selector -> 0x35",
            this->query_cycle_ == QueryCycle::OUTDOOR_OPERATING
                ? Phase::RESPONSE_DRAIN
                : Phase::QUERY_STATUS);
        break;
      case Phase::QUERY_STATUS:
        this->send_query_and_advance_(QUERY_EXTENDED_STATUS, 0x31,
                                      "extended status selector -> 0x31",
                                      Phase::QUERY_PAGE_42);
        break;
      case Phase::QUERY_PAGE_42:
        this->send_query_and_advance_(QUERY_SECONDARY_PAGE_42, 0x42,
                                      "secondary service selector bit 0 -> 0x42",
                                      Phase::QUERY_PAGE_41);
        break;
      case Phase::QUERY_PAGE_41:
        this->send_query_and_advance_(QUERY_SECONDARY_PAGE_41, 0x41,
                                      "secondary service selector bit 1 -> 0x41",
                                      Phase::QUERY_ENERGY);
        break;
      case Phase::QUERY_ENERGY:
        if (this->should_query_energy_()) {
          this->energy_discovery_attempted_ = true;
          this->send_query_and_advance_(QUERY_ENERGY_MONTH, 0x40,
                                        "secondary electrical selector bit 2 -> 0x40",
                                        Phase::RESPONSE_DRAIN);
        } else {
          this->finish_pending_query_();
          ESP_LOGI(TAG, "Skipping 0x40: 0x32 does not advertise ElcEn");
          this->phase_ = Phase::RESPONSE_DRAIN;
          this->next_action_at_ = now + RESPONSE_DRAIN_MS;
        }
        break;
      case Phase::RESPONSE_DRAIN:
        this->finish_pending_query_();
        this->finish_sequence_();
        break;
      case Phase::IDLE:
      default:
        break;
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Gree OEM boot/RTL8720CF telemetry probe:");
    ESP_LOGCONFIG(TAG, "  Start delay: %u ms", this->start_delay_ms_);
    ESP_LOGCONFIG(TAG, "  Captured boot frame spacing: %u ms", this->frame_spacing_ms_);
    ESP_LOGCONFIG(TAG, "  Query response window: %u ms", this->query_spacing_ms_());
    ESP_LOGCONFIG(TAG, "  Query quiesce delay: %u ms", this->quiesce_delay_ms_);
    ESP_LOGCONFIG(TAG, "  Scheduled query quiesce delay: %u ms", this->scheduled_quiesce_delay_ms_);
    ESP_LOGCONFIG(TAG, "  Recovered report queries: %s", YESNO(this->query_recovered_data_));
    ESP_LOGCONFIG(TAG, "  Outdoor operating repeat interval: %u ms",
                  this->repeat_interval_ms_);
    ESP_LOGCONFIG(TAG, "  Restore control: %s", YESNO(this->restore_control_));
  }

 protected:
  enum class QueryCycle : uint8_t { FULL_DISCOVERY, OUTDOOR_OPERATING };

  enum class Phase : uint8_t {
    IDLE,
    BOOT_IDENTITY,
    BOOT_MAC_1,
    BOOT_MAC_2,
    BOOT_LINK_1,
    BOOT_LINK_2,
    BOOT_LINK_3,
    BOOT_LINK_4,
    QUERY_QUIESCE,
    QUERY_COMBINED,
    QUERY_INDOOR,
    QUERY_OUTDOOR,
    QUERY_STATUS,
    QUERY_PAGE_42,
    QUERY_PAGE_41,
    QUERY_ENERGY,
    RESPONSE_DRAIN,
  };

  static constexpr uint32_t MIN_QUERY_SPACING_MS = 900;
  static constexpr uint32_t RESPONSE_DRAIN_MS = 1500;

  uint32_t query_spacing_ms_() const {
    return this->frame_spacing_ms_ < MIN_QUERY_SPACING_MS ? MIN_QUERY_SPACING_MS
                                                          : this->frame_spacing_ms_;
  }

  bool due_(uint32_t now) const {
    return static_cast<int32_t>(now - this->next_action_at_) >= 0;
  }

  void begin_initial_sequence_() {
    this->query_cycle_ = QueryCycle::FULL_DISCOVERY;
    this->sequence_active_ = true;
    this->tx_sequence_ = 0;
    this->pending_query_active_ = false;
    this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::RECEIVE_ONLY);
    this->phase_ = Phase::BOOT_IDENTITY;
    this->next_action_at_ = millis() + this->start_delay_ms_;
    ESP_LOGI(TAG, "Starting captured adapter boot followed by RTL8720CF report queries");
  }

  void begin_query_sequence_(bool quiesce, QueryCycle cycle) {
    if (this->sequence_active_) return;
    this->query_cycle_ = cycle;
    this->sequence_active_ = true;
    this->tx_sequence_ = 0;
    this->pending_query_active_ = false;
    this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::RECEIVE_ONLY);
    this->phase_ = Phase::QUERY_QUIESCE;
    const uint32_t delay = cycle == QueryCycle::OUTDOOR_OPERATING
                               ? this->scheduled_quiesce_delay_ms_
                               : (quiesce ? this->quiesce_delay_ms_ : 0);
    this->next_action_at_ = millis() + delay;
  }


  bool should_query_energy_() const {
    // The audited RTL8720CF dispatcher uses 0x32 payload[0] bit 0 to advertise
    // the optional 0x40 report page. 0x32 payload[1] is the separate ElcEn
    // property value and is only consumed after a supported 0x40 arrives.
    const auto *electrical = this->climate_->get_retained_payload(0x40);
    if (electrical != nullptr) return true;

    const auto *synchronization = this->climate_->get_retained_payload(0x32);
    if (synchronization == nullptr || synchronization->empty()) {
      return !this->energy_discovery_attempted_;
    }
    return ((*synchronization)[0] & 0x01U) != 0;
  }

  void finish_pending_query_() {
    if (!this->pending_query_active_) return;

    const uint32_t expected_after =
        this->climate_->get_retained_payload_generation(this->pending_expected_command_);
    const uint32_t status_after = this->climate_->get_retained_payload_generation(0x31);
    if (expected_after != this->pending_expected_generation_) {
      if (this->query_cycle_ == QueryCycle::OUTDOOR_OPERATING) {
        ESP_LOGD(TAG, "PROBE RX matched %s with command 0x%02X", this->pending_description_,
                 this->pending_expected_command_);
      } else {
        ESP_LOGI(TAG, "PROBE RX matched %s with command 0x%02X", this->pending_description_,
                 this->pending_expected_command_);
      }
    } else if (status_after != this->pending_status_generation_) {
      if (this->query_cycle_ == QueryCycle::OUTDOOR_OPERATING) {
        ESP_LOGD(TAG, "PROBE RX fallback after %s: command 0x31", this->pending_description_);
      } else {
        ESP_LOGI(TAG, "PROBE RX fallback after %s: command 0x31", this->pending_description_);
      }
    } else {
      ESP_LOGW(TAG, "PROBE RX timeout after %s: expected command 0x%02X",
               this->pending_description_, this->pending_expected_command_);
    }
    this->pending_query_active_ = false;
  }

  template<size_t N>
  void send_query_and_advance_(const std::array<uint8_t, N> &frame, uint8_t expected_command,
                               const char *description, Phase next) {
    this->finish_pending_query_();
    this->pending_expected_command_ = expected_command;
    this->pending_expected_generation_ =
        this->climate_->get_retained_payload_generation(expected_command);
    this->pending_status_generation_ = this->climate_->get_retained_payload_generation(0x31);
    this->pending_description_ = description;
    this->pending_query_active_ = true;
    this->send_(frame, description);
    this->phase_ = next;
    this->next_action_at_ = millis() + this->query_spacing_ms_();
  }

  void finish_sequence_() {
    if (this->restore_control_) {
      this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::CONTROL);
      if (this->query_cycle_ == QueryCycle::OUTDOOR_OPERATING) {
        ESP_LOGD(TAG, "Outdoor operating query complete; normal climate control enabled");
      } else {
        ESP_LOGI(TAG, "OEM telemetry discovery sequence complete; normal climate control enabled");
      }
    } else {
      ESP_LOGI(TAG, "OEM telemetry sequence complete; climate remains receive-only");
    }
    this->phase_ = Phase::IDLE;
    this->sequence_active_ = false;
    this->finished_ = true;
    this->last_sequence_finished_at_ = millis();
  }

  template<size_t N>
  void send_and_advance_(const std::array<uint8_t, N> &frame, const char *description,
                         Phase next, uint32_t delay_ms) {
    this->send_(frame, description);
    this->phase_ = next;
    this->next_action_at_ = millis() + delay_ms;
  }

  template<size_t N>
  static uint8_t checksum_(const std::array<uint8_t, N> &frame) {
    uint8_t checksum = 0;
    for (size_t i = 2; i + 1 < N; ++i) {
      checksum = static_cast<uint8_t>(checksum + frame[i]);
    }
    return checksum;
  }

  template<size_t N>
  void send_(const std::array<uint8_t, N> &frame, const char *description) {
    if (frame[0] != 0x7E || frame[1] != 0x7E ||
        static_cast<size_t>(frame[2]) + 3 != N || checksum_(frame) != frame[N - 1]) {
      ESP_LOGE(TAG, "Refusing invalid recovered frame: %s", description);
      return;
    }

    ++this->tx_sequence_;
    if (this->query_cycle_ == QueryCycle::OUTDOOR_OPERATING) {
      ESP_LOGD(TAG, "PROBE TX #%u cmd=0x%02X bytes=%u: %s", this->tx_sequence_, frame[3],
               static_cast<unsigned>(N), description);
    } else {
      ESP_LOGI(TAG, "PROBE TX #%u cmd=0x%02X bytes=%u: %s", this->tx_sequence_, frame[3],
               static_cast<unsigned>(N), description);
    }
    // At 4800-8E1, flushing a 29-byte frame blocks the ESPHome loop for about
    // 66 ms. The 900 ms state-machine spacing makes a blocking flush needless.
    this->uart_->write_array(frame.data(), frame.size());
  }

  static constexpr const char *TAG = "gree.oem_boot_probe";

  // These three startup frames were captured from the target installation.
  // Report selectors below come from the independently audited RTL8720CF V2/V3
  // firmware and deliberately use its 29-byte command-0x03 wire format.
  static constexpr std::array<uint8_t, 19> BOOT_IDENTITY{
      0x7E, 0x7E, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x28, 0x1E, 0x19, 0x23, 0x23, 0x00, 0xB8};
  static constexpr std::array<uint8_t, 8> MAC_REPORT{
      0x7E, 0x7E, 0x05, 0x04, 0x07, 0x00, 0x00, 0x10};
  static constexpr std::array<uint8_t, 17> LINK_SYNC_CONNECTED{
      0x7E, 0x7E, 0x0E, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x7E, 0x0F};

  static constexpr auto QUERY_REPORT_COMBINED = build_rtl_report_query(0x01, 0x00);
  static constexpr auto QUERY_REPORT_INDOOR = build_rtl_report_query(0x02, 0x00);
  static constexpr auto QUERY_REPORT_OUTDOOR = build_rtl_report_query(0x04, 0x00);
  static constexpr auto QUERY_EXTENDED_STATUS = build_rtl_report_query(0x00, 0x00);
  static constexpr auto QUERY_SECONDARY_PAGE_42 = build_rtl_report_query(0x00, 0x01);
  static constexpr auto QUERY_SECONDARY_PAGE_41 = build_rtl_report_query(0x00, 0x02);
  static constexpr auto QUERY_ENERGY_MONTH = build_rtl_report_query(0x00, 0x04);

  uart::UARTComponent *uart_{nullptr};
  sinclair_ac::SinclairAC *climate_{nullptr};
  QueryCycle query_cycle_{QueryCycle::FULL_DISCOVERY};
  Phase phase_{Phase::IDLE};
  uint32_t start_delay_ms_{100};
  uint32_t frame_spacing_ms_{450};
  uint32_t quiesce_delay_ms_{1800};
  uint32_t scheduled_quiesce_delay_ms_{150};
  uint32_t repeat_interval_ms_{0};
  uint32_t last_sequence_finished_at_{0};
  uint32_t next_action_at_{0};
  uint32_t tx_sequence_{0};
  uint32_t pending_expected_generation_{0};
  uint32_t pending_status_generation_{0};
  const char *pending_description_{nullptr};
  uint8_t pending_expected_command_{0};
  bool restore_control_{true};
  bool query_recovered_data_{true};
  bool sequence_active_{false};
  bool finished_{false};
  bool pending_query_active_{false};
  bool energy_discovery_attempted_{false};
};

}  // namespace gree_oem_probe
}  // namespace esphome
