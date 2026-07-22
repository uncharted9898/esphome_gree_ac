// based on: https://github.com/DomiStyle/esphome-panasonic-ac
#pragma once

#include <algorithm>
#include <map>
#include <vector>

#include "esphome/components/climate/climate.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "telemetry_discovery.h"

namespace esphome {

namespace sinclair_ac {

static const char *const VERSION = "0.0.1";

static const uint32_t UART_BAUD = 4800;
static const uint8_t UART_BITS_PER_CHARACTER = 11;  // 8E1
static const uint32_t FRAME_TIMEOUT_MARGIN_MS = 150;

static const uint8_t MIN_TEMPERATURE = 16;   // Minimum temperature as reported by EWPE SMART APP
static const uint8_t MAX_TEMPERATURE = 30;   // Maximum temperature as supported by EWPE SMART APP
static const float TEMPERATURE_STEP = 1.0;   // Steps the temperature can be set in
static const float TEMPERATURE_TOLERANCE = 2;  // The tolerance to allow when checking the climate state
static const uint8_t TEMPERATURE_THRESHOLD = 100;  // Maximum temperature the AC can report (formally 119.5 for sinclair protocol, but 100 is impossible, soo...)

namespace fan_modes{
    const char* const FAN_AUTO  = "0 - Auto";
    const char* const FAN_QUIET = "1 - Quiet";
    const char* const FAN_LOW   = "2 - Low";
    const char* const FAN_MEDL  = "3 - Medium-Low";
    const char* const FAN_MED   = "4 - Medium";
    const char* const FAN_MEDH  = "5 - Medium-High";
    const char* const FAN_HIGH  = "6 - High";
    const char* const FAN_TURBO = "7 - Turbo";
}

/* this must be same as HORIZONTAL_SWING_OPTIONS in climate.py */
namespace horizontal_swing_options{
    const std::string OFF    = "0 - OFF";
    const std::string FULL   = "1 - Swing - Full";
    const std::string CLEFT  = "2 - Constant - Left";
    const std::string CMIDL  = "3 - Constant - Mid-Left";
    const std::string CMID   = "4 - Constant - Middle";
    const std::string CMIDR  = "5 - Constant - Mid-Right";
    const std::string CRIGHT = "6 - Constant - Right";
}

/* this must be same as VERTICAL_SWING_OPTIONS in climate.py */
namespace vertical_swing_options{
    const std::string OFF   = "00 - OFF";
    const std::string FULL  = "01 - Swing - Full";
    const std::string DOWN  = "02 - Swing - Down";
    const std::string MIDD  = "03 - Swing - Mid-Down";
    const std::string MID   = "04 - Swing - Middle";
    const std::string MIDU  = "05 - Swing - Mid-Up";
    const std::string UP    = "06 - Swing - Up";
    const std::string CDOWN = "07 - Constant - Down";
    const std::string CMIDD = "08 - Constant - Mid-Down";
    const std::string CMID  = "09 - Constant - Middle";
    const std::string CMIDU = "10 - Constant - Mid-Up";
    const std::string CUP   = "11 - Constant - Up";
}

/* this must be same as DISPLAY_OPTIONS in climate.py */
namespace display_options{
    const std::string OFF  = "0 - OFF";
    const std::string AUTO = "1 - Auto";
    const std::string SET  = "2 - Set temperature";
    const std::string ACT  = "3 - Actual temperature";
    const std::string OUT  = "4 - Outside temperature";
}

/* this must be same as DISPLAY_UNIT_OPTIONS in climate.py */
namespace display_unit_options{
    const std::string DEGC = "C";
    const std::string DEGF = "F";
}

typedef enum {
        STATE_WAIT_SYNC,
        STATE_RECIEVE,
        STATE_COMPLETE,
        STATE_RESTART
} SerialProcessState_t;

static const uint8_t DATA_MAX = 200;

typedef struct {
        std::vector<uint8_t> data;
        uint16_t frame_size{0};
        uint32_t started_at{0};
        SerialProcessState_t state{STATE_WAIT_SYNC};
} SerialProcess_t;

enum class ProtocolMode : uint8_t { RECEIVE_ONLY, POLL_ONLY, CONTROL };
enum class FanProfile : uint8_t { AUTO, SINCLAIR_EXTENDED, GREE_4_SPEED };

class SinclairAC : public Component, public uart::UARTDevice, public climate::Climate {
    public:
        void set_vertical_swing_select(select::Select *vertical_swing_select);
        void set_horizontal_swing_select(select::Select *horizontal_swing_select);

