// based on: https://github.com/DomiStyle/esphome-panasonic-ac
#include "esphome/components/climate/climate.h"
#include "esphome/components/climate/climate_mode.h"
#include "esppac.h"
#include "protocol_state.h"
#include "request_lifecycle.h"

namespace esphome {
namespace sinclair_ac {
namespace CNT {

namespace protocol {
    /* SYNC */
    static const uint8_t SYNC                = 0x7E;
    /* packet types */
    static const uint8_t CMD_IN_UNIT_REPORT  = 0x31;
    static const uint8_t CMD_OUT_PARAMS_SET  = 0x01;
    static const uint8_t CMD_OUT_SYNC_TIME   = 0x03;
    static const uint8_t CMD_OUT_MAC_REPORT  = 0x04; /* 7e 7e 0d 04 04 00 00 00 AA BB CC DD EE FF 00 -> AA BB CC DD EE FF = MAC address */
    static const uint8_t CMD_OUT_UNKNOWN_1   = 0x02; /* 7e 7e 10 02 00 00 00 00 00 00 01 00 28 1e 19 23 23 00 b8 */
    static const uint8_t CMD_IN_UNKNOWN_1    = 0x44; /* 7e 7e 1a 44 01 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 */
    static const uint8_t CMD_IN_UNKNOWN_2    = 0x33; /* 7e 7e 2f 33 00 00 40 00 09 20 19 0a 00 10 00 14 17 5b 08 08 00 00 00 00 00 00 00 00 01 00 00 0d 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 */

    /* byte indexes are AFTER we remove first 4 bytes from the packet (sync, length, type) as well as a checksum */
    /* unit report packet data fields, for binary values there is no need to define bit offset/position */
    static const uint8_t REPORT_PWR_BYTE       = 4;
    static const uint8_t REPORT_PWR_MASK       = 0b10000000;

    static const uint8_t REPORT_MODE_BYTE      = 4;
    static const uint8_t REPORT_MODE_MASK      = 0b01110000;
    static const uint8_t REPORT_MODE_POS       = 4;
    static const uint8_t REPORT_MODE_AUTO          = 0;
    static const uint8_t REPORT_MODE_COOL          = 1;
    static const uint8_t REPORT_MODE_DRY           = 2;
    static const uint8_t REPORT_MODE_FAN           = 3;
    static const uint8_t REPORT_MODE_HEAT          = 4;

    static const uint8_t REPORT_FAN_SPD1_BYTE  = 18;
    static const uint8_t REPORT_FAN_SPD1_MASK  = 0b00001111;
    static const uint8_t REPORT_FAN_SPD1_POS   = 0;
    static const uint8_t REPORT_FAN_SPD2_BYTE  = 4;
    static const uint8_t REPORT_FAN_SPD2_MASK  = 0b00000011;
    static const uint8_t REPORT_FAN_SPD2_POS   = 0;
    /* Gree four-speed units pack mode and fan into byte 4: 1MMM xxFF. */
    static const uint8_t REPORT_GREE_FAN_MASK  = 0b00000011;
    static const uint8_t REPORT_GREE_FAN_AUTO  = 0;
    static const uint8_t REPORT_GREE_FAN_LOW   = 1;
    static const uint8_t REPORT_GREE_FAN_MED   = 2;
    static const uint8_t REPORT_GREE_FAN_HIGH  = 3;
    static const uint8_t REPORT_FAN_QUIET_BYTE = 16;
    static const uint8_t REPORT_FAN_QUIET_MASK = 0b00001000;
    static const uint8_t REPORT_FAN_TURBO_BYTE = 6;
    static const uint8_t REPORT_FAN_TURBO_MASK = 0b00000001;

    static const uint8_t REPORT_TEMP_SET_BYTE  = 5;
    static const uint8_t REPORT_TEMP_SET_MASK  = 0b11110000;
    static const uint8_t REPORT_TEMP_SET_POS   = 4;
    static const uint8_t REPORT_TEMP_SET_OFF   = 16; /* temperature offset from value in packet */

