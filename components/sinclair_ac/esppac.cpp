// based on: https://github.com/DomiStyle/esphome-panasonic-ac
#include "esppac.h"
#include "protocol_frame.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome {
namespace sinclair_ac {

static const char *const TAG = "sinclair_ac";

climate::ClimateTraits SinclairAC::traits()
{
    auto traits = climate::ClimateTraits();

    traits.set_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
    traits.set_visual_min_temperature(MIN_TEMPERATURE);
    traits.set_visual_max_temperature(MAX_TEMPERATURE);
    traits.set_visual_temperature_step(TEMPERATURE_STEP);

    traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_AUTO, climate::CLIMATE_MODE_COOL,
                                climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_FAN_ONLY, climate::CLIMATE_MODE_DRY});

    traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_BOTH,
                                      climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL});

    return traits;
}

void SinclairAC::setup()
{
    if (this->fan_profile_ == FanProfile::GREE_4_SPEED) {
        this->set_supported_custom_fan_modes({fan_modes::FAN_AUTO, fan_modes::FAN_QUIET, fan_modes::FAN_LOW,
                                              fan_modes::FAN_MED, fan_modes::FAN_HIGH, fan_modes::FAN_TURBO});
    } else {
        this->set_supported_custom_fan_modes({fan_modes::FAN_AUTO, fan_modes::FAN_QUIET, fan_modes::FAN_LOW,
                                              fan_modes::FAN_MEDL, fan_modes::FAN_MED, fan_modes::FAN_MEDH,
                                              fan_modes::FAN_HIGH, fan_modes::FAN_TURBO});
    }

  // Initialize times
    this->reset_parser();
    this->init_time_ = millis();
    this->last_packet_sent_ = this->init_time_;
    this->last_packet_received_ = this->init_time_;
    this->wait_response_ = false;
    this->mode = climate::CLIMATE_MODE_OFF;
    this->target_temperature = MIN_TEMPERATURE;
    this->current_temperature = NAN;

    ESP_LOGI(TAG, "Sinclair AC component v%s starting...", VERSION);
    this->publish_protocol_state("initializing");
    if (this->protocol_mode_sensor_) this->protocol_mode_sensor_->publish_state(this->protocol_mode_name());
    if (this->fan_decode_profile_sensor_) this->fan_decode_profile_sensor_->publish_state(this->fan_profile_name());
    if (this->receive_only_sensor_) this->receive_only_sensor_->publish_state(this->is_receive_only());
    if (this->poll_only_sensor_) this->poll_only_sensor_->publish_state(this->is_poll_only());
    if (this->communication_sensor_) this->communication_sensor_->publish_state(false);
    this->publish_diagnostics(true);
}

void SinclairAC::loop()
{
    if (this->serialProcess_.state == STATE_RECIEVE && millis() - this->serialProcess_.started_at > this->frame_timeout_ms()) {
        ESP_LOGW(TAG, "Discarding truncated UART frame after timeout");
        this->reset_parser(true);
        this->frame_timeouts_++;
        this->publish_protocol_state("timeout");
    }
    read_data();  // Read data from UART (if there is any)
    this->publish_discovery_capture();
}

void SinclairAC::read_data()
{
    while (available())  // Read while data is available
    {
        /* Do not overwrite a completed frame before its owner has processed it. */
        if (this->serialProcess_.state == STATE_COMPLETE)
        {
            break;
        }
        uint8_t c;
        this->read_byte(&c);  // Store in receive buffer

        if (this->serialProcess_.state == STATE_RESTART) this->reset_parser();
        switch (this->serialProcess_.state)
        {
            case STATE_WAIT_SYNC:
                if (c == 0x7E) {
                    if (this->serialProcess_.data.size() == 1 && this->serialProcess_.data[0] == 0x7E) {
                        this->serialProcess_.data.push_back(c);
                    } else {
                        this->serialProcess_.data.assign(1, c);
                    }
                } else if (this->serialProcess_.data.size() == 2) {
                    if (c < 3 || c > DATA_MAX - 3) { this->reset_parser(true); break; }
                    this->serialProcess_.data.push_back(c);

                    // LEN counts CMD + payload + checksum. Complete frame also
                    // includes two sync bytes and the LEN byte.
                    this->serialProcess_.frame_size = static_cast<size_t>(c) + 3;
                    this->serialProcess_.started_at = millis();
                    this->serialProcess_.state = STATE_RECIEVE;
                } else {
                    this->serialProcess_.data.clear();
                }
                break;
            case STATE_RECIEVE:
                this->serialProcess_.data.push_back(c);
                if (this->serialProcess_.data.size() > this->serialProcess_.frame_size) { this->reset_parser(true); break; }
                if (this->serialProcess_.data.size() == this->serialProcess_.frame_size) this->serialProcess_.state = STATE_COMPLETE;
                break;
            case STATE_RESTART:
            case STATE_COMPLETE:
                break;
            default:
                this->serialProcess_.state = STATE_WAIT_SYNC;
                this->serialProcess_.data.clear();
                break;
        }

    }
}

