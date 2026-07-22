// based on: https://github.com/DomiStyle/esphome-panasonic-ac
#include "esppac_cnt.h"
#include "protocol_frame.h"

#include <cassert>

namespace esphome {
namespace sinclair_ac {
namespace CNT {

static const char *const TAG = "sinclair_ac.serial";

void SinclairACCNT::setup()
{
    SinclairAC::setup();

    ESP_LOGD(TAG, "Using serial protocol for Sinclair AC");
    this->publish_request_diagnostics(true);
}

void SinclairACCNT::begin_pending_control()
{
    if (this->pending_control_.active) return;
    auto &pending = this->pending_control_;
    pending.active = true;
    pending.requested_fields = 0;
    pending.retries = 0;
    pending.mode = this->mode;
    pending.target_temperature = this->target_temperature;
    pending.custom_fan_mode = fan_modes::FAN_AUTO;
    if (this->has_custom_fan_mode()) {
        pending.custom_fan_mode = this->get_custom_fan_mode();
    }
    pending.vertical_swing = this->vertical_swing_state_;
    pending.horizontal_swing = this->horizontal_swing_state_;
    pending.display_mode = this->display_state_;
    pending.display_unit = this->display_unit_state_;
    pending.plasma = this->plasma_state_; pending.sleep = this->sleep_state_;
    pending.xfan = this->xfan_state_; pending.save = this->save_state_;
}

void SinclairACCNT::restart_pending_control(uint16_t fields)
{
    this->begin_pending_control();
    this->pending_control_.requested_fields |= fields;
    this->pending_control_.retries = 0;
    this->update_ = ACUpdate::UpdateStart;
    this->wait_response_ = false;
    this->request_lifecycle_.outstanding_request = OutstandingRequest::NONE;
}

bool SinclairACCNT::pending_control_matches_report(const std::vector<uint8_t> &payload) const
{
    (void) payload;
    const auto &pending = this->pending_control_;
    if (!pending.active) return true;
    if ((pending.requested_fields & PENDING_MODE) &&
        (this->mode != pending.mode || (pending.mode != climate::CLIMATE_MODE_OFF && !this->power_internal_))) return false;
    if ((pending.requested_fields & PENDING_TARGET_TEMPERATURE) && this->target_temperature != pending.target_temperature) return false;
    if ((pending.requested_fields & PENDING_FAN) && (!this->has_custom_fan_mode() || this->get_custom_fan_mode() != pending.custom_fan_mode)) return false;
    if ((pending.requested_fields & PENDING_VERTICAL_SWING) && this->vertical_swing_state_ != pending.vertical_swing) return false;
    if ((pending.requested_fields & PENDING_HORIZONTAL_SWING) && this->horizontal_swing_state_ != pending.horizontal_swing) return false;
    if ((pending.requested_fields & PENDING_DISPLAY) && this->display_state_ != pending.display_mode) return false;
    if ((pending.requested_fields & PENDING_DISPLAY_UNIT) && this->display_unit_state_ != pending.display_unit) return false;
    if ((pending.requested_fields & PENDING_PLASMA) && this->plasma_state_ != pending.plasma) return false;
    if ((pending.requested_fields & PENDING_SLEEP) && this->sleep_state_ != pending.sleep) return false;
    if ((pending.requested_fields & PENDING_XFAN) && this->xfan_state_ != pending.xfan) return false;
    if ((pending.requested_fields & PENDING_SAVE) && this->save_state_ != pending.save) return false;
    return true;
}

void SinclairACCNT::handle_pending_control_response(const std::vector<uint8_t> &payload)
{
    const auto request = this->request_lifecycle_.outstanding_request;
    if (request == OutstandingRequest::POLL) {
        this->request_lifecycle_.acknowledge_report(millis());
        this->wait_response_ = false;  // retained solely for compatibility with base setup.
        return;
    }
    if (request != OutstandingRequest::COMMAND_APPLY && request != OutstandingRequest::COMMAND_CLEAR) return;
    this->request_lifecycle_.acknowledge_report(millis());
    this->wait_response_ = false;
    const bool matches = this->pending_control_matches_report(payload);
    if (!matches) {
        ++this->request_lifecycle_.command_mismatches;
        if (++this->pending_control_.retries >= 3) {
            this->pending_control_.active = false;
            this->update_ = ACUpdate::NoUpdate;
            this->last_command_result_ = "failed";
            this->last_command_failure_reason_ = "mismatch";
            this->publish_protocol_state("command_failed_mismatch");
            return;
        }
        this->update_ = ACUpdate::UpdateStart;
        this->last_command_result_ = "retrying";
        this->publish_protocol_state("command_mismatch_retry");
        return;
    }
    if (request == OutstandingRequest::COMMAND_APPLY) {
        this->update_ = ACUpdate::UpdateClear;
        this->last_command_result_ = "clear_pending";
        this->publish_protocol_state("command_clear_pending");
    } else {
        this->pending_control_.active = false;
        this->update_ = ACUpdate::NoUpdate;
        this->last_command_result_ = "verified";
        this->last_command_failure_reason_ = "none";
        this->publish_protocol_state("command_verified");
    }
}

void SinclairACCNT::publish_request_diagnostics(bool force)
{
    const auto &d = this->request_lifecycle_;
    const bool initial = !this->has_published_request_diagnostics_;
    if (this->polls_sent_sensor_ && (force || initial || d.polls_sent != this->published_polls_sent_)) this->polls_sent_sensor_->publish_state(d.polls_sent);
    if (this->poll_responses_sensor_ && (force || initial || d.poll_responses != this->published_poll_responses_)) this->poll_responses_sensor_->publish_state(d.poll_responses);
    if (this->poll_response_timeouts_sensor_ && (force || initial || d.poll_response_timeouts != this->published_poll_response_timeouts_)) this->poll_response_timeouts_sensor_->publish_state(d.poll_response_timeouts);
    if (this->consecutive_poll_timeouts_sensor_ && (force || initial || d.consecutive_poll_timeouts != this->published_consecutive_poll_timeouts_)) this->consecutive_poll_timeouts_sensor_->publish_state(d.consecutive_poll_timeouts);
    if (this->last_poll_response_ms_sensor_ && (force || initial || d.last_poll_response_ms != this->published_last_poll_response_ms_)) this->last_poll_response_ms_sensor_->publish_state(d.last_poll_response_ms);
    if (this->command_attempts_sensor_ && (force || initial || d.command_attempts != this->published_command_attempts_)) this->command_attempts_sensor_->publish_state(d.command_attempts);
    if (this->command_response_timeouts_sensor_ && (force || initial || d.command_response_timeouts != this->published_command_response_timeouts_)) this->command_response_timeouts_sensor_->publish_state(d.command_response_timeouts);
    if (this->command_mismatches_sensor_ && (force || initial || d.command_mismatches != this->published_command_mismatches_)) this->command_mismatches_sensor_->publish_state(d.command_mismatches);
    if (this->last_command_result_sensor_ && (force || initial || this->last_command_result_ != this->published_last_command_result_)) this->last_command_result_sensor_->publish_state(this->last_command_result_);
    if (this->last_command_failure_reason_sensor_ && (force || initial || this->last_command_failure_reason_ != this->published_last_command_failure_reason_)) this->last_command_failure_reason_sensor_->publish_state(this->last_command_failure_reason_);
    this->published_polls_sent_ = d.polls_sent;
    this->published_poll_responses_ = d.poll_responses;
    this->published_poll_response_timeouts_ = d.poll_response_timeouts;
    this->published_consecutive_poll_timeouts_ = d.consecutive_poll_timeouts;
    this->published_last_poll_response_ms_ = d.last_poll_response_ms;
    this->published_command_attempts_ = d.command_attempts;
    this->published_command_response_timeouts_ = d.command_response_timeouts;
    this->published_command_mismatches_ = d.command_mismatches;
    this->published_last_command_result_ = this->last_command_result_;
    this->published_last_command_failure_reason_ = this->last_command_failure_reason_;
    this->has_published_request_diagnostics_ = true;
    this->last_request_diagnostics_publish_ = millis();
}

void SinclairACCNT::loop()
{
    /* this reads data from UART */
    SinclairAC::loop();

    /* we have a frame from AC */
    if (this->serialProcess_.state == STATE_COMPLETE)
    {
        /* do not forget to order for restart of the recieve state machine */
        this->serialProcess_.state = STATE_RESTART;
        /* log for ESPHome debug */
        log_packet(this->serialProcess_.data);

        const auto validation = verify_packet();
        if (validation == PacketValidationResult::INVALID_TOO_SHORT || validation == PacketValidationResult::INVALID_LENGTH || validation == PacketValidationResult::INVALID_CHECKSUM) {
            this->reset_parser();
        } else {
        const bool known = validation == PacketValidationResult::VALID_KNOWN;
        if (known) this->record_received_packet(true);  // Unsupported traffic is not proof of HVAC health.
        this->log_packet_difference(this->serialProcess_.data);
        this->retain_payload(this->serialProcess_.data[3], std::vector<uint8_t>(this->serialProcess_.data.begin() + 4, this->serialProcess_.data.end() - 1));
        const std::string packet_description = "RX cmd=0x" + format_hex_pretty(std::vector<uint8_t>{this->serialProcess_.data[3]});
        if (!known) {
            ESP_LOGD(TAG, "RX valid unsupported command 0x%02X retained for discovery", this->serialProcess_.data[3]);
            if (this->log_unknown_ && !this->log_rx_) this->log_packet(this->serialProcess_.data);
            const size_t n = std::min(this->serialProcess_.data.size(), static_cast<size_t>(this->maximum_hex_length_));
            const std::string unknown_packet_description = format_hex_pretty(std::vector<uint8_t>(this->serialProcess_.data.begin(), this->serialProcess_.data.begin() + n));
            this->record_last_packet_diagnostics(this->serialProcess_.data.size(), this->serialProcess_.data[3], packet_description, &unknown_packet_description);
        } else {
        this->record_last_packet_diagnostics(this->serialProcess_.data.size(), this->serialProcess_.data[3], packet_description);

        /* A valid recieved packet of accepted type marks module as being ready */
        if (this->state_ != ACState::Ready)
        {
            this->state_ = ACState::Ready;  
            Component::status_clear_error();
            this->last_packet_sent_ = millis();
        }

        handle_packet(); /* Reports are acknowledgements as well as state updates. */
        const std::vector<uint8_t> payload(this->serialProcess_.data.begin() + 4, this->serialProcess_.data.end() - 1);
        this->handle_pending_control_response(payload); /* Verify after decoded state has been updated. */
        this->publish_request_diagnostics();
        }
        this->reset_parser();
    }  // closes validation else
    }  // closes: if (serialProcess_.state == STATE_COMPLETE)

    this->publish_diagnostics();
    if (millis() - this->last_request_diagnostics_publish_ >= 60000) this->publish_request_diagnostics(true);
    /* we will send a packet to the AC as a reponse to indicate changes */
    send_packet();

    /* if there are no packets for 5 seconds - mark module as not ready */
    if (millis() - this->last_packet_received_ >= protocol::COMMUNICATION_TIMEOUT_MS)
    {
        if (this->state_ != ACState::Initializing)
        {
            this->state_ = ACState::Initializing;
            Component::status_set_error();
            if (this->communication_sensor_) this->communication_sensor_->publish_state(false);
            this->publish_protocol_state("timeout");
        }
    }
}

/*
 * ESPHome control request
 */

void SinclairACCNT::control(const climate::ClimateCall &call)
{
    if (!this->can_control()) {
        if (!this->transmit_warning_logged_) { ESP_LOGW(TAG, "Ignoring control request: protocol mode is not control"); this->transmit_warning_logged_ = true; }
        return;
    }
    if (this->state_ != ACState::Ready)
        return;

    if (call.get_mode().has_value())
    {
        ESP_LOGV(TAG, "Requested mode change");
        this->restart_pending_control(PENDING_MODE);
        ESP_LOGD(TAG, "Control request: mode=%u -> %u", static_cast<unsigned>(this->mode), static_cast<unsigned>(*call.get_mode()));
        this->pending_control_.mode = *call.get_mode();
    }

    if (call.get_target_temperature().has_value())
    {
        ESP_LOGV(TAG, "Requested target teperature change");
        this->restart_pending_control(PENDING_TARGET_TEMPERATURE);
        this->pending_control_.target_temperature = *call.get_target_temperature();
        if (this->pending_control_.target_temperature < MIN_TEMPERATURE)
        {
            this->pending_control_.target_temperature = MIN_TEMPERATURE;
        }
        else if (this->pending_control_.target_temperature > MAX_TEMPERATURE)
        {
            this->pending_control_.target_temperature = MAX_TEMPERATURE;
        }
    }

    if (call.has_custom_fan_mode())
    {
        ESP_LOGV(TAG, "Requested fan mode change");
        this->restart_pending_control(PENDING_FAN);
        this->pending_control_.custom_fan_mode = call.get_custom_fan_mode();
    }

    if (call.get_swing_mode().has_value())
    {
        ESP_LOGV(TAG, "Requested swing mode change");
        this->restart_pending_control(PENDING_VERTICAL_SWING | PENDING_HORIZONTAL_SWING);
        switch (*call.get_swing_mode()) {
            case climate::CLIMATE_SWING_BOTH:
                this->pending_control_.vertical_swing = vertical_swing_options::FULL;
                this->pending_control_.horizontal_swing = horizontal_swing_options::FULL;
                break;
            case climate::CLIMATE_SWING_OFF:
                /* both center */
                this->pending_control_.vertical_swing = vertical_swing_options::CMID;
                this->pending_control_.horizontal_swing = horizontal_swing_options::CMID;
                break;
            case climate::CLIMATE_SWING_VERTICAL:
                /* vertical full, horizontal center */
                this->pending_control_.vertical_swing = vertical_swing_options::FULL;
                this->pending_control_.horizontal_swing = horizontal_swing_options::CMID;
                break;
            case climate::CLIMATE_SWING_HORIZONTAL:
                /* horizontal full, vertical center */
                this->pending_control_.vertical_swing = vertical_swing_options::CMID;
                this->pending_control_.horizontal_swing = horizontal_swing_options::FULL;
                break;
            default:
                ESP_LOGV(TAG, "Unsupported swing mode requested");
                /* both center */
                this->pending_control_.vertical_swing = vertical_swing_options::CMID;
                this->pending_control_.horizontal_swing = horizontal_swing_options::CMID;
                break;
        }
    }
}

/*
 * Send a raw packet, as is
 */

std::vector<uint8_t> SinclairACCNT::build_poll_packet() const
{
    std::vector<uint8_t> packet(protocol::SET_PACKET_LEN, 0);
    if (!this->last_report_payload_.empty()) {
        std::copy_n(this->last_report_payload_.begin(), std::min(this->last_report_payload_.size(), packet.size()), packet.begin());
    }

    // Polls are deliberately raw report pass-throughs.  Do not call any state
    // encoder here: only the Wi-Fi protocol envelope belongs to this component.
    packet[protocol::SET_AF_BYTE] &= ~protocol::SET_AF_VAL;
    packet[protocol::SET_CONST_02_BYTE] = protocol::SET_CONST_02_VAL;
    packet[protocol::SET_CONST_BIT_BYTE] |= protocol::SET_CONST_BIT_MASK;
    packet[protocol::SET_NOCHANGE_BYTE] |= protocol::SET_NOCHANGE_MASK;
    return packet;
}

std::vector<uint8_t> SinclairACCNT::build_command_packet(ACUpdate update) const
{
    assert(update == ACUpdate::UpdateStart || update == ACUpdate::UpdateClear);
    std::vector<uint8_t> packet(protocol::SET_PACKET_LEN, 0);
    if (!this->last_report_payload_.empty()) {
        std::copy_n(this->last_report_payload_.begin(), std::min(this->last_report_payload_.size(), packet.size()), packet.begin());
    }

    packet[protocol::SET_CONST_02_BYTE] = protocol::SET_CONST_02_VAL;
    packet[protocol::SET_CONST_BIT_BYTE] |= protocol::SET_CONST_BIT_MASK;
    packet[protocol::SET_NOCHANGE_BYTE] &= ~protocol::SET_NOCHANGE_MASK;
    if (update == ACUpdate::UpdateStart) {
        packet[protocol::SET_AF_BYTE] = protocol::SET_AF_VAL;
    } else {
        packet[protocol::SET_AF_BYTE] &= ~protocol::SET_AF_VAL;
    }
    this->apply_requested_field_patches(packet);
    return packet;
}

void SinclairACCNT::apply_requested_field_patches(std::vector<uint8_t> &packet) const
{
    const auto fields = this->pending_control_.requested_fields;
    if (!this->pending_control_.active || fields == 0) return;
    const auto put = [&packet](size_t index, uint8_t mask, uint8_t value) {
        packet[index] = (packet[index] & ~mask) | (value & mask);
    };

    if (fields & PENDING_MODE) {
        uint8_t mode = protocol::REPORT_MODE_AUTO;
        bool power = true;
        switch (this->pending_control_.mode) {
            case climate::CLIMATE_MODE_COOL: mode = protocol::REPORT_MODE_COOL; break;
            case climate::CLIMATE_MODE_DRY: mode = protocol::REPORT_MODE_DRY; break;
            case climate::CLIMATE_MODE_FAN_ONLY: mode = protocol::REPORT_MODE_FAN; break;
            case climate::CLIMATE_MODE_HEAT: mode = protocol::REPORT_MODE_HEAT; break;
            case climate::CLIMATE_MODE_OFF: power = false; mode = (packet[protocol::REPORT_MODE_BYTE] & protocol::REPORT_MODE_MASK) >> protocol::REPORT_MODE_POS; break;
            default: break;
        }
        put(protocol::REPORT_MODE_BYTE, protocol::REPORT_PWR_MASK | protocol::REPORT_MODE_MASK,
            (power ? protocol::REPORT_PWR_MASK : 0) | (mode << protocol::REPORT_MODE_POS));
    }
    if (fields & PENDING_TARGET_TEMPERATURE) {
        const uint8_t temp = (static_cast<uint8_t>(this->pending_control_.target_temperature) - protocol::REPORT_TEMP_SET_OFF) << protocol::REPORT_TEMP_SET_POS;
        put(protocol::REPORT_TEMP_SET_BYTE, protocol::REPORT_TEMP_SET_MASK, temp);
    }
    if (fields & PENDING_FAN) {
        uint8_t speed1 = 0, speed2 = 0;
        bool quiet = false, turbo = false;
        const auto &fan = this->pending_control_.custom_fan_mode;
        if (fan == fan_modes::FAN_LOW) { speed1 = 1; speed2 = 1; }
        else if (fan == fan_modes::FAN_QUIET) { speed1 = 1; speed2 = 1; quiet = true; }
        else if (fan == fan_modes::FAN_MEDL) { speed1 = 2; speed2 = 2; }
        else if (fan == fan_modes::FAN_MED) { speed1 = 3; speed2 = 2; }
        else if (fan == fan_modes::FAN_MEDH) { speed1 = 4; speed2 = 3; }
        else if (fan == fan_modes::FAN_HIGH) { speed1 = 5; speed2 = 3; }
        else if (fan == fan_modes::FAN_TURBO) { speed1 = 5; speed2 = 3; turbo = true; }
        if (this->uses_gree_fan_layout()) put(protocol::REPORT_FAN_SPD2_BYTE, protocol::REPORT_GREE_FAN_MASK, speed2);
        else {
            put(protocol::REPORT_FAN_SPD1_BYTE, protocol::REPORT_FAN_SPD1_MASK, speed1);
            put(protocol::REPORT_FAN_SPD2_BYTE, protocol::REPORT_FAN_SPD2_MASK, speed2);
        }
        put(protocol::REPORT_FAN_QUIET_BYTE, protocol::REPORT_FAN_QUIET_MASK, quiet ? protocol::REPORT_FAN_QUIET_MASK : 0);
        put(protocol::REPORT_FAN_TURBO_BYTE, protocol::REPORT_FAN_TURBO_MASK, turbo ? protocol::REPORT_FAN_TURBO_MASK : 0);
    }
    const auto vertical_value = [this]() {
        const auto &v = this->pending_control_.vertical_swing;
        if (v == vertical_swing_options::FULL) return protocol::REPORT_VSWING_FULL;
        if (v == vertical_swing_options::DOWN) return protocol::REPORT_VSWING_DOWN;
        if (v == vertical_swing_options::MIDD) return protocol::REPORT_VSWING_MIDD;
        if (v == vertical_swing_options::MID) return protocol::REPORT_VSWING_MID;
        if (v == vertical_swing_options::MIDU) return protocol::REPORT_VSWING_MIDU;
        if (v == vertical_swing_options::UP) return protocol::REPORT_VSWING_UP;
        if (v == vertical_swing_options::CDOWN) return protocol::REPORT_VSWING_CDOWN;
        if (v == vertical_swing_options::CMIDD) return protocol::REPORT_VSWING_CMIDD;
        if (v == vertical_swing_options::CMID) return protocol::REPORT_VSWING_CMID;
        if (v == vertical_swing_options::CMIDU) return protocol::REPORT_VSWING_CMIDU;
        if (v == vertical_swing_options::CUP) return protocol::REPORT_VSWING_CUP;
        return protocol::REPORT_VSWING_OFF;
    };
    const auto horizontal_value = [this]() {
        const auto &v = this->pending_control_.horizontal_swing;
        if (v == horizontal_swing_options::FULL) return protocol::REPORT_HSWING_FULL;
        if (v == horizontal_swing_options::CLEFT) return protocol::REPORT_HSWING_CLEFT;
        if (v == horizontal_swing_options::CMIDL) return protocol::REPORT_HSWING_CMIDL;
        if (v == horizontal_swing_options::CMID) return protocol::REPORT_HSWING_CMID;
        if (v == horizontal_swing_options::CMIDR) return protocol::REPORT_HSWING_CMIDR;
        if (v == horizontal_swing_options::CRIGHT) return protocol::REPORT_HSWING_CRIGHT;
        return protocol::REPORT_HSWING_OFF;
    };
    if (fields & PENDING_VERTICAL_SWING) put(protocol::REPORT_VSWING_BYTE, protocol::REPORT_VSWING_MASK, vertical_value() << protocol::REPORT_VSWING_POS);
    if (fields & PENDING_HORIZONTAL_SWING) put(protocol::REPORT_HSWING_BYTE, protocol::REPORT_HSWING_MASK, horizontal_value() << protocol::REPORT_HSWING_POS);
    if (fields & PENDING_DISPLAY) {
        uint8_t mode = (packet[protocol::REPORT_DISP_MODE_BYTE] & protocol::REPORT_DISP_MODE_MASK) >> protocol::REPORT_DISP_MODE_POS;
        bool power = this->pending_control_.display_mode != display_options::OFF;
        const auto &display = this->pending_control_.display_mode;
        if (display == display_options::AUTO) mode = protocol::REPORT_DISP_MODE_AUTO;
        else if (display == display_options::SET) mode = protocol::REPORT_DISP_MODE_SET;
        else if (display == display_options::ACT) mode = protocol::REPORT_DISP_MODE_ACT;
        else if (display == display_options::OUT) mode = protocol::REPORT_DISP_MODE_OUT;
        put(protocol::REPORT_DISP_MODE_BYTE, protocol::REPORT_DISP_MODE_MASK, mode << protocol::REPORT_DISP_MODE_POS);
        put(protocol::REPORT_DISP_ON_BYTE, protocol::REPORT_DISP_ON_MASK, power ? protocol::REPORT_DISP_ON_MASK : 0);
    }
    if (fields & PENDING_DISPLAY_UNIT) put(protocol::REPORT_DISP_F_BYTE, protocol::REPORT_DISP_F_MASK,
                                            this->pending_control_.display_unit == display_unit_options::DEGF ? protocol::REPORT_DISP_F_MASK : 0);
    if (fields & PENDING_PLASMA) {
        const uint8_t value = this->pending_control_.plasma ? 0xFF : 0;
        put(protocol::REPORT_PLASMA1_BYTE, protocol::REPORT_PLASMA1_MASK, value);
        put(protocol::REPORT_PLASMA2_BYTE, protocol::REPORT_PLASMA2_MASK, value);
    }
    if (fields & PENDING_SLEEP) put(protocol::REPORT_SLEEP_BYTE, protocol::REPORT_SLEEP_MASK, this->pending_control_.sleep ? protocol::REPORT_SLEEP_MASK : 0);
    if (fields & PENDING_XFAN) put(protocol::REPORT_XFAN_BYTE, protocol::REPORT_XFAN_MASK, this->pending_control_.xfan ? protocol::REPORT_XFAN_MASK : 0);
    if (fields & PENDING_SAVE) put(protocol::REPORT_SAVE_BYTE, protocol::REPORT_SAVE_MASK, this->pending_control_.save ? protocol::REPORT_SAVE_MASK : 0);
}

uint8_t SinclairACCNT::allowed_command_mask(size_t index, uint16_t fields, bool gree) const
{
    uint8_t mask = 0;
    if (fields & PENDING_MODE && index == protocol::REPORT_MODE_BYTE) mask |= protocol::REPORT_PWR_MASK | protocol::REPORT_MODE_MASK;
    if (fields & PENDING_TARGET_TEMPERATURE && index == protocol::REPORT_TEMP_SET_BYTE) mask |= protocol::REPORT_TEMP_SET_MASK;
    if (fields & PENDING_FAN) {
        if (index == protocol::REPORT_FAN_SPD2_BYTE) mask |= gree ? protocol::REPORT_GREE_FAN_MASK : protocol::REPORT_FAN_SPD2_MASK;
        if (!gree && index == protocol::REPORT_FAN_SPD1_BYTE) mask |= protocol::REPORT_FAN_SPD1_MASK;
        if (index == protocol::REPORT_FAN_QUIET_BYTE) mask |= protocol::REPORT_FAN_QUIET_MASK;
        if (index == protocol::REPORT_FAN_TURBO_BYTE) mask |= protocol::REPORT_FAN_TURBO_MASK;
    }
    if (fields & PENDING_VERTICAL_SWING && index == protocol::REPORT_VSWING_BYTE) mask |= protocol::REPORT_VSWING_MASK;
    if (fields & PENDING_HORIZONTAL_SWING && index == protocol::REPORT_HSWING_BYTE) mask |= protocol::REPORT_HSWING_MASK;
    if (fields & PENDING_DISPLAY) { if (index == protocol::REPORT_DISP_MODE_BYTE) mask |= protocol::REPORT_DISP_MODE_MASK; if (index == protocol::REPORT_DISP_ON_BYTE) mask |= protocol::REPORT_DISP_ON_MASK; }
    if (fields & PENDING_DISPLAY_UNIT && index == protocol::REPORT_DISP_F_BYTE) mask |= protocol::REPORT_DISP_F_MASK;
    if (fields & PENDING_PLASMA) { if (index == protocol::REPORT_PLASMA1_BYTE) mask |= protocol::REPORT_PLASMA1_MASK; if (index == protocol::REPORT_PLASMA2_BYTE) mask |= protocol::REPORT_PLASMA2_MASK; }
    if (fields & PENDING_SLEEP && index == protocol::REPORT_SLEEP_BYTE) mask |= protocol::REPORT_SLEEP_MASK;
    if (fields & PENDING_XFAN && index == protocol::REPORT_XFAN_BYTE) mask |= protocol::REPORT_XFAN_MASK;
    if (fields & PENDING_SAVE && index == protocol::REPORT_SAVE_BYTE) mask |= protocol::REPORT_SAVE_MASK;
    return mask;
}

uint8_t SinclairACCNT::allowed_envelope_mask(size_t index, ACUpdate update) const
{
    if (index == protocol::SET_CONST_02_BYTE) return 0xFF;
    if (index == protocol::SET_CONST_BIT_BYTE) return protocol::SET_CONST_BIT_MASK;
    if (index == protocol::SET_NOCHANGE_BYTE) return protocol::SET_NOCHANGE_MASK;
    if (index == protocol::SET_AF_BYTE) return update == ACUpdate::UpdateStart ? 0xFF : protocol::SET_AF_VAL;
    return 0;
}

void SinclairACCNT::verify_packet_changes(const std::vector<uint8_t> &packet, ACUpdate update) const
{
    if (this->last_report_payload_.empty()) return;
    const uint16_t fields = update == ACUpdate::NoUpdate ? 0 : this->pending_control_.requested_fields;
    for (size_t i = 0; i < packet.size() && i < this->last_report_payload_.size(); ++i) {
        const uint8_t allowed = this->allowed_envelope_mask(i, update) | this->allowed_command_mask(i, fields, this->uses_gree_fan_layout());
        const uint8_t unexpected = (packet[i] ^ this->last_report_payload_[i]) & ~allowed;
        if (unexpected != 0) ESP_LOGW(TAG, "TX invariant: payload[%u] changed outside mask 0x%02X (delta 0x%02X)", static_cast<unsigned>(i), allowed, unexpected);
    }
}

void SinclairACCNT::send_packet()
{
    if (this->is_receive_only()) return;
    if (this->is_poll_only()) this->update_ = ACUpdate::NoUpdate;
    const auto expired = this->request_lifecycle_.timeout(millis(), protocol::POLL_RESPONSE_TIMEOUT_MS);
    if (expired != OutstandingRequest::NONE) {
        this->wait_response_ = false;
        if (expired == OutstandingRequest::POLL) { ESP_LOGW(TAG, "Poll response timed out"); this->publish_protocol_state("response_timeout"); }
        else if (++this->pending_control_.retries >= 3) { this->pending_control_.active = false; this->update_ = ACUpdate::NoUpdate; this->last_command_result_ = "failed"; this->last_command_failure_reason_ = "timeout"; this->publish_protocol_state("command_failed_timeout"); }
        else { this->update_ = expired == OutstandingRequest::COMMAND_CLEAR ? ACUpdate::UpdateClear : ACUpdate::UpdateStart; this->last_command_result_ = "retrying"; }
        this->publish_request_diagnostics();
    }
    if (!this->request_lifecycle_.may_send() || millis() - this->last_packet_sent_ < protocol::TIME_REFRESH_PERIOD_MS) return;

    const ACUpdate update = this->update_;
    std::vector<uint8_t> packet = update == ACUpdate::NoUpdate ? this->build_poll_packet() : this->build_command_packet(update);
    this->verify_packet_changes(packet, update);
    if (update == ACUpdate::NoUpdate) this->verify_no_change_packet(packet);
    else {
        ESP_LOGD(TAG, "Command build: fields=0x%03X envelope=%s", this->pending_control_.requested_fields, update == ACUpdate::UpdateStart ? "apply" : "clear");
        for (size_t i = 0; i < packet.size() && i < this->last_report_payload_.size(); ++i) if (packet[i] != this->last_report_payload_[i]) ESP_LOGD(TAG, "TX build changed payload[%u]: 0x%02X -> 0x%02X", static_cast<unsigned>(i), this->last_report_payload_[i], packet[i]);
    }

    packet.insert(packet.begin(), protocol::CMD_OUT_PARAMS_SET);
    packet.insert(packet.begin(), protocol::SET_PACKET_LEN + 2);
    uint8_t checksum = 0;
    for (const auto byte : packet) checksum += byte;
    packet.push_back(checksum);
    packet.insert(packet.begin(), protocol::SYNC);
    packet.insert(packet.begin(), protocol::SYNC);
    this->last_packet_sent_ = millis();
    const OutstandingRequest request = update == ACUpdate::NoUpdate ? OutstandingRequest::POLL : update == ACUpdate::UpdateStart ? OutstandingRequest::COMMAND_APPLY : OutstandingRequest::COMMAND_CLEAR;
    this->request_lifecycle_.sent(request, this->last_packet_sent_);
    this->publish_request_diagnostics();
    this->wait_response_ = true;
    write_array(packet);
    this->record_transmitted_packet(packet);
    log_packet(packet, true);
    const char *protocol_state = protocol_state_after_transmit(update, this->state_);
    if (protocol_state != nullptr) this->publish_protocol_state(protocol_state);
}

/*
 * Packet handling
 */

SinclairACCNT::PacketValidationResult SinclairACCNT::verify_packet()
{
    sinclair_ac_protocol::ParsedFrame frame;
    const auto result = sinclair_ac_protocol::parse(this->serialProcess_.data, frame);
    switch (result) {
        case sinclair_ac_protocol::Result::VALID_KNOWN: return PacketValidationResult::VALID_KNOWN;
        case sinclair_ac_protocol::Result::VALID_UNKNOWN: return PacketValidationResult::VALID_UNKNOWN;
        case sinclair_ac_protocol::Result::TOO_SHORT:
            this->too_short_frames_++;
            ESP_LOGW(TAG, "Dropping invalid packet: too short");
            return PacketValidationResult::INVALID_TOO_SHORT;
        case sinclair_ac_protocol::Result::LENGTH:
            this->invalid_lengths_++;
            ESP_LOGW(TAG, "Dropping invalid packet: declared length %u, actual %u", frame.declared_length, this->serialProcess_.data.size());
            return PacketValidationResult::INVALID_LENGTH;
        case sinclair_ac_protocol::Result::CHECKSUM:
            this->checksum_failures_++;
            ESP_LOGW(TAG, "Dropping invalid packet: calculated checksum 0x%02X, received 0x%02X", frame.calculated_checksum, frame.received_checksum);
            return PacketValidationResult::INVALID_CHECKSUM;
    }
    return PacketValidationResult::INVALID_TOO_SHORT;
}

void SinclairACCNT::handle_packet()
{
    if (this->serialProcess_.data[3] == protocol::CMD_IN_UNIT_REPORT)
    {
        const size_t payload_size = this->serialProcess_.data.size() - 5;
        if (payload_size <= protocol::REPORT_TEMP_ACT_BYTE) {
            ESP_LOGW(TAG, "Ignoring unsupported short 0x31 report (payload %u; need at least %u)", payload_size, protocol::REPORT_TEMP_ACT_BYTE + 1);
            return;
        }
        std::vector<uint8_t> payload(this->serialProcess_.data.begin() + 4, this->serialProcess_.data.end() - 1);
        if (this->processUnitReport(payload)) this->publish_state();
    }
    else 
    {
        ESP_LOGD(TAG, "Received unknown packet");
    }
}

bool SinclairACCNT::uses_gree_fan_layout() const
{
    return this->fan_profile_ == FanProfile::GREE_4_SPEED ||
           (this->fan_profile_ == FanProfile::AUTO && this->gree_fan_layout_detected_);
}

void SinclairACCNT::verify_no_change_packet(const std::vector<uint8_t> &packet) const
{
#ifndef NDEBUG
    if (this->last_report_payload_.empty()) return;
    assert((packet[protocol::SET_NOCHANGE_BYTE] & protocol::SET_NOCHANGE_MASK) != 0);
    for (size_t i = 0; i < packet.size() && i < this->last_report_payload_.size(); ++i) {
        const uint8_t unexpected = (packet[i] ^ this->last_report_payload_[i]) & ~this->allowed_envelope_mask(i, ACUpdate::NoUpdate);
        assert(unexpected == 0);
    }
#else
    (void) packet;
#endif
}

/*
 * This decodes frame recieved from AC Unit
 */
bool SinclairACCNT::processUnitReport(const std::vector<uint8_t> &payload)
{
    bool hasChanged = false;
    this->last_report_payload_ = payload;
    if (payload.size() > 44) {
        const bool refresh_due = millis() - this->last_candidate_telemetry_byte_44_publish_ >= 60000;
        const bool changed = !this->has_published_candidate_telemetry_byte_44_ || payload[44] != this->published_candidate_telemetry_byte_44_;
        if (changed || refresh_due) {
            if (this->candidate_telemetry_byte_44_raw_sensor_) this->candidate_telemetry_byte_44_raw_sensor_->publish_state(payload[44]);
        // Diagnostic-only and opt-in.  Byte 44 has no assigned physical meaning.
            if (this->candidate_byte_44_temperature_hypothesis_sensor_) this->candidate_byte_44_temperature_hypothesis_sensor_->publish_state((payload[44] - 16) / 2.0f);
            this->published_candidate_telemetry_byte_44_ = payload[44];
            this->has_published_candidate_telemetry_byte_44_ = true;
            this->last_candidate_telemetry_byte_44_publish_ = millis();
        }
    }
    if (this->fan_profile_ == FanProfile::AUTO && !this->gree_fan_layout_detected_ &&
        payload.size() > protocol::REPORT_FAN_SPD1_BYTE &&
        payload[protocol::REPORT_FAN_SPD1_BYTE] == 0x08 &&
        (payload[protocol::REPORT_FAN_SPD2_BYTE] & protocol::REPORT_GREE_FAN_MASK) <= protocol::REPORT_GREE_FAN_HIGH) {
        this->gree_fan_layout_detected_ = true;
        ESP_LOGD(TAG, "Detected persistent Gree four-speed fan layout from valid report");
    }
    this->report_payload_ = &payload;

    climate::ClimateMode newMode = determine_mode();
    if (this->mode != newMode) hasChanged = true;
    this->mode = newMode;

    const char* newFanMode = determine_fan_mode();
    if (this->has_custom_fan_mode())
    {
        if (strcmp(this->get_custom_fan_mode().c_str(), newFanMode) != 0) hasChanged = true;
    }
    else
    {
        hasChanged = true;
    }
    this->set_custom_fan_mode_(newFanMode);
    
    float newTargetTemperature = (float)((((*this->report_payload_)[protocol::REPORT_TEMP_SET_BYTE] & protocol::REPORT_TEMP_SET_MASK) >> protocol::REPORT_TEMP_SET_POS)
        + protocol::REPORT_TEMP_SET_OFF);
    if (this->target_temperature != newTargetTemperature) hasChanged = true;
    this->update_target_temperature(newTargetTemperature);
    
    /* if there is no external sensor mapped to represent current temperature we will get data from AC unit */
    if (this->current_temperature_sensor_ == nullptr)
    {
        float newCurrentTemperature = (float)((((*this->report_payload_)[protocol::REPORT_TEMP_ACT_BYTE] & protocol::REPORT_TEMP_ACT_MASK) >> protocol::REPORT_TEMP_ACT_POS)
            - protocol::REPORT_TEMP_ACT_OFF) / protocol::REPORT_TEMP_ACT_DIV;
        if (this->current_temperature != newCurrentTemperature) hasChanged = true;
        this->update_current_temperature(newCurrentTemperature);
    }

    std::string verticalSwing = determine_vertical_swing();
    std::string horizontalSwing = determine_horizontal_swing();

    this->update_swing_vertical(verticalSwing);
    this->update_swing_horizontal(horizontalSwing);

    climate::ClimateSwingMode newSwingMode;
    /* update legacy swing mode to somehow represent actual state and support
       this setting without detailed settings done with additional switches */
    if (verticalSwing == vertical_swing_options::FULL && horizontalSwing == horizontal_swing_options::FULL)
        newSwingMode = climate::CLIMATE_SWING_BOTH;
    else if (verticalSwing == vertical_swing_options::FULL)
        newSwingMode = climate::CLIMATE_SWING_VERTICAL;
    else if (horizontalSwing == horizontal_swing_options::FULL)
        newSwingMode = climate::CLIMATE_SWING_HORIZONTAL;
    else
        newSwingMode = climate::CLIMATE_SWING_OFF;
    
    if (this->swing_mode != newSwingMode) hasChanged = true;
    this->swing_mode = newSwingMode;

    this->update_display(determine_display());
    this->update_display_unit(determine_display_unit());

    this->update_plasma(determine_plasma());
    this->update_sleep(determine_sleep());
    this->update_xfan(determine_xfan());
    this->update_save(determine_save());

    this->report_payload_ = nullptr;
    return hasChanged;
}

climate::ClimateMode SinclairACCNT::determine_mode()
{
    uint8_t mode = ((*this->report_payload_)[protocol::REPORT_MODE_BYTE] & protocol::REPORT_MODE_MASK) >> protocol::REPORT_MODE_POS;

    /* as mode presented by climate component incorporates both power and mode we will store this separately for Sinclair
       in _internal_ fields */
    /* check unit power flag */
    this->power_internal_ = ((*this->report_payload_)[protocol::REPORT_PWR_BYTE] & protocol::REPORT_PWR_MASK) != 0;

    /* check unit mode */
    switch (mode)
    {
        case protocol::REPORT_MODE_AUTO:
            this->mode_internal_ = climate::CLIMATE_MODE_AUTO;
            break;
        case protocol::REPORT_MODE_COOL:
            this->mode_internal_ = climate::CLIMATE_MODE_COOL;
            break;
        case protocol::REPORT_MODE_DRY:
            this->mode_internal_ = climate::CLIMATE_MODE_DRY;
            break;
        case protocol::REPORT_MODE_FAN:
            this->mode_internal_ = climate::CLIMATE_MODE_FAN_ONLY;
            break;
        case protocol::REPORT_MODE_HEAT:
            this->mode_internal_ = climate::CLIMATE_MODE_HEAT;
            break;
        default:
            ESP_LOGW(TAG, "Received unknown climate mode");
            this->mode_internal_ = climate::CLIMATE_MODE_OFF;
            break;
    }

    /* if unit is powered on - return the mode, otherwise return CLIMATE_MODE_OFF */
    if (this->power_internal_)
    {
        return this->mode_internal_;
    }
    else
    {
        return climate::CLIMATE_MODE_OFF;
    }
}

const char* SinclairACCNT::determine_fan_mode()
{
    /* fan setting has quite complex representation in the packet, brace for it */
    const uint8_t fan_speed1_raw = (*this->report_payload_)[protocol::REPORT_FAN_SPD1_BYTE];
    const uint8_t fan_speed2_raw = (*this->report_payload_)[protocol::REPORT_FAN_SPD2_BYTE];
    const uint8_t fanSpeed1 = (fan_speed1_raw & protocol::REPORT_FAN_SPD1_MASK) >> protocol::REPORT_FAN_SPD1_POS;
    const uint8_t fanSpeed2 = (fan_speed2_raw & protocol::REPORT_FAN_SPD2_MASK) >> protocol::REPORT_FAN_SPD2_POS;
    const bool fanQuiet = ((*this->report_payload_)[protocol::REPORT_FAN_QUIET_BYTE] & protocol::REPORT_FAN_QUIET_MASK) != 0;
    const bool fanTurbo = ((*this->report_payload_)[protocol::REPORT_FAN_TURBO_BYTE] & protocol::REPORT_FAN_TURBO_MASK) != 0;
    const char *fan_mode = fan_modes::FAN_AUTO;
    const char *decode_status = "unknown";
    const bool gree_profile = this->uses_gree_fan_layout();
    const uint8_t gree_fan = fan_speed2_raw & protocol::REPORT_GREE_FAN_MASK;
    /* Gree Livo packs its active fan setting in byte 4's low two bits. */
    if (gree_profile) {
        switch (gree_fan) {
            case protocol::REPORT_GREE_FAN_AUTO: fan_mode = fan_modes::FAN_AUTO; decode_status = "gree_4_speed:auto"; break;
            case protocol::REPORT_GREE_FAN_LOW: fan_mode = fan_modes::FAN_LOW; decode_status = "gree_4_speed:low"; break;
            case protocol::REPORT_GREE_FAN_MED: fan_mode = fan_modes::FAN_MED; decode_status = "gree_4_speed:medium"; break;
            case protocol::REPORT_GREE_FAN_HIGH: fan_mode = fan_modes::FAN_HIGH; decode_status = "gree_4_speed:high"; break;
            default: decode_status = "gree_4_speed:unknown"; break;
        }
        if (fanQuiet) { fan_mode = fan_modes::FAN_QUIET; decode_status = "gree_4_speed:quiet"; }
        if (fanTurbo) { fan_mode = fan_modes::FAN_TURBO; decode_status = "gree_4_speed:turbo"; }
        this->record_fan_diagnostics(fan_speed1_raw, fan_speed1_raw & 0x07, fan_speed2_raw, fanQuiet, fanTurbo, decode_status);
        return fan_mode;
    }
    /* Sinclair extended dual-field mapping. */
    if      (fanSpeed1 == 0 && fanSpeed2 == 0 && fanQuiet == false && fanTurbo == false)
    {
        fan_mode = fan_modes::FAN_AUTO;
        decode_status = "auto";
    }
    else if (fanSpeed1 == 1 && fanSpeed2 == 1 && fanQuiet == false && fanTurbo == false)
    {
        fan_mode = fan_modes::FAN_LOW;
        decode_status = "low";
    }
    else if (fanSpeed1 == 1 && fanSpeed2 == 1 && fanQuiet == true  && fanTurbo == false)
    {
        fan_mode = fan_modes::FAN_QUIET;
        decode_status = "quiet";
    }
    else if (fanSpeed1 == 2 && fanSpeed2 == 2 && fanQuiet == false && fanTurbo == false)
    {
        fan_mode = fan_modes::FAN_MEDL;
        decode_status = "medium_low";
    }
    else if (fanSpeed1 == 3 && fanSpeed2 == 2 && fanQuiet == false && fanTurbo == false)
    {
        fan_mode = fan_modes::FAN_MED;
        decode_status = "medium";
    }
    else if (fanSpeed1 == 4 && fanSpeed2 == 3 && fanQuiet == false && fanTurbo == false)
    {
        fan_mode = fan_modes::FAN_MEDH;
        decode_status = "medium_high";
    }
    else if (fanSpeed1 == 5 && fanSpeed2 == 3 && fanQuiet == false && fanTurbo == false)
    {
        fan_mode = fan_modes::FAN_HIGH;
        decode_status = "high";
    }
    else if (fanSpeed1 == 5 && fanSpeed2 == 3 && fanQuiet == false && fanTurbo == true )
    {
        fan_mode = fan_modes::FAN_TURBO;
        decode_status = "turbo";
    }
    else 
    {
        const uint32_t signature = (static_cast<uint32_t>(fan_speed1_raw) << 24) |
                                   (static_cast<uint32_t>(fan_speed2_raw) << 16) |
                                   (static_cast<uint32_t>(fanQuiet) << 8) | static_cast<uint32_t>(fanTurbo);
        if (!this->has_unknown_fan_signature_ || this->last_unknown_fan_signature_ != signature ||
            millis() - this->last_unknown_fan_warning_ >= 5000) {
            ESP_LOGW(TAG, "Unknown fan mode: speed1_raw=0x%02X speed1_4bit=%u speed1_3bit=%u speed2_raw=0x%02X speed2=%u quiet=%s turbo=%s",
                     fan_speed1_raw, fanSpeed1, fan_speed1_raw & 0x07, fan_speed2_raw, fanSpeed2,
                     fanQuiet ? "true" : "false", fanTurbo ? "true" : "false");
            this->last_unknown_fan_warning_ = millis();
            this->last_unknown_fan_signature_ = signature;
            this->has_unknown_fan_signature_ = true;
        }
    }
    this->record_fan_diagnostics(fan_speed1_raw, fan_speed1_raw & 0x07, fan_speed2_raw, fanQuiet, fanTurbo, decode_status);
    return fan_mode;
}

std::string SinclairACCNT::determine_vertical_swing()
{
    uint8_t mode = ((*this->report_payload_)[protocol::REPORT_VSWING_BYTE]  & protocol::REPORT_VSWING_MASK) >> protocol::REPORT_VSWING_POS;

    switch (mode) {
        case protocol::REPORT_VSWING_OFF:
            return vertical_swing_options::OFF;
        case protocol::REPORT_VSWING_FULL:
            return vertical_swing_options::FULL;
        case protocol::REPORT_VSWING_DOWN:
            return vertical_swing_options::DOWN;
        case protocol::REPORT_VSWING_MIDD:
            return vertical_swing_options::MIDD;
        case protocol::REPORT_VSWING_MID:
            return vertical_swing_options::MID;
        case protocol::REPORT_VSWING_MIDU:
            return vertical_swing_options::MIDU;
        case protocol::REPORT_VSWING_UP:
            return vertical_swing_options::UP;
        case protocol::REPORT_VSWING_CDOWN:
            return vertical_swing_options::CDOWN;
        case protocol::REPORT_VSWING_CMIDD:
            return vertical_swing_options::CMIDD;
        case protocol::REPORT_VSWING_CMID:
            return vertical_swing_options::CMID;
        case protocol::REPORT_VSWING_CMIDU:
            return vertical_swing_options::CMIDU;
        case protocol::REPORT_VSWING_CUP:
            return vertical_swing_options::CUP;
        default:
            ESP_LOGW(TAG, "Received unknown vertical swing mode");
            return vertical_swing_options::OFF;;
    }
}

std::string SinclairACCNT::determine_horizontal_swing()
{
    uint8_t mode = ((*this->report_payload_)[protocol::REPORT_HSWING_BYTE]  & protocol::REPORT_HSWING_MASK) >> protocol::REPORT_HSWING_POS;

    switch (mode) {
        case protocol::REPORT_HSWING_OFF:
            return horizontal_swing_options::OFF;
        case protocol::REPORT_HSWING_FULL:
            return horizontal_swing_options::FULL;
        case protocol::REPORT_HSWING_CLEFT:
            return horizontal_swing_options::CLEFT;
        case protocol::REPORT_HSWING_CMIDL:
            return horizontal_swing_options::CMIDL;
        case protocol::REPORT_HSWING_CMID:
            return horizontal_swing_options::CMID;
        case protocol::REPORT_HSWING_CMIDR:
            return horizontal_swing_options::CMIDR;
        case protocol::REPORT_HSWING_CRIGHT:
            return horizontal_swing_options::CRIGHT;
        default:
            ESP_LOGW(TAG, "Received unknown horizontal swing mode");
            return horizontal_swing_options::OFF;
    }
}

std::string SinclairACCNT::determine_display()
{
    uint8_t mode = ((*this->report_payload_)[protocol::REPORT_DISP_MODE_BYTE] & protocol::REPORT_DISP_MODE_MASK) >> protocol::REPORT_DISP_MODE_POS;

    this->display_power_internal_ = ((*this->report_payload_)[protocol::REPORT_DISP_ON_BYTE] & protocol::REPORT_DISP_ON_MASK);

    switch (mode) {
        case protocol::REPORT_DISP_MODE_AUTO:
            this->display_mode_internal_ = display_options::AUTO;
            break;
        case protocol::REPORT_DISP_MODE_SET:
            this->display_mode_internal_ = display_options::SET;
            break;
        case protocol::REPORT_DISP_MODE_ACT:
            this->display_mode_internal_ = display_options::ACT;
            break;
        case protocol::REPORT_DISP_MODE_OUT:
            this->display_mode_internal_ = display_options::OUT;
            break;
        default:
            ESP_LOGW(TAG, "Received unknown display mode");
            this->display_mode_internal_ = display_options::AUTO;
            break;
    }

    if (this->display_power_internal_)
    {
        return this->display_mode_internal_;
    }
    else
    {
        return display_options::OFF;
    }
}

std::string SinclairACCNT::determine_display_unit()
{
    if ((*this->report_payload_)[protocol::REPORT_DISP_F_BYTE] & protocol::REPORT_DISP_F_MASK)
    {
        return display_unit_options::DEGF;
    }
    else
    {
        return display_unit_options::DEGC;
    }
}

bool SinclairACCNT::determine_plasma(){
    bool plasma1 = ((*this->report_payload_)[protocol::REPORT_PLASMA1_BYTE] & protocol::REPORT_PLASMA1_MASK) != 0;
    bool plasma2 = ((*this->report_payload_)[protocol::REPORT_PLASMA2_BYTE] & protocol::REPORT_PLASMA2_MASK) != 0;
    return plasma1 || plasma2;
}

bool SinclairACCNT::determine_sleep(){
    return ((*this->report_payload_)[protocol::REPORT_SLEEP_BYTE] & protocol::REPORT_SLEEP_MASK) != 0;
}

bool SinclairACCNT::determine_xfan(){
    return ((*this->report_payload_)[protocol::REPORT_XFAN_BYTE] & protocol::REPORT_XFAN_MASK) != 0;
}

bool SinclairACCNT::determine_save(){
    return ((*this->report_payload_)[protocol::REPORT_SAVE_BYTE] & protocol::REPORT_SAVE_MASK) != 0;
}


/*
 * Sensor handling
 */

void SinclairACCNT::on_vertical_swing_change(const std::string &swing)
{
    if (!this->can_control()) return;
    if (this->state_ != ACState::Ready)
        return;

    ESP_LOGD(TAG, "Setting vertical swing position");

    this->restart_pending_control(PENDING_VERTICAL_SWING);
    this->pending_control_.vertical_swing = swing;
}

void SinclairACCNT::on_horizontal_swing_change(const std::string &swing)
{
    if (!this->can_control()) return;
    if (this->state_ != ACState::Ready)
        return;

    ESP_LOGD(TAG, "Setting horizontal swing position");

    this->restart_pending_control(PENDING_HORIZONTAL_SWING);
    this->pending_control_.horizontal_swing = swing;
}

void SinclairACCNT::on_display_change(const std::string &display)
{
    if (!this->can_control()) return;
    if (this->state_ != ACState::Ready)
        return;

    ESP_LOGD(TAG, "Setting display mode");

    this->restart_pending_control(PENDING_DISPLAY);
    this->pending_control_.display_mode = display;
}

void SinclairACCNT::on_display_unit_change(const std::string &display_unit)
{
    if (!this->can_control()) return;
    if (this->state_ != ACState::Ready)
        return;

    ESP_LOGD(TAG, "Setting display unit");

    this->restart_pending_control(PENDING_DISPLAY_UNIT);
    this->pending_control_.display_unit = display_unit;
}

void SinclairACCNT::on_plasma_change(bool plasma)
{
    if (!this->can_control()) return;
    if (this->state_ != ACState::Ready)
        return;

    ESP_LOGD(TAG, "Setting plasma");

    this->restart_pending_control(PENDING_PLASMA);
    this->pending_control_.plasma = plasma;
}

void SinclairACCNT::on_sleep_change(bool sleep)
{
    if (!this->can_control()) return;
    if (this->state_ != ACState::Ready)
        return;

    ESP_LOGD(TAG, "Setting sleep");

    this->restart_pending_control(PENDING_SLEEP);
    this->pending_control_.sleep = sleep;
}

void SinclairACCNT::on_xfan_change(bool xfan)
{
    if (!this->can_control()) return;
    if (this->state_ != ACState::Ready)
        return;

    ESP_LOGD(TAG, "Setting xfan");

    this->restart_pending_control(PENDING_XFAN);
    this->pending_control_.xfan = xfan;
}

void SinclairACCNT::on_save_change(bool save)
{
    if (!this->can_control()) return;
    if (this->state_ != ACState::Ready)
        return;

    ESP_LOGD(TAG, "Setting save");

    this->restart_pending_control(PENDING_SAVE);
    this->pending_control_.save = save;
}

}  // namespace CNT
}  // namespace sinclair_ac
}  // namespace esphome
