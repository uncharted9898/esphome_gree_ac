// based on: https://github.com/DomiStyle/esphome-panasonic-ac
#include "esppac_cnt.h"
#include "protocol_frame.h"

namespace esphome {
namespace sinclair_ac {
namespace CNT {

static const char *const TAG = "sinclair_ac.serial";

void SinclairACCNT::setup()
{
    SinclairAC::setup();

    ESP_LOGD(TAG, "Using serial protocol for Sinclair AC");
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
        this->record_received_packet(known);
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

        if (this->wait_response_) {
            this->wait_response_ = false;
            if (this->update_ == ACUpdate::UpdateStart) {
                this->update_ = ACUpdate::UpdateClear;
                this->publish_protocol_state("command_clear_pending");
            } else if (this->update_ == ACUpdate::UpdateClear) {
                this->update_ = ACUpdate::NoUpdate;
                this->publish_protocol_state("command_verified");
            }
        }
        handle_packet(); /* Reports are acknowledgements as well as state updates. */
        }
        this->reset_parser();
    }  // closes validation else
    }  // closes: if (serialProcess_.state == STATE_COMPLETE)

    this->publish_diagnostics();
    /* we will send a packet to the AC as a reponse to indicate changes */
    send_packet();

    /* if there are no packets for 5 seconds - mark module as not ready */
    if (millis() - this->last_packet_received_ >= protocol::TIME_TIMEOUT_INACTIVE_MS)
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
        this->update_ = ACUpdate::UpdateStart;
        this->mode = *call.get_mode();
    }

    if (call.get_target_temperature().has_value())
    {
        ESP_LOGV(TAG, "Requested target teperature change");
        this->update_ = ACUpdate::UpdateStart;
        this->target_temperature = *call.get_target_temperature();
        if (this->target_temperature < MIN_TEMPERATURE)
        {
            this->target_temperature = MIN_TEMPERATURE;
        }
        else if (this->target_temperature > MAX_TEMPERATURE)
        {
            this->target_temperature = MAX_TEMPERATURE;
        }
    }

    if (call.has_custom_fan_mode())
    {
        ESP_LOGV(TAG, "Requested fan mode change");
        this->update_ = ACUpdate::UpdateStart;
        this->set_custom_fan_mode_(call.get_custom_fan_mode());
    }

    if (call.get_swing_mode().has_value())
    {
        ESP_LOGV(TAG, "Requested swing mode change");
        this->update_ = ACUpdate::UpdateStart;
        switch (*call.get_swing_mode()) {
            case climate::CLIMATE_SWING_BOTH:
                this->vertical_swing_state_   =   vertical_swing_options::FULL;
                this->horizontal_swing_state_ = horizontal_swing_options::FULL;
                break;
            case climate::CLIMATE_SWING_OFF:
                /* both center */
                this->vertical_swing_state_   =   vertical_swing_options::CMID;
                this->horizontal_swing_state_ = horizontal_swing_options::CMID;
                break;
            case climate::CLIMATE_SWING_VERTICAL:
                /* vertical full, horizontal center */
                this->vertical_swing_state_   =   vertical_swing_options::FULL;
                this->horizontal_swing_state_ = horizontal_swing_options::CMID;
                break;
            case climate::CLIMATE_SWING_HORIZONTAL:
                /* horizontal full, vertical center */
                this->vertical_swing_state_   =   vertical_swing_options::CMID;
                this->horizontal_swing_state_ = horizontal_swing_options::FULL;
                break;
            default:
                ESP_LOGV(TAG, "Unsupported swing mode requested");
                /* both center */
                this->vertical_swing_state_   =   vertical_swing_options::CMID;
                this->horizontal_swing_state_ = horizontal_swing_options::CMID;
                break;
        }
    }
}

/*
 * Send a raw packet, as is
 */