uint32_t SinclairAC::frame_timeout_ms() const {
    const uint32_t bytes = this->serialProcess_.frame_size == 0 ? DATA_MAX : this->serialProcess_.frame_size;
    return (bytes * UART_BITS_PER_CHARACTER * 1000UL + UART_BAUD - 1) / UART_BAUD + FRAME_TIMEOUT_MARGIN_MS;
}
const char *SinclairAC::protocol_mode_name() const {
    switch (this->protocol_mode_) { case ProtocolMode::RECEIVE_ONLY: return "receive_only"; case ProtocolMode::POLL_ONLY: return "poll_only"; default: return "control"; }
}
void SinclairAC::reset_parser(bool resynchronized) {
    this->serialProcess_.data.clear();
    this->serialProcess_.frame_size = 0;
    this->serialProcess_.started_at = 0;
    this->serialProcess_.state = STATE_WAIT_SYNC;
    if (resynchronized) this->parser_resyncs_++;
}

void SinclairAC::set_telemetry_discovery(bool enabled, bool expose_raw_payload, bool expose_raw_bytes, bool log_changes_only, uint8_t history_depth) {
    this->telemetry_discovery_enabled_ = enabled;
    this->telemetry_expose_raw_payload_ = expose_raw_payload;
    this->telemetry_expose_raw_bytes_ = expose_raw_bytes;
    this->telemetry_log_changes_only_ = log_changes_only;
    this->telemetry_history_depth_ = history_depth;
    this->telemetry_capture_.set_history_depth(history_depth);
}
const char *SinclairAC::fan_profile_name() const {
    switch (this->fan_profile_) { case FanProfile::SINCLAIR_EXTENDED: return "sinclair_extended"; case FanProfile::GREE_4_SPEED: return "gree_4_speed"; default: return "auto"; }
}
void SinclairAC::retain_payload(uint8_t command, const std::vector<uint8_t> &payload) {
    // Retain every checksum-valid command, including unsupported commands.
    // This is observational and deliberately does not acknowledge requests.
    this->telemetry_capture_.observe(command, payload, millis());
    this->capture_packet(false, command, payload);
    const auto previous = this->last_payloads_.find(command);
    const bool raw_changed = previous == this->last_payloads_.end() || previous->second != payload;
    bool meaningful_changed = raw_changed;
    if (meaningful_changed && command == 0x31 && previous != this->last_payloads_.end() &&
        previous->second.size() == payload.size() && payload.size() > 42) {
        meaningful_changed = false;
        for (size_t i = 0; i < payload.size(); ++i) {
            // Byte 42 is confirmed indoor temperature; byte 44 deliberately remains visible.
            if (i != 42 && previous->second[i] != payload[i]) { meaningful_changed = true; break; }
        }
    }
    // Raw retention is unconditional: the Last 0x31 entity must never be stale.
    this->last_payloads_[command] = payload;
    ++this->payload_generations_[command];
    if (this->telemetry_expose_raw_payload_) {
        text_sensor::TextSensor *target = nullptr;
        switch (command) {
            case 0x31: target = this->last_0x31_payload_sensor_; break;
            case 0x33: target = this->last_0x33_payload_sensor_; break;
            case 0x40: target = this->last_0x40_payload_sensor_; break;
            case 0x44: target = this->last_0x44_payload_sensor_; break;
            default:
                // Known diagnostic pages are retained for component-level
                // decoders, but they are not unsupported/unknown payloads.
                if (!sinclair_ac_protocol::is_diagnostic_command(command)) {
                    target = this->last_unknown_payload_sensor_;
                }
                break;
        }
        if (target != nullptr && (raw_changed || !this->telemetry_log_changes_only_)) {
            target->publish_state(format_hex_pretty(payload));
        }
    }
    if (!this->telemetry_discovery_enabled_) return;
    if (meaningful_changed || !this->telemetry_log_changes_only_) ESP_LOGD(TAG, "Telemetry discovery: cmd=0x%02X payload changed (%u bytes)", command, payload.size());
    this->publish_discovery_capture();
}

