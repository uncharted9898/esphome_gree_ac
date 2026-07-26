#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "esphome/components/sinclair_ac/esppac.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "payload_evolution.h"
#include "payload_fingerprint.h"
#include "rtl_handshake.h"
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
  void set_selector_discovery(bool enabled) { this->selector_discovery_ = enabled; }
  void set_module_state_discovery(bool enabled) { this->module_state_discovery_ = enabled; }
  void set_operating_profile(bool enabled) { this->operating_profile_ = enabled; }
  void set_operating_profile_cycles(uint16_t cycles) { this->operating_profile_cycles_ = cycles; }
  void set_module_state_selectors(uint8_t primary, uint8_t secondary) {
    this->module_state_primary_selector_ = primary;
    this->module_state_secondary_selector_ = secondary;
  }

  float get_setup_priority() const override { return setup_priority::LATE; }

  void setup() override {
    if (this->uart_ == nullptr || this->climate_ == nullptr) {
      ESP_LOGE(TAG, "UART and climate references are required");
      this->mark_failed();
      return;
    }
    get_mac_address_raw(this->module_mac_.data());
    if (!mac_address_is_valid(this->module_mac_.data())) {
      ESP_LOGW(TAG, "ESP module MAC is invalid; the RTL command-0x04 handshake may fail");
    }
    this->mac_report_ = build_rtl_mac_report(this->module_mac_);
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
        this->send_and_advance_(RTL_BOOT_IDENTITY, "RTL boot identity 0x10/0x02",
                                Phase::BOOT_MAC_SEND, this->frame_spacing_ms_);
        break;
      case Phase::BOOT_MAC_SEND:
        // The RTL firmware always solicits identification with at least one
        // full-MAC command 0x04 before accepting the resulting 0x44.
        this->send_boot_mac_attempt_(now);
        break;
      case Phase::BOOT_MAC_WAIT:
        if (this->consume_boot_information_44_()) {
          this->begin_link_synchronization_(now);
        } else if (this->boot_mac_attempts_ < RTL_MAC_REPORT_MAX_ATTEMPTS) {
          this->send_boot_mac_attempt_(now);
        } else {
          ESP_LOGW(TAG,
                   "No valid 0x44 after %u full-MAC command-0x04 attempts; "
                   "continuing with the RTL firmware's MID fallback behavior",
                   static_cast<unsigned>(this->boot_mac_attempts_));
          this->begin_link_synchronization_(now);
        }
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
                                        Phase::QUERY_SELECTOR_DISCOVERY);
        } else {
          this->finish_pending_query_();
          ESP_LOGI(TAG, "Skipping 0x40: 0x32 does not advertise ElcEn");
          this->phase_ = Phase::QUERY_SELECTOR_DISCOVERY;
          this->next_action_at_ = now;
        }
        break;
      case Phase::QUERY_SELECTOR_DISCOVERY:
        if (this->selector_discovery_ &&
            this->selector_discovery_index_ < SELECTOR_DISCOVERY_CASES) {
          this->send_next_selector_discovery_query_();
        } else {
          this->finish_pending_query_();
          if (this->selector_discovery_) {
            ESP_LOGI(TAG, "Read-only selector discovery complete: %u combinations tested",
                     static_cast<unsigned>(SELECTOR_DISCOVERY_CASES));
          }
          if (this->module_state_discovery_) {
            this->begin_module_state_discovery_();
          } else if (this->operating_profile_) {
            this->begin_operating_profile_();
          } else {
            this->phase_ = Phase::RESPONSE_DRAIN;
            this->next_action_at_ = now + RESPONSE_DRAIN_MS;
          }
        }
        break;
      case Phase::QUERY_MODULE_STATE_DISCOVERY:
        if (this->module_state_discovery_index_ < MODULE_STATE_DISCOVERY_CASES) {
          this->send_next_module_state_discovery_query_();
        } else {
          this->finish_pending_query_();
          ESP_LOGI(TAG,
                   "Read-only module-state discovery complete: %u states tested "
                   "for primary=0x%02X secondary=0x%02X",
                   static_cast<unsigned>(MODULE_STATE_DISCOVERY_CASES),
                   this->module_state_primary_selector_,
                   this->module_state_secondary_selector_);
          if (this->operating_profile_) {
            this->begin_operating_profile_();
          } else {
            this->phase_ = Phase::RESPONSE_DRAIN;
            this->next_action_at_ = now + RESPONSE_DRAIN_MS;
          }
        }
        break;
      case Phase::QUERY_PROFILE_STATUS:
        this->send_profile_query_and_advance_(QUERY_EXTENDED_STATUS, 0x31,
                                              "profile extended status 0x31",
                                              Phase::QUERY_PROFILE_COMBINED);
        break;
      case Phase::QUERY_PROFILE_COMBINED:
        this->send_profile_query_and_advance_(QUERY_REPORT_COMBINED, 0x33,
                                              "profile combined report 0x33",
                                              Phase::QUERY_PROFILE_INDOOR);
        break;
      case Phase::QUERY_PROFILE_INDOOR:
        this->send_profile_query_and_advance_(QUERY_REPORT_INDOOR, 0x34,
                                              "profile indoor report 0x34",
                                              Phase::QUERY_PROFILE_OUTDOOR);
        break;
      case Phase::QUERY_PROFILE_OUTDOOR:
        this->send_profile_query_and_advance_(QUERY_REPORT_OUTDOOR, 0x35,
                                              "profile outdoor report 0x35",
                                              Phase::QUERY_PROFILE_CYCLE_END);
        break;
      case Phase::QUERY_PROFILE_CYCLE_END:
        this->finish_pending_query_();
        ++this->operating_profile_cycle_index_;
        if (this->operating_profile_cycle_index_ >= this->operating_profile_cycles_) {
          this->log_operating_profile_summary_();
          this->phase_ = Phase::RESPONSE_DRAIN;
          this->next_action_at_ = now + RESPONSE_DRAIN_MS;
        } else {
          this->phase_ = Phase::QUERY_PROFILE_STATUS;
          this->next_action_at_ = now;
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
    ESP_LOGCONFIG(TAG, "  Exhaustive read-only selector discovery: %s", YESNO(this->selector_discovery_));
    ESP_LOGCONFIG(TAG, "  Read-only module-state discovery: %s", YESNO(this->module_state_discovery_));
    ESP_LOGCONFIG(TAG, "  Full operating payload profile: %s", YESNO(this->operating_profile_));
    ESP_LOGCONFIG(TAG, "  Operating profile cycles: %u", this->operating_profile_cycles_);
    ESP_LOGCONFIG(TAG, "  Module-state selector: primary=0x%02X secondary=0x%02X",
                  this->module_state_primary_selector_, this->module_state_secondary_selector_);
    ESP_LOGCONFIG(TAG, "  Outdoor operating repeat interval: %u ms",
                  this->repeat_interval_ms_);
    ESP_LOGCONFIG(TAG, "  Restore control: %s", YESNO(this->restore_control_));
  }

 protected:
  enum class QueryCycle : uint8_t { FULL_DISCOVERY, OUTDOOR_OPERATING };
  enum class DiscoveryKind : uint8_t { SELECTOR, MODULE_STATE };

  enum class Phase : uint8_t {
    IDLE,
    BOOT_IDENTITY,
    BOOT_MAC_SEND,
    BOOT_MAC_WAIT,
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
    QUERY_SELECTOR_DISCOVERY,
    QUERY_MODULE_STATE_DISCOVERY,
    QUERY_PROFILE_STATUS,
    QUERY_PROFILE_COMBINED,
    QUERY_PROFILE_INDOOR,
    QUERY_PROFILE_OUTDOOR,
    QUERY_PROFILE_CYCLE_END,
    RESPONSE_DRAIN,
  };

  static constexpr uint32_t MIN_QUERY_SPACING_MS = 900;
  static constexpr uint32_t RESPONSE_DRAIN_MS = 1500;
  static constexpr uint8_t RTL_MAC_REPORT_MAX_ATTEMPTS = 6;
  static constexpr uint8_t SELECTOR_DISCOVERY_CASES = 64;
  static constexpr uint8_t MODULE_STATE_DISCOVERY_CASES = 8;

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
    this->boot_mac_attempts_ = 0;
    this->boot_0x44_generation_ = this->climate_->get_retained_payload_generation(0x44);
    this->pending_query_active_ = false;
    this->pending_any_response_ = false;
    this->selector_discovery_index_ = 0;
    this->module_state_discovery_index_ = 0;
    this->discovery_baselines_.clear();
    this->operating_profile_cycle_index_ = 0;
    this->operating_profiles_.clear();
    this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::RECEIVE_ONLY);
    this->phase_ = Phase::BOOT_IDENTITY;
    this->next_action_at_ = millis() + this->start_delay_ms_;
    ESP_LOGI(TAG, "Starting captured adapter boot followed by RTL8720CF report queries");
  }

  void send_boot_mac_attempt_(uint32_t now) {
    ++this->boot_mac_attempts_;
    char description[96];
    std::snprintf(description, sizeof(description),
                  "RTL full-MAC command 0x04 attempt %u/%u",
                  static_cast<unsigned>(this->boot_mac_attempts_),
                  static_cast<unsigned>(RTL_MAC_REPORT_MAX_ATTEMPTS));
    this->send_(this->mac_report_, description);
    this->phase_ = Phase::BOOT_MAC_WAIT;
    this->next_action_at_ = now + this->frame_spacing_ms_;
  }

  bool consume_boot_information_44_() {
    const uint32_t generation = this->climate_->get_retained_payload_generation(0x44);
    if (generation == this->boot_0x44_generation_) return false;
    this->boot_0x44_generation_ = generation;

    const auto *payload = this->climate_->get_retained_payload(0x44);
    RtlInformation44Fields fields;
    if (payload == nullptr || !decode_rtl_information_44(*payload, fields)) {
      ESP_LOGW(TAG, "Received malformed RTL information response 0x44");
      return false;
    }

    if (!fields.accepted_by_rtl_firmware) {
      ESP_LOGW(TAG,
               "RTL information response 0x44 did not satisfy firmware gate: "
               "MID=0x%08lX VendorInt=0x%08lX",
               static_cast<unsigned long>(fields.device_mid),
               static_cast<unsigned long>(fields.vendor_id));
      return false;
    }

    char binding_code[RTL_INFORMATION_44_BINDING_CODE_SIZE * 2 + 1];
    static constexpr char HEX[] = "0123456789ABCDEF";
    for (size_t i = 0; i < fields.binding_code.size(); ++i) {
      binding_code[i * 2] = HEX[fields.binding_code[i] >> 4U];
      binding_code[i * 2 + 1] = HEX[fields.binding_code[i] & 0x0FU];
    }
    binding_code[sizeof(binding_code) - 1] = '\0';

    ESP_LOGI(TAG,
             "RTL information response 0x44 accepted: MID=0x%08lX bc=%s "
             "VendorInt=0x%08lX extension=%s",
             static_cast<unsigned long>(fields.device_mid), binding_code,
             static_cast<unsigned long>(fields.vendor_id),
             fields.extension_marker_c9 ? "C9" : (fields.has_extension ? "other" : "none"));
    return true;
  }

  void begin_link_synchronization_(uint32_t now) {
    this->phase_ = Phase::BOOT_LINK_1;
    this->next_action_at_ = now;
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

    if (this->pending_profile_response_) {
      const uint32_t generation_after =
          this->climate_->get_retained_payload_generation(this->pending_expected_command_);
      if (generation_after != this->pending_expected_generation_) {
        const auto *payload = this->climate_->get_retained_payload(this->pending_expected_command_);
        if (payload != nullptr) {
          auto &evolution = this->operating_profiles_[this->pending_expected_command_];
          const std::string differences = evolution.observe(*payload);
          const uint32_t hash = payload_fnv1a(*payload);
          if (differences == "none") {
            ESP_LOGI(TAG,
                     "PROFILE RX cycle=%u cmd=0x%02X payload=%u hash=0x%08X diff=[none]",
                     this->operating_profile_cycle_index_, this->pending_expected_command_,
                     static_cast<unsigned>(payload->size()), static_cast<unsigned>(hash));
          } else {
            const std::string raw = payload_hex(*payload);
            ESP_LOGI(TAG,
                     "PROFILE RX cycle=%u cmd=0x%02X payload=%u hash=0x%08X "
                     "diff=[%s] raw=%s",
                     this->operating_profile_cycle_index_, this->pending_expected_command_,
                     static_cast<unsigned>(payload->size()), static_cast<unsigned>(hash),
                     differences.c_str(), raw.c_str());
          }
        }
      } else {
        ESP_LOGW(TAG, "PROFILE RX timeout cycle=%u cmd=0x%02X",
                 this->operating_profile_cycle_index_, this->pending_expected_command_);
      }
      this->pending_profile_response_ = false;
      this->pending_query_active_ = false;
      return;
    }

    if (this->pending_any_response_) {
      const char *kind = this->pending_discovery_kind_ == DiscoveryKind::MODULE_STATE
                             ? "STATE"
                             : "SELECTOR";
      const uint32_t total_after = this->climate_->get_retained_payload_total_generation();
      if (total_after != this->pending_total_generation_) {
        const uint8_t command = this->climate_->get_last_retained_command();
        const auto *payload = this->climate_->get_retained_payload(command);
        if (payload != nullptr) {
          const uint32_t hash = payload_fnv1a(*payload);
          const auto baseline = this->discovery_baselines_.find(command);
          uint32_t baseline_hash = hash;
          std::string differences = "baseline";
          if (baseline == this->discovery_baselines_.end()) {
            this->discovery_baselines_[command] = *payload;
          } else {
            baseline_hash = payload_fnv1a(baseline->second);
            differences = payload_changed_indices(baseline->second, *payload);
          }
          ESP_LOGI(TAG,
                   "%s RX primary=0x%02X secondary=0x%02X state=0x%02X -> "
                   "cmd=0x%02X payload=%u hash=0x%08X baseline=0x%08X diff=[%s]",
                   kind, this->pending_primary_selector_, this->pending_secondary_selector_,
                   this->pending_module_state_, command,
                   static_cast<unsigned>(payload->size()), static_cast<unsigned>(hash),
                   static_cast<unsigned>(baseline_hash), differences.c_str());
        } else {
          ESP_LOGW(TAG,
                   "%s RX primary=0x%02X secondary=0x%02X state=0x%02X -> "
                   "cmd=0x%02X retained payload missing",
                   kind, this->pending_primary_selector_, this->pending_secondary_selector_,
                   this->pending_module_state_, command);
        }
      } else {
        ESP_LOGW(TAG,
                 "%s RX timeout primary=0x%02X secondary=0x%02X state=0x%02X",
                 kind, this->pending_primary_selector_, this->pending_secondary_selector_,
                 this->pending_module_state_);
      }
      this->pending_any_response_ = false;
      this->pending_query_active_ = false;
      return;
    }

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

  void send_next_selector_discovery_query_() {
    this->finish_pending_query_();
    const uint8_t primary = static_cast<uint8_t>(this->selector_discovery_index_ / 8U);
    const uint8_t secondary = static_cast<uint8_t>(this->selector_discovery_index_ % 8U);
    ++this->selector_discovery_index_;

    this->pending_discovery_kind_ = DiscoveryKind::SELECTOR;
    this->pending_primary_selector_ = primary;
    this->pending_secondary_selector_ = secondary;
    this->pending_module_state_ = 0x01;
    this->pending_total_generation_ = this->climate_->get_retained_payload_total_generation();
    this->pending_any_response_ = true;
    this->pending_query_active_ = true;

    std::snprintf(this->selector_description_, sizeof(this->selector_description_),
                  "read-only selector primary=0x%02X secondary=0x%02X state=0x01",
                  primary, secondary);
    const auto frame = build_rtl_report_query(primary, secondary, 0x3B, 0x01);
    this->send_(frame, this->selector_description_);
    this->phase_ = Phase::QUERY_SELECTOR_DISCOVERY;
    this->next_action_at_ = millis() + this->query_spacing_ms_();
  }

  void begin_module_state_discovery_() {
    this->module_state_discovery_index_ = 0;
    this->discovery_baselines_.clear();
    this->phase_ = Phase::QUERY_MODULE_STATE_DISCOVERY;
    this->next_action_at_ = millis();
    ESP_LOGI(TAG,
             "Starting read-only module-state discovery for primary=0x%02X "
             "secondary=0x%02X",
             this->module_state_primary_selector_, this->module_state_secondary_selector_);
  }

  void send_next_module_state_discovery_query_() {
    this->finish_pending_query_();
    const uint8_t module_state = this->module_state_discovery_index_++;

    this->pending_discovery_kind_ = DiscoveryKind::MODULE_STATE;
    this->pending_primary_selector_ = this->module_state_primary_selector_;
    this->pending_secondary_selector_ = this->module_state_secondary_selector_;
    this->pending_module_state_ = module_state;
    this->pending_total_generation_ = this->climate_->get_retained_payload_total_generation();
    this->pending_any_response_ = true;
    this->pending_query_active_ = true;

    std::snprintf(this->selector_description_, sizeof(this->selector_description_),
                  "read-only module state primary=0x%02X secondary=0x%02X state=0x%02X",
                  this->pending_primary_selector_, this->pending_secondary_selector_,
                  module_state);
    const auto frame = build_rtl_report_query(this->pending_primary_selector_,
                                              this->pending_secondary_selector_,
                                              0x3B, module_state);
    this->send_(frame, this->selector_description_);
    this->phase_ = Phase::QUERY_MODULE_STATE_DISCOVERY;
    this->next_action_at_ = millis() + this->query_spacing_ms_();
  }

  void begin_operating_profile_() {
    this->operating_profile_cycle_index_ = 0;
    this->operating_profiles_.clear();
    this->phase_ = Phase::QUERY_PROFILE_STATUS;
    this->next_action_at_ = millis();
    ESP_LOGI(TAG,
             "Starting full Livo payload evolution profile: %u cycles, "
             "pages 0x31/0x33/0x34/0x35",
             this->operating_profile_cycles_);
  }

  template<size_t N>
  void send_profile_query_and_advance_(const std::array<uint8_t, N> &frame,
                                       uint8_t expected_command,
                                       const char *description, Phase next) {
    this->finish_pending_query_();
    this->pending_expected_command_ = expected_command;
    this->pending_expected_generation_ =
        this->climate_->get_retained_payload_generation(expected_command);
    this->pending_description_ = description;
    this->pending_profile_response_ = true;
    this->pending_query_active_ = true;
    this->send_(frame, description);
    this->phase_ = next;
    this->next_action_at_ = millis() + this->query_spacing_ms_();
  }

  void log_operating_profile_summary_() const {
    ESP_LOGI(TAG, "PROFILE COMPLETE cycles=%u commands=%u",
             this->operating_profile_cycle_index_,
             static_cast<unsigned>(this->operating_profiles_.size()));
    for (const auto &entry : this->operating_profiles_) {
      const uint8_t command = entry.first;
      const PayloadEvolution &evolution = entry.second;
      ESP_LOGI(TAG, "PROFILE BASELINE cmd=0x%02X samples=%u raw=%s", command,
               static_cast<unsigned>(evolution.samples()),
               payload_hex(evolution.baseline()).c_str());
      ESP_LOGI(TAG, "PROFILE LAST cmd=0x%02X raw=%s", command,
               payload_hex(evolution.last()).c_str());
      for (size_t start = 0; start < evolution.size(); start += 6) {
        std::string line;
        const size_t end = std::min(start + 6, evolution.size());
        for (size_t index = start; index < end; ++index) {
          char token[72];
          std::snprintf(token, sizeof(token),
                        "%u=%02X..%02X/x%02X/c%u",
                        static_cast<unsigned>(index), evolution.minimum(index),
                        evolution.maximum(index), evolution.xor_mask(index),
                        static_cast<unsigned>(evolution.change_count(index)));
          if (!line.empty()) line.push_back(' ');
          line += token;
        }
        ESP_LOGI(TAG, "PROFILE MAP cmd=0x%02X bytes=%u-%u %s", command,
                 static_cast<unsigned>(start), static_cast<unsigned>(end - 1),
                 line.c_str());
      }
    }
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

  // Command 0x02 and the full-MAC command 0x04 come directly from the audited
  // RTL8720CF V2/V3 builders. The link-synchronization frame remains a separate
  // target capture. Report selectors use the audited 29-byte command-0x03 format.
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
  DiscoveryKind pending_discovery_kind_{DiscoveryKind::SELECTOR};
  Phase phase_{Phase::IDLE};
  uint32_t start_delay_ms_{100};
  uint32_t frame_spacing_ms_{450};
  uint32_t quiesce_delay_ms_{1800};
  uint32_t scheduled_quiesce_delay_ms_{150};
  uint32_t repeat_interval_ms_{0};
  uint32_t last_sequence_finished_at_{0};
  uint32_t next_action_at_{0};
  uint32_t tx_sequence_{0};
  uint32_t boot_0x44_generation_{0};
  uint32_t pending_expected_generation_{0};
  uint32_t pending_status_generation_{0};
  uint32_t pending_total_generation_{0};
  const char *pending_description_{nullptr};
  char selector_description_[80]{};
  uint8_t pending_expected_command_{0};
  uint8_t boot_mac_attempts_{0};
  uint8_t selector_discovery_index_{0};
  uint8_t module_state_discovery_index_{0};
  uint8_t module_state_primary_selector_{0x04};
  uint8_t module_state_secondary_selector_{0x00};
  uint8_t pending_primary_selector_{0};
  uint8_t pending_secondary_selector_{0};
  uint8_t pending_module_state_{0};
  uint16_t operating_profile_cycles_{40};
  uint16_t operating_profile_cycle_index_{0};
  bool restore_control_{true};
  bool query_recovered_data_{true};
  bool selector_discovery_{false};
  bool module_state_discovery_{false};
  bool operating_profile_{false};
  bool sequence_active_{false};
  bool finished_{false};
  bool pending_query_active_{false};
  bool pending_any_response_{false};
  bool pending_profile_response_{false};
  bool energy_discovery_attempted_{false};
  std::array<uint8_t, 6> module_mac_{};
  std::array<uint8_t, RTL_MAC_REPORT_FRAME_SIZE> mac_report_{};
  std::map<uint8_t, std::vector<uint8_t>> discovery_baselines_;
  std::map<uint8_t, PayloadEvolution> operating_profiles_;
};

}  // namespace gree_oem_probe
}  // namespace esphome