    static const uint8_t REPORT_TEMP_ACT_BYTE  = 42;
    static const uint8_t REPORT_TEMP_ACT_MASK  = 0b11111111;
    static const uint8_t REPORT_TEMP_ACT_POS   = 0;
    static const uint8_t REPORT_TEMP_ACT_OFF   = 16;  /* temperature offset from value in packet */
    static const float   REPORT_TEMP_ACT_DIV   = 2.0; /* temperature divider from value in packet */

    static const uint8_t REPORT_HSWING_BYTE    = 8;
    static const uint8_t REPORT_HSWING_MASK    = 0b00000111;
    static const uint8_t REPORT_HSWING_POS     = 0;
    static const uint8_t REPORT_HSWING_OFF         = 0;
    static const uint8_t REPORT_HSWING_FULL        = 1;
    static const uint8_t REPORT_HSWING_CLEFT       = 2;
    static const uint8_t REPORT_HSWING_CMIDL       = 3;
    static const uint8_t REPORT_HSWING_CMID        = 4;
    static const uint8_t REPORT_HSWING_CMIDR       = 5;
    static const uint8_t REPORT_HSWING_CRIGHT      = 6;

    static const uint8_t REPORT_VSWING_BYTE    = 8;
    static const uint8_t REPORT_VSWING_MASK    = 0b11110000;
    static const uint8_t REPORT_VSWING_POS     = 4;
    static const uint8_t REPORT_VSWING_OFF         = 0;
    static const uint8_t REPORT_VSWING_FULL        = 1;
    static const uint8_t REPORT_VSWING_CUP         = 2;
    static const uint8_t REPORT_VSWING_CMIDU       = 3;
    static const uint8_t REPORT_VSWING_CMID        = 4;
    static const uint8_t REPORT_VSWING_CMIDD       = 5;
    static const uint8_t REPORT_VSWING_CDOWN       = 6;
    static const uint8_t REPORT_VSWING_DOWN        = 7;
    static const uint8_t REPORT_VSWING_MIDD        = 8;
    static const uint8_t REPORT_VSWING_MID         = 9;
    static const uint8_t REPORT_VSWING_MIDU        = 10;
    static const uint8_t REPORT_VSWING_UP          = 11;

    static const uint8_t REPORT_DISP_ON_BYTE   = 6;
    static const uint8_t REPORT_DISP_ON_MASK   = 0b00000010;
    static const uint8_t REPORT_DISP_MODE_BYTE = 9;
    static const uint8_t REPORT_DISP_MODE_MASK = 0b00110000;
    static const uint8_t REPORT_DISP_MODE_POS  = 4;
    static const uint8_t REPORT_DISP_MODE_AUTO     = 0;
    static const uint8_t REPORT_DISP_MODE_SET      = 1;
    static const uint8_t REPORT_DISP_MODE_ACT      = 2;
    static const uint8_t REPORT_DISP_MODE_OUT      = 3;

    static const uint8_t REPORT_DISP_F_BYTE    = 7;
    static const uint8_t REPORT_DISP_F_MASK    = 0b10000000;

    static const uint8_t REPORT_PLASMA1_BYTE   = 6;
    static const uint8_t REPORT_PLASMA1_MASK   = 0b00000100;
    static const uint8_t REPORT_PLASMA2_BYTE   = 0;
    static const uint8_t REPORT_PLASMA2_MASK   = 0b00000100;

    static const uint8_t REPORT_SLEEP_BYTE     = 4;
    static const uint8_t REPORT_SLEEP_MASK     = 0b00001000;

    static const uint8_t REPORT_XFAN_BYTE      = 6;
    static const uint8_t REPORT_XFAN_MASK      = 0b00001000;

    static const uint8_t REPORT_SAVE_BYTE      = 11;
    static const uint8_t REPORT_SAVE_MASK      = 0b01000000;