void SinclairAC::capture_packet(bool transmitted, uint8_t command, const std::vector<uint8_t> &payload) {
    CaptureRecord record;
    record.timestamp_ms = millis(); record.transmitted = transmitted; record.command = command; record.payload = payload;
    record.decoded_mode = static_cast<uint8_t>(this->mode); record.target_temperature = this->target_temperature;
    record.indoor_temperature = this->current_temperature;
    record.requested_fan.clear();
    if (this->has_custom_fan_mode()) record.requested_fan = this->get_custom_fan_mode();
    this->telemetry_capture_.capture(record);
    this->discovery_capture_dirty_ = true;
}

void SinclairAC::publish_discovery_capture() {
    if (!this->telemetry_discovery_enabled_ || !this->discovery_capture_dirty_ ||
        (this->last_discovery_summary_publish_ != 0 && millis() - this->last_discovery_summary_publish_ < 5000)) return;
    this->last_discovery_summary_publish_ = millis();
    const std::string summary = this->telemetry_capture_.summary();
    const std::string capture_export = this->telemetry_capture_.export_csv();
    if (this->discovery_summary_sensor_ && (!this->has_published_discovery_capture_ || summary != this->published_discovery_summary_)) {
        this->discovery_summary_sensor_->publish_state(summary);
    }
    if (this->capture_export_sensor_ && (!this->has_published_discovery_capture_ || capture_export != this->published_capture_export_)) {
        this->capture_export_sensor_->publish_state(capture_export);
    }
    this->published_discovery_summary_ = summary;
    this->published_capture_export_ = capture_export;
    this->has_published_discovery_capture_ = true;
    this->discovery_capture_dirty_ = false;
}

void SinclairAC::set_debug(bool rx, bool tx, bool unknown, bool differences, uint16_t maximum_hex_length) {
    this->log_rx_ = rx; this->log_tx_ = tx; this->log_unknown_ = unknown; this->log_differences_ = differences; this->maximum_hex_length_ = maximum_hex_length;
}
void SinclairAC::publish_protocol_state(const char *state) {
    if (this->protocol_state_ == state) return;

    this->protocol_state_ = state;
    if (this->protocol_state_sensor_) this->protocol_state_sensor_->publish_state(state);
}
void SinclairAC::record_received_packet(bool known, bool establishes_health) {
    this->valid_rx_packets_++;
    if (!known) this->unknown_packets_++;
    if (!establishes_health) return;
    this->last_packet_received_ = millis();
    if (this->communication_sensor_) this->communication_sensor_->publish_state(true);
    this->publish_protocol_state("ready");
}
void SinclairAC::record_transmitted_packet(const std::vector<uint8_t> &packet) {
    this->valid_tx_packets_++;
    if (packet.size() >= 5 && packet[0] == 0x7E && packet[1] == 0x7E) {
        this->capture_packet(true, packet[3], std::vector<uint8_t>(packet.begin() + 4, packet.end() - 1));
        this->publish_discovery_capture();
    }
}
void SinclairAC::publish_diagnostics(bool force) {
    if (!force && millis() - this->last_diagnostics_publish_ < 5000) return;
    this->last_diagnostics_publish_ = millis();
    if (this->valid_rx_packets_sensor_) this->valid_rx_packets_sensor_->publish_state(this->valid_rx_packets_);
    if (this->valid_tx_packets_sensor_) this->valid_tx_packets_sensor_->publish_state(this->valid_tx_packets_);
    if (this->unknown_packets_sensor_) this->unknown_packets_sensor_->publish_state(this->unknown_packets_);
    if (this->checksum_failures_sensor_) this->checksum_failures_sensor_->publish_state(this->checksum_failures_);
    if (this->invalid_length_sensor_) this->invalid_length_sensor_->publish_state(this->invalid_lengths_);
    if (this->too_short_sensor_) this->too_short_sensor_->publish_state(this->too_short_frames_);
    if (this->parser_resync_sensor_) this->parser_resync_sensor_->publish_state(this->parser_resyncs_);
    if (this->frame_timeout_sensor_) this->frame_timeout_sensor_->publish_state(this->frame_timeouts_);
    this->publish_last_packet_diagnostics(true);
}