        void set_display_select(select::Select *display_select);
        void set_display_unit_select(select::Select *display_unit_select);

        void set_plasma_switch(switch_::Switch *plasma_switch);
        void set_sleep_switch(switch_::Switch *sleep_switch);
        void set_xfan_switch(switch_::Switch *plasma_switch);
        void set_save_switch(switch_::Switch *plasma_switch);

        void set_current_temperature_sensor(sensor::Sensor *current_temperature_sensor);
        void set_protocol_mode(ProtocolMode mode) { this->protocol_mode_ = mode; }
        void set_fan_profile(FanProfile profile) { this->fan_profile_ = profile; }
        void set_telemetry_discovery(bool enabled, bool expose_raw_payload, bool expose_raw_bytes, bool log_changes_only, uint8_t history_depth);
        void set_supplemental_queries(bool enabled, uint8_t max_attempts) { this->supplemental_query_gate_.configure(enabled, max_attempts); }
        void set_debug(bool log_rx, bool log_tx, bool log_unknown, bool log_differences, uint16_t maximum_hex_length);
        void set_valid_rx_packets_sensor(sensor::Sensor *sensor) { this->valid_rx_packets_sensor_ = sensor; }
        void set_valid_tx_packets_sensor(sensor::Sensor *sensor) { this->valid_tx_packets_sensor_ = sensor; }
        void set_unknown_packets_sensor(sensor::Sensor *sensor) { this->unknown_packets_sensor_ = sensor; }
        void set_checksum_failures_sensor(sensor::Sensor *sensor) { this->checksum_failures_sensor_ = sensor; }
        void set_invalid_length_sensor(sensor::Sensor *sensor) { this->invalid_length_sensor_ = sensor; }
        void set_too_short_sensor(sensor::Sensor *sensor) { this->too_short_sensor_ = sensor; }
        void set_frame_timeout_sensor(sensor::Sensor *sensor) { this->frame_timeout_sensor_ = sensor; }
        void set_parser_resync_sensor(sensor::Sensor *sensor) { this->parser_resync_sensor_ = sensor; }
        void set_last_packet_length_sensor(sensor::Sensor *sensor) { this->last_packet_length_sensor_ = sensor; }
        void set_last_packet_type_sensor(sensor::Sensor *sensor) { this->last_packet_type_sensor_ = sensor; }
        void set_communication_sensor(binary_sensor::BinarySensor *sensor) { this->communication_sensor_ = sensor; }
        void set_receive_only_sensor(binary_sensor::BinarySensor *sensor) { this->receive_only_sensor_ = sensor; }
        void set_poll_only_sensor(binary_sensor::BinarySensor *sensor) { this->poll_only_sensor_ = sensor; }
        void set_protocol_mode_sensor(text_sensor::TextSensor *sensor) { this->protocol_mode_sensor_ = sensor; }
        void set_protocol_state_sensor(text_sensor::TextSensor *sensor) { this->protocol_state_sensor_ = sensor; }
        void set_last_packet_sensor(text_sensor::TextSensor *sensor) { this->last_packet_sensor_ = sensor; }
        void set_last_unknown_packet_sensor(text_sensor::TextSensor *sensor) { this->last_unknown_packet_sensor_ = sensor; }
        void set_fan_speed_field_1_raw_sensor(sensor::Sensor *sensor) { this->fan_speed_field_1_raw_sensor_ = sensor; }
        void set_fan_speed_field_1_low_3_bits_sensor(sensor::Sensor *sensor) { this->fan_speed_field_1_low_3_bits_sensor_ = sensor; }
        void set_fan_speed_field_2_raw_sensor(sensor::Sensor *sensor) { this->fan_speed_field_2_raw_sensor_ = sensor; }
        void set_fan_quiet_raw_sensor(sensor::Sensor *sensor) { this->fan_quiet_raw_sensor_ = sensor; }
        void set_fan_turbo_raw_sensor(sensor::Sensor *sensor) { this->fan_turbo_raw_sensor_ = sensor; }
        void set_fan_decode_status_sensor(text_sensor::TextSensor *sensor) { this->fan_decode_status_sensor_ = sensor; }
        void set_fan_decode_profile_sensor(text_sensor::TextSensor *sensor) { this->fan_decode_profile_sensor_ = sensor; }
        void set_last_0x31_payload_sensor(text_sensor::TextSensor *sensor) { this->last_0x31_payload_sensor_ = sensor; }
        void set_last_0x33_payload_sensor(text_sensor::TextSensor *sensor) { this->last_0x33_payload_sensor_ = sensor; }
        void set_last_0x44_payload_sensor(text_sensor::TextSensor *sensor) { this->last_0x44_payload_sensor_ = sensor; }
        void set_last_0x40_payload_sensor(text_sensor::TextSensor *sensor) { this->last_0x40_payload_sensor_ = sensor; }
        void set_last_unknown_payload_sensor(text_sensor::TextSensor *sensor) { this->last_unknown_payload_sensor_ = sensor; }
        void set_candidate_telemetry_byte_44_raw_sensor(sensor::Sensor *sensor) { this->candidate_telemetry_byte_44_raw_sensor_ = sensor; }
        void set_candidate_byte_44_temperature_hypothesis_sensor(sensor::Sensor *sensor) { this->candidate_byte_44_temperature_hypothesis_sensor_ = sensor; }
        void set_discovery_summary_sensor(text_sensor::TextSensor *sensor) { this->discovery_summary_sensor_ = sensor; }
        void set_capture_export_sensor(text_sensor::TextSensor *sensor) { this->capture_export_sensor_ = sensor; }