    /* SET packet shares all the byte definition with REPORT */
    static const uint8_t SET_PACKET_LEN        = 45;
    
    static const uint8_t SET_CONST_02_BYTE     = 39;
    static const uint8_t SET_CONST_02_VAL      = 0x02;

    static const uint8_t SET_AF_BYTE           = 3;
    static const uint8_t SET_AF_VAL            = 0xAF;

    static const uint8_t SET_NOCHANGE_BYTE     = 11;
    static const uint8_t SET_NOCHANGE_MASK     = 0b00001000;

    static const uint8_t SET_CONST_BIT_BYTE    = 7;
    static const uint8_t SET_CONST_BIT_MASK    = 0b00000010;

    /* time constraints */
    static const unsigned long TIME_REFRESH_PERIOD_MS       =  300;
    static const unsigned long POLL_RESPONSE_TIMEOUT_MS     = 1500;
    static const unsigned long COMMUNICATION_TIMEOUT_MS     = 5000;
}

/* Define packets from AC that would be processed by software */
const std::vector<uint8_t> allowedPackets = {protocol::CMD_IN_UNIT_REPORT};

class SinclairACCNT : public SinclairAC {
    public:
        void control(const climate::ClimateCall &call) override;

        void on_horizontal_swing_change(const std::string &swing) override;
        void on_vertical_swing_change(const std::string &swing) override;

        void on_display_change(const std::string &display) override;
        void on_display_unit_change(const std::string &display_unit) override;

        void on_plasma_change(bool plasma) override;
        void on_sleep_change(bool sleep) override;
        void on_xfan_change(bool xfan) override;
        void on_save_change(bool save) override;

        void setup() override;
        void loop() override;
        void set_polls_sent_sensor(sensor::Sensor *s) { polls_sent_sensor_ = s; }
        void set_poll_responses_sensor(sensor::Sensor *s) { poll_responses_sensor_ = s; }
        void set_poll_response_timeouts_sensor(sensor::Sensor *s) { poll_response_timeouts_sensor_ = s; }
        void set_consecutive_poll_timeouts_sensor(sensor::Sensor *s) { consecutive_poll_timeouts_sensor_ = s; }
        void set_last_poll_response_ms_sensor(sensor::Sensor *s) { last_poll_response_ms_sensor_ = s; }
        void set_command_attempts_sensor(sensor::Sensor *s) { command_attempts_sensor_ = s; }
        void set_command_response_timeouts_sensor(sensor::Sensor *s) { command_response_timeouts_sensor_ = s; }
        void set_command_mismatches_sensor(sensor::Sensor *s) { command_mismatches_sensor_ = s; }
        void set_last_command_result_sensor(text_sensor::TextSensor *s) { last_command_result_sensor_ = s; }
        void set_last_command_failure_reason_sensor(text_sensor::TextSensor *s) { last_command_failure_reason_sensor_ = s; }

    protected:
        enum PendingField : uint16_t {
            PENDING_MODE = 1 << 0, PENDING_TARGET_TEMPERATURE = 1 << 1,
            PENDING_FAN = 1 << 2, PENDING_VERTICAL_SWING = 1 << 3,
            PENDING_HORIZONTAL_SWING = 1 << 4, PENDING_DISPLAY = 1 << 5,
            PENDING_DISPLAY_UNIT = 1 << 6, PENDING_PLASMA = 1 << 7,
            PENDING_SLEEP = 1 << 8, PENDING_XFAN = 1 << 9, PENDING_SAVE = 1 << 10,
        };
        struct PendingControlState {
            bool active{false};
            uint16_t requested_fields{0};
            climate::ClimateMode mode{climate::CLIMATE_MODE_OFF};
            float target_temperature{MIN_TEMPERATURE};
            std::string custom_fan_mode{fan_modes::FAN_AUTO};
            std::string vertical_swing, horizontal_swing, display_mode, display_unit;
            bool plasma{false}, sleep{false}, xfan{false}, save{false};
            uint8_t retries{0};
        };
        ACState state_ = ACState::Initializing; /* Stores if the AC is responsive or not */
        ACUpdate update_ = ACUpdate::NoUpdate;  /* Stores if we need tu send update to AC or no */
        PendingControlState pending_control_;
        RequestLifecycle request_lifecycle_;