void SinclairAC::record_last_packet_diagnostics(uint32_t length, uint32_t type, const std::string &description,
                                                const std::string *unknown_description) {
    const bool changed = !this->has_last_packet_diagnostics_ || this->last_packet_length_ != length ||
                         this->last_packet_type_ != type || this->last_packet_description_ != description ||
                         (unknown_description != nullptr && this->last_unknown_packet_description_ != *unknown_description);
    this->has_last_packet_diagnostics_ = true;
    this->last_packet_length_ = length;
    this->last_packet_type_ = type;
    this->last_packet_description_ = description;
    if (unknown_description != nullptr) this->last_unknown_packet_description_ = *unknown_description;
    if (changed) this->publish_last_packet_diagnostics(true);
}

void SinclairAC::publish_last_packet_diagnostics(bool force) {
    if (!this->has_last_packet_diagnostics_) return;
    if (!force && millis() - this->last_packet_diagnostics_publish_ < 5000) return;
    this->last_packet_diagnostics_publish_ = millis();
    if (this->last_packet_length_sensor_) this->last_packet_length_sensor_->publish_state(this->last_packet_length_);
    if (this->last_packet_type_sensor_) this->last_packet_type_sensor_->publish_state(this->last_packet_type_);
    if (this->last_packet_sensor_) this->last_packet_sensor_->publish_state(this->last_packet_description_);
    if (this->last_unknown_packet_sensor_ && !this->last_unknown_packet_description_.empty()) {
        this->last_unknown_packet_sensor_->publish_state(this->last_unknown_packet_description_);
    }
}

void SinclairAC::record_fan_diagnostics(uint8_t speed_field_1_raw, uint8_t speed_field_1_low_3_bits,
                                        uint8_t speed_field_2_raw, bool quiet, bool turbo, const char *decode_status) {
    const uint8_t quiet_raw = quiet ? 1 : 0;
    const uint8_t turbo_raw = turbo ? 1 : 0;
    const std::string status(decode_status);
    const bool changed = !this->has_fan_diagnostics_ || this->fan_speed_field_1_raw_ != speed_field_1_raw ||
                         this->fan_speed_field_1_low_3_bits_ != speed_field_1_low_3_bits ||
                         this->fan_speed_field_2_raw_ != speed_field_2_raw || this->fan_quiet_raw_ != quiet_raw ||
                         this->fan_turbo_raw_ != turbo_raw || this->fan_decode_status_ != status;
    this->has_fan_diagnostics_ = true;
    this->fan_speed_field_1_raw_ = speed_field_1_raw;
    this->fan_speed_field_1_low_3_bits_ = speed_field_1_low_3_bits;
    this->fan_speed_field_2_raw_ = speed_field_2_raw;
    this->fan_quiet_raw_ = quiet_raw;
    this->fan_turbo_raw_ = turbo_raw;
    this->fan_decode_status_ = status;
    if (!changed) return;
    if (this->fan_speed_field_1_raw_sensor_) this->fan_speed_field_1_raw_sensor_->publish_state(speed_field_1_raw);
    if (this->fan_speed_field_1_low_3_bits_sensor_) this->fan_speed_field_1_low_3_bits_sensor_->publish_state(speed_field_1_low_3_bits);
    if (this->fan_speed_field_2_raw_sensor_) this->fan_speed_field_2_raw_sensor_->publish_state(speed_field_2_raw);
    if (this->fan_quiet_raw_sensor_) this->fan_quiet_raw_sensor_->publish_state(quiet_raw);
    if (this->fan_turbo_raw_sensor_) this->fan_turbo_raw_sensor_->publish_state(turbo_raw);
    if (this->fan_decode_status_sensor_) this->fan_decode_status_sensor_->publish_state(status);
}

void SinclairAC::log_packet_difference(const std::vector<uint8_t> &packet) {
    if (!this->log_differences_ || packet.size() < 4) return;
    auto &old = this->previous_frames_[packet[3]];
    if (!old.empty() && old.size() != packet.size()) ESP_LOGD(TAG, "RX cmd=0x%02X frame length changed: %u -> %u", packet[3], old.size(), packet.size());
    for (size_t i = 0; i < old.size() && i < packet.size(); i++) if (old[i] != packet[i]) { const char *field = i < 2 ? "sync" : i == 2 ? "length" : i == 3 ? "command" : i + 1 == packet.size() ? "checksum" : "payload"; ESP_LOGD(TAG, "changed %s%s%u: 0x%02X -> 0x%02X xor=0x%02X", field, std::string(field) == "payload" ? "[" : "", std::string(field) == "payload" ? static_cast<unsigned>(i - 4) : static_cast<unsigned>(i), old[i], packet[i], old[i] ^ packet[i]); }
    old = packet;
}