        void setup() override;
        void loop() override;

    protected:
        select::Select *vertical_swing_select_   = nullptr; /* Advanced vertical swing select */
        select::Select *horizontal_swing_select_ = nullptr; /* Advanced horizontal swing select */

        select::Select *display_select_          = nullptr; /* Select for setting display mode */
        select::Select *display_unit_select_     = nullptr; /* Select for setting display temperature unit */

        switch_::Switch *plasma_switch_          = nullptr; /* Switch for plasma */
        switch_::Switch *sleep_switch_           = nullptr; /* Switch for sleep */
        switch_::Switch *xfan_switch_            = nullptr; /* Switch for X-fan */
        switch_::Switch *save_switch_            = nullptr; /* Switch for save */

        sensor::Sensor *current_temperature_sensor_ = nullptr; /* If user wants to replace reported temperature by an external sensor readout */

        std::string vertical_swing_state_;
        std::string horizontal_swing_state_;

        std::string display_state_;
        std::string display_unit_state_;

        bool plasma_state_{false}; bool sleep_state_{false}; bool xfan_state_{false}; bool save_state_{false};

        SerialProcess_t serialProcess_;

        uint32_t init_time_{0};   // Stores the current time
        // uint32_t last_read_;   // Stores the time at which the last read was done
        uint32_t last_packet_sent_{0};
        uint32_t last_packet_received_{0};
        std::string protocol_state_;
        bool wait_response_{false};
        ProtocolMode protocol_mode_{ProtocolMode::CONTROL};
        FanProfile fan_profile_{FanProfile::AUTO};
        bool telemetry_discovery_enabled_{false}, telemetry_expose_raw_payload_{true}, telemetry_expose_raw_bytes_{false}, telemetry_log_changes_only_{true};
        uint8_t telemetry_history_depth_{16};
        bool transmit_warning_logged_{false};
        bool log_rx_{false}, log_tx_{false}, log_unknown_{false}, log_differences_{false};
        uint16_t maximum_hex_length_{128};
        uint32_t valid_rx_packets_{0}, valid_tx_packets_{0}, unknown_packets_{0}, checksum_failures_{0}, invalid_lengths_{0}, too_short_frames_{0}, parser_resyncs_{0}, frame_timeouts_{0};
        uint32_t last_diagnostics_publish_{0};
        uint32_t last_packet_diagnostics_publish_{0};
        sensor::Sensor *valid_rx_packets_sensor_{nullptr}, *valid_tx_packets_sensor_{nullptr}, *unknown_packets_sensor_{nullptr}, *checksum_failures_sensor_{nullptr}, *invalid_length_sensor_{nullptr}, *too_short_sensor_{nullptr}, *parser_resync_sensor_{nullptr}, *frame_timeout_sensor_{nullptr}, *last_packet_length_sensor_{nullptr}, *last_packet_type_sensor_{nullptr}, *candidate_telemetry_byte_44_raw_sensor_{nullptr}, *candidate_byte_44_temperature_hypothesis_sensor_{nullptr};
        sensor::Sensor *fan_speed_field_1_raw_sensor_{nullptr}, *fan_speed_field_1_low_3_bits_sensor_{nullptr}, *fan_speed_field_2_raw_sensor_{nullptr}, *fan_quiet_raw_sensor_{nullptr}, *fan_turbo_raw_sensor_{nullptr};
        binary_sensor::BinarySensor *communication_sensor_{nullptr}, *receive_only_sensor_{nullptr}, *poll_only_sensor_{nullptr};
        text_sensor::TextSensor *protocol_mode_sensor_{nullptr}, *protocol_state_sensor_{nullptr}, *last_packet_sensor_{nullptr}, *last_unknown_packet_sensor_{nullptr}, *fan_decode_status_sensor_{nullptr}, *fan_decode_profile_sensor_{nullptr};
        text_sensor::TextSensor *last_0x31_payload_sensor_{nullptr}, *last_0x33_payload_sensor_{nullptr}, *last_0x44_payload_sensor_{nullptr}, *last_0x40_payload_sensor_{nullptr}, *last_unknown_payload_sensor_{nullptr}, *discovery_summary_sensor_{nullptr}, *capture_export_sensor_{nullptr};
        TelemetryDiscovery telemetry_capture_{16};
        SupplementalQueryGate supplemental_query_gate_;
        uint32_t last_discovery_summary_publish_{0};
        std::map<uint8_t, std::vector<uint8_t>> last_payloads_;
        std::map<uint8_t, std::vector<uint8_t>> previous_frames_;
        bool has_last_packet_diagnostics_{false};
        uint32_t last_packet_length_{0}, last_packet_type_{0};
        std::string last_packet_description_, last_unknown_packet_description_;
        bool has_fan_diagnostics_{false};
        uint8_t fan_speed_field_1_raw_{0}, fan_speed_field_1_low_3_bits_{0}, fan_speed_field_2_raw_{0}, fan_quiet_raw_{0}, fan_turbo_raw_{0};
        std::string fan_decode_status_;