        climate::ClimateMode mode_internal_{climate::CLIMATE_MODE_OFF};
        bool power_internal_{false};

        std::string display_mode_internal_;
        bool display_power_internal_{false};

        bool processUnitReport(const std::vector<uint8_t> &payload);
        bool uses_gree_fan_layout() const;
        std::vector<uint8_t> build_poll_packet() const;
        std::vector<uint8_t> build_command_packet(ACUpdate update) const;
        void apply_requested_field_patches(std::vector<uint8_t> &packet) const;
        uint8_t allowed_command_mask(size_t index, uint16_t requested_fields, bool gree_layout) const;
        uint8_t allowed_envelope_mask(size_t index, ACUpdate update) const;
        void verify_packet_changes(const std::vector<uint8_t> &packet, ACUpdate update) const;
        void verify_no_change_packet(const std::vector<uint8_t> &packet) const;
        void begin_pending_control();
        void restart_pending_control(uint16_t fields);
        bool pending_control_matches_report(const std::vector<uint8_t> &payload) const;
        void handle_pending_control_response(const std::vector<uint8_t> &payload);

        void send_packet();

        enum class PacketValidationResult { VALID_KNOWN, VALID_UNKNOWN, INVALID_TOO_SHORT, INVALID_LENGTH, INVALID_CHECKSUM };
        PacketValidationResult verify_packet();
        void handle_packet();

        const std::vector<uint8_t> *report_payload_{nullptr};
        uint32_t last_unknown_fan_warning_{0};
        uint32_t last_unknown_fan_signature_{0};
        bool has_unknown_fan_signature_{false};
        std::vector<uint8_t> last_report_payload_;
        bool gree_fan_layout_detected_{false};
        std::string last_command_result_{"none"};
        std::string last_command_failure_reason_{"none"};
        bool has_published_request_diagnostics_{false};
        uint32_t last_request_diagnostics_publish_{0};
        uint32_t published_polls_sent_{0}, published_poll_responses_{0}, published_poll_response_timeouts_{0};
        uint32_t published_consecutive_poll_timeouts_{0}, published_last_poll_response_ms_{0};
        uint32_t published_command_attempts_{0}, published_command_response_timeouts_{0}, published_command_mismatches_{0};
        std::string published_last_command_result_, published_last_command_failure_reason_;
        bool has_published_candidate_telemetry_byte_44_{false};
        uint8_t published_candidate_telemetry_byte_44_{0};
        uint32_t last_candidate_telemetry_byte_44_publish_{0};
        sensor::Sensor *polls_sent_sensor_{nullptr}, *poll_responses_sensor_{nullptr}, *poll_response_timeouts_sensor_{nullptr}, *consecutive_poll_timeouts_sensor_{nullptr}, *last_poll_response_ms_sensor_{nullptr}, *command_attempts_sensor_{nullptr}, *command_response_timeouts_sensor_{nullptr}, *command_mismatches_sensor_{nullptr};
        text_sensor::TextSensor *last_command_result_sensor_{nullptr}, *last_command_failure_reason_sensor_{nullptr};

        void publish_request_diagnostics(bool force = false);

        climate::ClimateMode determine_mode();
        const char* determine_fan_mode();

        std::string determine_vertical_swing();
        std::string determine_horizontal_swing();

        std::string determine_display();
        std::string determine_display_unit();

        bool determine_plasma();
        bool determine_sleep();
        bool determine_xfan();
        bool determine_save();
};

}  // namespace CNT
}  // namespace sinclair_ac
}  // namespace esphome