void SinclairAC::update_current_temperature(float temperature)
{
    if (temperature > TEMPERATURE_THRESHOLD) {
        ESP_LOGW(TAG, "Received out of range inside temperature: %f", temperature);
        return;
    }

    this->current_temperature = temperature;
}

void SinclairAC::update_target_temperature(float temperature)
{
    if (temperature > TEMPERATURE_THRESHOLD) {
        ESP_LOGW(TAG, "Received out of range target temperature %.2f", temperature);
        return;
    }

    this->target_temperature = temperature;
}

void SinclairAC::update_swing_horizontal(const std::string &swing)
{
    this->horizontal_swing_state_ = swing;

    if (this->horizontal_swing_select_ != nullptr &&
        this->horizontal_swing_select_->current_option().str() != this->horizontal_swing_state_)
    {
        this->horizontal_swing_select_->publish_state(this->horizontal_swing_state_);
    }
}

void SinclairAC::update_swing_vertical(const std::string &swing)
{
    this->vertical_swing_state_ = swing;

    if (this->vertical_swing_select_ != nullptr && 
        this->vertical_swing_select_->current_option().str() != this->vertical_swing_state_)
    {
        this->vertical_swing_select_->publish_state(this->vertical_swing_state_);
    }
}

void SinclairAC::update_display(const std::string &display)
{
    this->display_state_ = display;

    if (this->display_select_ != nullptr && 
        this->display_select_->current_option().str() != this->display_state_)
    {
        this->display_select_->publish_state(this->display_state_);
    }
}

void SinclairAC::update_display_unit(const std::string &display_unit)
{
    this->display_unit_state_ = display_unit;

    if (this->display_unit_select_ != nullptr && 
        this->display_unit_select_->current_option().str() != this->display_unit_state_)
    {
        this->display_unit_select_->publish_state(this->display_unit_state_);
    }
}

void SinclairAC::update_plasma(bool plasma)
{
    this->plasma_state_ = plasma;

    if (this->plasma_switch_ != nullptr)
    {
        this->plasma_switch_->publish_state(this->plasma_state_);
    }
}

void SinclairAC::update_sleep(bool sleep)
{
    this->sleep_state_ = sleep;

    if (this->sleep_switch_ != nullptr)
    {
        this->sleep_switch_->publish_state(this->sleep_state_);
    }
}

void SinclairAC::update_xfan(bool xfan)
{
    this->xfan_state_ = xfan;

    if (this->xfan_switch_ != nullptr)
    {
        this->xfan_switch_->publish_state(this->xfan_state_);
    }
}

void SinclairAC::update_save(bool save)
{
    this->save_state_ = save;

    if (this->save_switch_ != nullptr)
    {
        this->save_switch_->publish_state(this->save_state_);
    }
}

climate::ClimateAction SinclairAC::determine_action()
{
    if (this->mode == climate::CLIMATE_MODE_OFF) {
        return climate::CLIMATE_ACTION_OFF;
    } else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY) {
        return climate::CLIMATE_ACTION_FAN;
    } else if (this->mode == climate::CLIMATE_MODE_DRY) {
        return climate::CLIMATE_ACTION_DRYING;
    } else if ((this->mode == climate::CLIMATE_MODE_COOL || this->mode == climate::CLIMATE_MODE_HEAT_COOL) &&
                this->current_temperature + TEMPERATURE_TOLERANCE >= this->target_temperature) {
        return climate::CLIMATE_ACTION_COOLING;
    } else if ((this->mode == climate::CLIMATE_MODE_HEAT || this->mode == climate::CLIMATE_MODE_HEAT_COOL) &&
                this->current_temperature - TEMPERATURE_TOLERANCE <= this->target_temperature) {
        return climate::CLIMATE_ACTION_HEATING;
    } else {
        return climate::CLIMATE_ACTION_IDLE;
    }
}

/*
 * Sensor handling
 */