void SinclairACCNT::send_packet()
{
    if (this->is_receive_only()) return;
    if (this->is_poll_only()) this->update_ = ACUpdate::NoUpdate;
    if (this->wait_response_) {
        if (millis() - this->last_packet_sent_ < protocol::TIME_TIMEOUT_INACTIVE_MS) return;
        this->wait_response_ = false;
        this->poll_timeouts_++;
        ESP_LOGW(TAG, "Poll/command response timed out; retrying after one outstanding request");
        this->publish_protocol_state("response_timeout");
    }
    if (millis() - this->last_packet_sent_ < protocol::TIME_REFRESH_PERIOD_MS) return;
    /* Preserve model-specific fields from the last valid report; patch only owned fields below. */
    std::vector<uint8_t> packet(protocol::SET_PACKET_LEN, 0);
    if (!this->last_report_payload_.empty()) {
        std::copy_n(this->last_report_payload_.begin(), std::min(this->last_report_payload_.size(), packet.size()), packet.begin());
    }
    packet[protocol::SET_CONST_02_BYTE] = protocol::SET_CONST_02_VAL;
    /* Clear only fields this component owns before writing requested state. */
    packet[protocol::REPORT_MODE_BYTE] &= ~(protocol::REPORT_PWR_MASK | protocol::REPORT_MODE_MASK | protocol::REPORT_FAN_SPD2_MASK | protocol::REPORT_SLEEP_MASK);
    packet[protocol::REPORT_TEMP_SET_BYTE] &= ~protocol::REPORT_TEMP_SET_MASK;
    packet[protocol::REPORT_FAN_SPD1_BYTE] &= ~protocol::REPORT_FAN_SPD1_MASK;
    packet[protocol::REPORT_FAN_QUIET_BYTE] &= ~protocol::REPORT_FAN_QUIET_MASK;
    packet[protocol::REPORT_FAN_TURBO_BYTE] &= ~(protocol::REPORT_FAN_TURBO_MASK | protocol::REPORT_DISP_ON_MASK | protocol::REPORT_PLASMA1_MASK | protocol::REPORT_XFAN_MASK);
    packet[protocol::REPORT_HSWING_BYTE] &= ~(protocol::REPORT_HSWING_MASK | protocol::REPORT_VSWING_MASK);
    packet[protocol::REPORT_DISP_MODE_BYTE] &= ~protocol::REPORT_DISP_MODE_MASK;
    packet[protocol::REPORT_DISP_F_BYTE] &= ~protocol::REPORT_DISP_F_MASK;
    packet[protocol::REPORT_PLASMA2_BYTE] &= ~protocol::REPORT_PLASMA2_MASK;
    packet[protocol::REPORT_SAVE_BYTE] &= ~(protocol::REPORT_SAVE_MASK | protocol::SET_NOCHANGE_MASK);
    packet[protocol::SET_CONST_BIT_BYTE] |= protocol::SET_CONST_BIT_MASK;

    /* Prepare the rest of the frame */
    /* this handles tricky part of 0xAF value and flag marking that WiFi does not apply any changes */
    switch(this->update_)
    {
        default:
        case ACUpdate::NoUpdate:
            packet[protocol::SET_NOCHANGE_BYTE] |= protocol::SET_NOCHANGE_MASK;
            break;
        case ACUpdate::UpdateStart:
            packet[protocol::SET_AF_BYTE] = protocol::SET_AF_VAL;
            break;
        case ACUpdate::UpdateClear:
            break;
    }

    /* MODE and POWER --------------------------------------------------------------------------- */
    uint8_t mode = protocol::REPORT_MODE_AUTO;
    bool power = false;
    switch (this->mode)
    {
        case climate::CLIMATE_MODE_AUTO:
            mode = protocol::REPORT_MODE_AUTO;
            power = true;
            break;
        case climate::CLIMATE_MODE_COOL:
            mode = protocol::REPORT_MODE_COOL;
            power = true;
            break;
        case climate::CLIMATE_MODE_DRY:
            mode = protocol::REPORT_MODE_DRY;
            power = true;
            break;
        case climate::CLIMATE_MODE_FAN_ONLY:
            mode = protocol::REPORT_MODE_FAN;
            power = true;
            break;
        case climate::CLIMATE_MODE_HEAT:
            mode = protocol::REPORT_MODE_HEAT;
            power = true;
            break;
        default:
        case climate::CLIMATE_MODE_OFF:
            /* In case of MODE_OFF we will not alter the last mode setting recieved from AC, see determine_mode() */
            switch (this->mode_internal_)
            {
                case climate::CLIMATE_MODE_AUTO:
                    mode = protocol::REPORT_MODE_AUTO;
                    break;
                case climate::CLIMATE_MODE_COOL:
                    mode = protocol::REPORT_MODE_COOL;
                    break;
                case climate::CLIMATE_MODE_DRY:
                    mode = protocol::REPORT_MODE_DRY;
                    break;
                case climate::CLIMATE_MODE_FAN_ONLY:
                    mode = protocol::REPORT_MODE_FAN;
                    break;
                case climate::CLIMATE_MODE_HEAT:
                    mode = protocol::REPORT_MODE_HEAT;
                    break;
                case climate::CLIMATE_MODE_HEAT_COOL:
                case climate::CLIMATE_MODE_OFF:
                    // Neither value is reported by the unit; retain a safe AUTO mode while powered off.
                    mode = protocol::REPORT_MODE_AUTO;
                    break;
            }
            power = false;
            break;
    }

    packet[protocol::REPORT_MODE_BYTE] |= (mode << protocol::REPORT_MODE_POS);
    if (power)
    {
        packet[protocol::REPORT_PWR_BYTE] |= protocol::REPORT_PWR_MASK;
    }

    /* TARGET TEMPERATURE --------------------------------------------------------------------------- */
    uint8_t target_temperature = ((((uint8_t)this->target_temperature) - protocol::REPORT_TEMP_SET_OFF) << protocol::REPORT_TEMP_SET_POS);
    packet[protocol::REPORT_TEMP_SET_BYTE] |= (target_temperature & protocol::REPORT_TEMP_SET_MASK);

    /* FAN SPEED --------------------------------------------------------------------------- */
    /* below will default to AUTO */
    uint8_t fanSpeed1 = 0;
    uint8_t fanSpeed2 = 0;
    bool    fanQuiet  = false;
    bool    fanTurbo  = false;
    if (this->has_custom_fan_mode())
    {
        const char* custom_fan_mode = this->get_custom_fan_mode().c_str();

        if (strcmp(custom_fan_mode, fan_modes::FAN_AUTO) == 0)
        {
            fanSpeed1 = 0;
            fanSpeed2 = 0;
            fanQuiet  = false;
            fanTurbo  = false;
        }
        else if (strcmp(custom_fan_mode, fan_modes::FAN_LOW) == 0)
        {
            fanSpeed1 = 1;
            fanSpeed2 = 1;
            fanQuiet  = false;
            fanTurbo  = false;
        }
        else if (strcmp(custom_fan_mode, fan_modes::FAN_QUIET) == 0)
        {
            fanSpeed1 = 1;
            fanSpeed2 = 1;
            fanQuiet  = true;
            fanTurbo  = false;
        }
        else if (strcmp(custom_fan_mode, fan_modes::FAN_MEDL) == 0)
        {
            fanSpeed1 = 2;
            fanSpeed2 = 2;
            fanQuiet  = false;
            fanTurbo  = false;
        }
        else if (strcmp(custom_fan_mode, fan_modes::FAN_MED) == 0)
        {
            fanSpeed1 = 3;
            fanSpeed2 = 2;
            fanQuiet  = false;
            fanTurbo  = false;
        }
        else if (strcmp(custom_fan_mode, fan_modes::FAN_MEDH) == 0)
        {
            fanSpeed1 = 4;
            fanSpeed2 = 3;
            fanQuiet  = false;
            fanTurbo  = false;
        }
        else if (strcmp(custom_fan_mode, fan_modes::FAN_HIGH) == 0)
        {
            fanSpeed1 = 5;
            fanSpeed2 = 3;
            fanQuiet  = false;
            fanTurbo  = false;
        }
        else if (strcmp(custom_fan_mode, fan_modes::FAN_TURBO) == 0)
        {
            fanSpeed1 = 5;
            fanSpeed2 = 3;
            fanQuiet  = false;
            fanTurbo  = true;
        }
        else
        {
            fanSpeed1 = 0;
            fanSpeed2 = 0;
            fanQuiet  = false;
            fanTurbo  = false;
        }
    }

    packet[protocol::REPORT_FAN_SPD1_BYTE] |= (fanSpeed1 << protocol::REPORT_FAN_SPD1_POS);
    packet[protocol::REPORT_FAN_SPD2_BYTE] |= (fanSpeed2 << protocol::REPORT_FAN_SPD2_POS);
    if (fanTurbo)
    {
        packet[protocol::REPORT_FAN_TURBO_BYTE] |= protocol::REPORT_FAN_TURBO_MASK;
    }
    if (fanQuiet)
    {
        packet[protocol::REPORT_FAN_QUIET_BYTE] |= protocol::REPORT_FAN_QUIET_MASK;
    }

    /* VERTICAL SWING --------------------------------------------------------------------------- */
    uint8_t mode_vertical_swing = protocol::REPORT_VSWING_OFF;
    if (this->vertical_swing_state_ == vertical_swing_options::OFF)
    {
        mode_vertical_swing = protocol::REPORT_VSWING_OFF;
    }
    else if (this->vertical_swing_state_ == vertical_swing_options::FULL)
    {
        mode_vertical_swing = protocol::REPORT_VSWING_FULL;
    }
    else if (this->vertical_swing_state_ == vertical_swing_options::DOWN)
    {
        mode_vertical_swing = protocol::REPORT_VSWING_DOWN;
    }
    else if (this->vertical_swing_state_ == vertical_swing_options::MIDD)
    {
        mode_vertical_swing = protocol::REPORT_VSWING_MIDD;
    }
    else if (this->vertical_swing_state_ == vertical_swing_options::MID)
    {
        mode_vertical_swing = protocol::REPORT_VSWING_MID;
    }
    else if (this->vertical_swing_state_ == vertical_swing_options::MIDU)
    {
        mode_vertical_swing = protocol::REPORT_VSWING_MIDU;
    }
    else if (this->vertical_swing_state_ == vertical_swing_options::UP)
    {
        mode_vertical_swing = protocol::REPORT_VSWING_UP;
    }
    else if (this->vertical_swing_state_ == vertical_swing_options::CDOWN)
    {
        mode_vertical_swing = protocol::REPORT_VSWING_CDOWN;
    }
    else if (this->vertical_swing_state_ == vertical_swing_options::CMIDD)
    {
        mode_vertical_swing = protocol::REPORT_VSWING_CMIDD;
    }
    else if (this->vertical_swing_state_ == vertical_swing_options::CMID)
    {
        mode_vertical_swing = protocol::REPORT_VSWING_CMID;
    }
    else if (this->vertical_swing_state_ == vertical_swing_options::CMIDU)
    {
        mode_vertical_swing = protocol::REPORT_VSWING_CMIDU;
    }
    else if (this->vertical_swing_state_ == vertical_swing_options::CUP)
    {
        mode_vertical_swing = protocol::REPORT_VSWING_CUP;
    }
    else
    {
        mode_vertical_swing = protocol::REPORT_VSWING_OFF;
    }
    packet[protocol::REPORT_VSWING_BYTE] |= (mode_vertical_swing << protocol::REPORT_VSWING_POS);

    /* HORIZONTAL SWING --------------------------------------------------------------------------- */
    uint8_t mode_horizontal_swing = protocol::REPORT_HSWING_OFF;
    if (this->horizontal_swing_state_ == horizontal_swing_options::OFF)
    {
        mode_horizontal_swing = protocol::REPORT_HSWING_OFF;
    }
    else if (this->horizontal_swing_state_ == horizontal_swing_options::FULL)
    {
        mode_horizontal_swing = protocol::REPORT_HSWING_FULL;
    }
    else if (this->horizontal_swing_state_ == horizontal_swing_options::CLEFT)
    {
        mode_horizontal_swing = protocol::REPORT_HSWING_CLEFT;
    }
    else if (this->horizontal_swing_state_ == horizontal_swing_options::CMIDL)
    {
        mode_horizontal_swing = protocol::REPORT_HSWING_CMIDL;
    }
    else if (this->horizontal_swing_state_ == horizontal_swing_options::CMID)
    {
        mode_horizontal_swing = protocol::REPORT_HSWING_CMID;
    }
    else if (this->horizontal_swing_state_ == horizontal_swing_options::CMIDR)
    {
        mode_horizontal_swing = protocol::REPORT_HSWING_CMIDR;
    }
    else if (this->horizontal_swing_state_ == horizontal_swing_options::CRIGHT)
    {
        mode_horizontal_swing = protocol::REPORT_HSWING_CRIGHT;
    }
    else
    {
        mode_horizontal_swing = protocol::REPORT_HSWING_OFF;
    }
    packet[protocol::REPORT_HSWING_BYTE] |= (mode_horizontal_swing << protocol::REPORT_HSWING_POS);

    /* DISPLAY --------------------------------------------------------------------------- */
    uint8_t display_mode = protocol::REPORT_DISP_MODE_AUTO;
    if (this->display_state_ == display_options::AUTO)
    {
        display_mode = protocol::REPORT_DISP_MODE_AUTO;
        this->display_power_internal_ = true;
    }
    else if (this->display_state_ == display_options::SET)
    {
        display_mode = protocol::REPORT_DISP_MODE_SET;
        this->display_power_internal_ = true;
    }
    else if (this->display_state_ == display_options::ACT)
    {
        display_mode = protocol::REPORT_DISP_MODE_ACT;
        this->display_power_internal_ = true;
    }
    else if (this->display_state_ == display_options::OUT)
    {
        display_mode = protocol::REPORT_DISP_MODE_OUT;
        this->display_power_internal_ = true;
    }
    else if (this->display_state_ == display_options::OFF)
    {
        /* we do not want to alter display setting - only turn it off */
        this->display_power_internal_ = false;
        if (this->display_mode_internal_ == display_options::AUTO)
        {
            display_mode = protocol::REPORT_DISP_MODE_AUTO;
        }
        else if (this->display_mode_internal_ == display_options::SET)
        {
            display_mode = protocol::REPORT_DISP_MODE_SET;
        }
        else if (this->display_mode_internal_ == display_options::ACT)
        {
            display_mode = protocol::REPORT_DISP_MODE_ACT;
        }
        else if (this->display_mode_internal_ == display_options::OUT)
        {
            display_mode = protocol::REPORT_DISP_MODE_OUT;
        }
        else
        {
            display_mode = protocol::REPORT_DISP_MODE_AUTO;
        }
    }
    else
    {
        display_mode = protocol::REPORT_DISP_MODE_AUTO;
        this->display_power_internal_ = true;
    }

    packet[protocol::REPORT_DISP_MODE_BYTE] |= (display_mode << protocol::REPORT_DISP_MODE_POS);

    if (this->display_power_internal_)
    {
        packet[protocol::REPORT_DISP_ON_BYTE] |= protocol::REPORT_DISP_ON_MASK;
    }

    /* DISPLAY UNIT --------------------------------------------------------------------------- */
    if (this->display_unit_state_ == display_unit_options::DEGF)
    {
        packet[protocol::REPORT_DISP_F_BYTE] |= protocol::REPORT_DISP_F_MASK;
    }

    /* PLASMA --------------------------------------------------------------------------- */
    if (this->plasma_state_)
    {
        packet[protocol::REPORT_PLASMA1_BYTE] |= protocol::REPORT_PLASMA1_MASK;
        packet[protocol::REPORT_PLASMA2_BYTE] |= protocol::REPORT_PLASMA2_MASK;
    }

    /* SLEEP --------------------------------------------------------------------------- */
    if (this->sleep_state_)
    {
        packet[protocol::REPORT_SLEEP_BYTE] |= protocol::REPORT_SLEEP_MASK;
    }

    /* XFAN --------------------------------------------------------------------------- */
    if (this->xfan_state_)
    {
        packet[protocol::REPORT_XFAN_BYTE] |= protocol::REPORT_XFAN_MASK;
    }

    /* SAVE --------------------------------------------------------------------------- */
    if (this->save_state_)
    {
        packet[protocol::REPORT_SAVE_BYTE] |= protocol::REPORT_SAVE_MASK;
    }
    
    /* Do the command, length */
    packet.insert(packet.begin(), protocol::CMD_OUT_PARAMS_SET);
    packet.insert(packet.begin(), protocol::SET_PACKET_LEN + 2); /* Add 2 bytes as we added a command and will add checksum */

    /* Do checksum - sum of all bytes except sync and checksum itself% 0x100 
       the module would be realized by the fact that we are using uint8_t*/
    uint8_t checksum = 0;
    for (size_t i = 0 ; i < packet.size() ; i++)
    {
        checksum += packet[i];
    }
    packet.push_back(checksum);

    /* Do SYNC bytes */
    packet.insert(packet.begin(), protocol::SYNC);
    packet.insert(packet.begin(), protocol::SYNC);

    this->last_packet_sent_ = millis();  /* Save the time when we sent the last packet */
    this->wait_response_ = true;
    write_array(packet);                 /* Sent the packet by UART */
    this->record_transmitted_packet(packet);
    log_packet(packet, true);            /* Log uart for debug purposes */

    if (this->update_ == ACUpdate::UpdateStart) this->publish_protocol_state("command_apply_waiting");
    else if (this->update_ == ACUpdate::UpdateClear) this->publish_protocol_state("command_clear_waiting");
    else this->publish_protocol_state("waiting_for_poll_response");
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

/*
 * This decodes frame recieved from AC Unit
 */
bool SinclairACCNT::processUnitReport(const std::vector<uint8_t> &payload)
{
    bool hasChanged = false;
    this->last_report_payload_ = payload;
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
    const bool gree_profile = this->fan_profile_ == FanProfile::GREE_4_SPEED ||
                              (this->fan_profile_ == FanProfile::AUTO && fan_speed1_raw == 0x08);
    /* Gree Livo uses byte 4 low bits. Byte 18=0x08 is not a fan request. */
    if (gree_profile) {
        switch (fanSpeed2) {
            case 0: fan_mode = fan_modes::FAN_AUTO; decode_status = "gree_4_speed:auto"; break;
            case 1: fan_mode = fan_modes::FAN_LOW; decode_status = "gree_4_speed:low"; break;
            case 2: fan_mode = fan_modes::FAN_MED; decode_status = "gree_4_speed:medium"; break;
            case 3: fan_mode = fan_modes::FAN_HIGH; decode_status = "gree_4_speed:high"; break;
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

    this->update_ = ACUpdate::UpdateStart;
    this->vertical_swing_state_ = swing;
}

void SinclairACCNT::on_horizontal_swing_change(const std::string &swing)
{
    if (!this->can_control()) return;
    if (this->state_ != ACState::Ready)
        return;

    ESP_LOGD(TAG, "Setting horizontal swing position");

    this->update_ = ACUpdate::UpdateStart;
    this->horizontal_swing_state_ = swing;
}

void SinclairACCNT::on_display_change(const std::string &display)
{
    if (!this->can_control()) return;
    if (this->state_ != ACState::Ready)
        return;

    ESP_LOGD(TAG, "Setting display mode");

    this->update_ = ACUpdate::UpdateStart;
    this->display_state_ = display;
}

void SinclairACCNT::on_display_unit_change(const std::string &display_unit)
{
    if (!this->can_control()) return;
    if (this->state_ != ACState::Ready)
        return;

    ESP_LOGD(TAG, "Setting display unit");

    this->update_ = ACUpdate::UpdateStart;
    this->display_unit_state_ = display_unit;
}

void SinclairACCNT::on_plasma_change(bool plasma)
{
    if (!this->can_control()) return;
    if (this->state_ != ACState::Ready)
        return;

    ESP_LOGD(TAG, "Setting plasma");

    this->update_ = ACUpdate::UpdateStart;
    this->plasma_state_ = plasma;
}

void SinclairACCNT::on_sleep_change(bool sleep)
{
    if (!this->can_control()) return;
    if (this->state_ != ACState::Ready)
        return;

    ESP_LOGD(TAG, "Setting sleep");

    this->update_ = ACUpdate::UpdateStart;
    this->sleep_state_ = sleep;
}

void SinclairACCNT::on_xfan_change(bool xfan)
{
    if (!this->can_control()) return;
    if (this->state_ != ACState::Ready)
        return;

    ESP_LOGD(TAG, "Setting xfan");

    this->update_ = ACUpdate::UpdateStart;
    this->xfan_state_ = xfan;
}

void SinclairACCNT::on_save_change(bool save)
{
    if (!this->can_control()) return;
    if (this->state_ != ACState::Ready)
        return;

    ESP_LOGD(TAG, "Setting save");

    this->update_ = ACUpdate::UpdateStart;
    this->save_state_ = save;
}

}  // namespace CNT
}  // namespace sinclair_ac
}  // namespace esphome