        climate::ClimateTraits traits() override;

        void read_data();
        void reset_parser(bool resynchronized = false);
        uint32_t frame_timeout_ms() const;
        bool is_receive_only() const { return this->protocol_mode_ == ProtocolMode::RECEIVE_ONLY; }
        bool is_poll_only() const { return this->protocol_mode_ == ProtocolMode::POLL_ONLY; }
        bool can_control() const { return this->protocol_mode_ == ProtocolMode::CONTROL; }
        const char *protocol_mode_name() const;
        void record_received_packet(bool known);
        void record_transmitted_packet(const std::vector<uint8_t> &packet);
        void publish_diagnostics(bool force = false);
        void record_last_packet_diagnostics(uint32_t length, uint32_t type, const std::string &description, const std::string *unknown_description = nullptr);
        void publish_last_packet_diagnostics(bool force = false);
        void record_fan_diagnostics(uint8_t speed_field_1_raw, uint8_t speed_field_1_low_3_bits, uint8_t speed_field_2_raw, bool quiet, bool turbo, const char *decode_status);
        void publish_protocol_state(const char *state);
        void log_packet_difference(const std::vector<uint8_t> &packet);
        const char *fan_profile_name() const;
        void retain_payload(uint8_t command, const std::vector<uint8_t> &payload);
        void capture_packet(bool transmitted, uint8_t command, const std::vector<uint8_t> &payload);
        void publish_discovery_capture();

        void update_current_temperature(float temperature);
        void update_target_temperature(float temperature);

        void update_swing_horizontal(const std::string &swing);
        void update_swing_vertical(const std::string &swing);

        void update_display(const std::string &display);
        void update_display_unit(const std::string &display_unit);

        void update_plasma(bool plasma);
        void update_sleep(bool sleep);
        void update_xfan(bool xfan);
        void update_save(bool save);

        virtual void on_horizontal_swing_change(const std::string &swing) = 0;
        virtual void on_vertical_swing_change(const std::string &swing) = 0;

        virtual void on_display_change(const std::string &display) = 0;
        virtual void on_display_unit_change(const std::string &display_unit) = 0;

        virtual void on_plasma_change(bool plasma) = 0;
        virtual void on_sleep_change(bool sleep) = 0;
        virtual void on_xfan_change(bool xfan) = 0;
        virtual void on_save_change(bool save) = 0;

        climate::ClimateAction determine_action();

        void log_packet(const std::vector<uint8_t> &data, bool outgoing = false);
};

}  // namespace sinclair_ac
}  // namespace esphome