void SinclairAC::set_current_temperature_sensor(sensor::Sensor *current_temperature_sensor)
{
    this->current_temperature_sensor_ = current_temperature_sensor;
    this->current_temperature_sensor_->add_on_state_callback([this](float state)
        {
            this->current_temperature = state;
            this->publish_state();
        });
}

void SinclairAC::set_vertical_swing_select(select::Select *vertical_swing_select)
{
    this->vertical_swing_select_ = vertical_swing_select;
    this->vertical_swing_select_->add_on_state_callback([this](size_t index) {
        auto selected = this->vertical_swing_select_->at(index);
        if (!selected.has_value())
            return;
        auto &value = selected.value();
        if (value == this->vertical_swing_state_)
            return;
        this->on_vertical_swing_change(value);
    });
}

void SinclairAC::set_horizontal_swing_select(select::Select *horizontal_swing_select)
{
    this->horizontal_swing_select_ = horizontal_swing_select;
    this->horizontal_swing_select_->add_on_state_callback([this](size_t index) {
        auto selected = this->horizontal_swing_select_->at(index);
        if (!selected.has_value())
            return;
        auto &value = selected.value();
        if (value == this->horizontal_swing_state_)
            return;
        this->on_horizontal_swing_change(value);
    });
}

void SinclairAC::set_display_select(select::Select *display_select)
{
    this->display_select_ = display_select;
    this->display_select_->add_on_state_callback([this](size_t index) {
        auto selected = this->display_select_->at(index);
        if (!selected.has_value())
            return;
        auto &value = selected.value();
        if (value == this->display_state_)
            return;
        this->on_display_change(value);
    });
}

void SinclairAC::set_display_unit_select(select::Select *display_unit_select)
{
    this->display_unit_select_ = display_unit_select;
    this->display_unit_select_->add_on_state_callback([this](size_t index) {
        auto selected = this->display_unit_select_->at(index);
        if (!selected.has_value())
            return;
        auto &value = selected.value();
        if (value == this->display_unit_state_)
            return;
        this->on_display_unit_change(value);
    });
}

void SinclairAC::set_plasma_switch(switch_::Switch *plasma_switch)
{
    this->plasma_switch_ = plasma_switch;
    this->plasma_switch_->add_on_state_callback([this](bool state) {
        if (state == this->plasma_state_)
            return;
        this->on_plasma_change(state);
    });
}

void SinclairAC::set_sleep_switch(switch_::Switch *sleep_switch)
{
    this->sleep_switch_ = sleep_switch;
    this->sleep_switch_->add_on_state_callback([this](bool state) {
        if (state == this->sleep_state_)
            return;
        this->on_sleep_change(state);
    });
}

void SinclairAC::set_xfan_switch(switch_::Switch *xfan_switch)
{
    this->xfan_switch_ = xfan_switch;
    this->xfan_switch_->add_on_state_callback([this](bool state) {
        if (state == this->xfan_state_)
            return;
        this->on_xfan_change(state);
    });
}

void SinclairAC::set_save_switch(switch_::Switch *save_switch)
{
    this->save_switch_ = save_switch;
    this->save_switch_->add_on_state_callback([this](bool state) {
        if (state == this->save_state_)
            return;
        this->on_save_change(state);
    });
}

/*
 * Debugging
 */

void SinclairAC::log_packet(const std::vector<uint8_t> &data, bool outgoing)
{
    if ((outgoing && !this->log_tx_) || (!outgoing && !this->log_rx_)) return;
    const size_t bytes = std::min(data.size(), static_cast<size_t>(this->maximum_hex_length_));
    std::vector<uint8_t> display(data.begin(), data.begin() + bytes);
    const uint8_t calculated = data.size() >= 5 ? [&data](){ uint8_t sum=0; for (size_t i=2;i+1<data.size();++i) sum += data[i]; return sum; }() : 0;
    const uint8_t received = data.size() >= 5 ? data.back() : 0;
    ESP_LOGD(TAG, "%s command=0x%02X declared=%u actual=%u payload=%u calculated_checksum=0x%02X received_checksum=0x%02X checksum=%s: %s%s", outgoing ? "TX" : "RX", data.size()>3 ? data[3] : 0, data.size()>2 ? data[2] : 0, data.size(), data.size()>=5 ? data.size()-5 : 0, calculated, received, calculated==received ? "OK" : "FAIL", format_hex_pretty(display).c_str(), bytes<data.size()?" ...":"");
}

}  // namespace sinclair_ac
}  // namespace esphome
