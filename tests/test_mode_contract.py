"""Source-level safety contracts that do not require ESPHome headers or hardware."""
from pathlib import Path
import unittest
ROOT = Path(__file__).parents[1]
CNT = (ROOT / "components/sinclair_ac/esppac_cnt.cpp").read_text()
CPP = (ROOT / "components/sinclair_ac/esppac.cpp").read_text()
CLIMATE_PY = (ROOT / "components/sinclair_ac/climate.py").read_text()
class ModeContractTests(unittest.TestCase):
    def test_receive_only_has_no_uart_write_path(self):
        self.assertIn("void SinclairACCNT::send_packet()\n{\n    if (this->is_receive_only()) return;", CNT)
        self.assertEqual(CNT.count("write_array(packet)"), 1)
    def test_poll_only_clears_updates_before_packet_build(self):
        self.assertIn("if (this->is_poll_only()) this->update_ = ACUpdate::NoUpdate;", CNT)
        self.assertIn("packet[protocol::SET_NOCHANGE_BYTE] |= protocol::SET_NOCHANGE_MASK;", CNT)
    def test_control_callbacks_are_guarded(self):
        self.assertGreaterEqual(CNT.count("if (!this->can_control()) return;"), 8)
    def test_invalid_or_unknown_do_not_release_response_guard(self):
        self.assertIn("if (known && this->wait_response_) this->wait_response_ = false;", CNT)
        self.assertNotIn("this->wait_response_ = false;\n        /* log", CNT)
    def test_mode_and_diagnostics_schema_accept_documented_keys(self):
        self.assertIn('cv.Optional(CONF_PROTOCOL_MODE)', CLIMATE_PY)
        for key in ("too_short_frames", "frame_timeouts", "poll_only", "protocol_mode"):
            self.assertIn(f'cv.Optional("{key}")', CLIMATE_PY)
    def test_custom_fan_modes_use_the_climate_entity_api(self):
        self.assertIn("this->set_supported_custom_fan_modes(", CPP)
        self.assertNotIn("traits.set_supported_custom_fan_modes(", CPP)
    def test_uart_frame_length_includes_sync_bytes_and_length_byte(self):
        self.assertIn("c < 3 || c > DATA_MAX - 3", CPP)
        self.assertIn("frame_size = static_cast<size_t>(c) + 3", CPP)
    def test_protocol_state_is_published_only_on_change(self):
        self.assertIn("if (this->protocol_state_ == state) return;", CPP)
        self.assertIn("if (this->state_ != ACState::Initializing)", CNT)
    def test_climate_state_is_published_only_for_a_changed_report(self):
        self.assertIn("if (this->processUnitReport(payload)) this->publish_state();", CNT)
    def test_last_packet_diagnostics_are_change_or_interval_driven(self):
        self.assertIn("if (changed) this->publish_last_packet_diagnostics(true);", CPP)
        self.assertIn("millis() - this->last_packet_diagnostics_publish_ < 5000", CPP)
        self.assertNotIn("last_packet_length_sensor_->publish_state(this->serialProcess_.data.size())", CNT)
    def test_unknown_fan_logging_includes_raw_fields_and_is_rate_limited(self):
        self.assertIn('"Unknown fan mode: speed1_raw=0x%02X speed1_4bit=%u speed1_3bit=%u speed2_raw=0x%02X', CNT)
        self.assertIn("fan_speed1_raw & 0x07", CNT)
        self.assertIn("millis() - this->last_unknown_fan_warning_ >= 5000", CNT)
    def test_fan_diagnostic_schema_and_codegen_are_available(self):
        for key in ("fan_speed_field_1_raw", "fan_speed_field_1_low_3_bits", "fan_speed_field_2_raw", "fan_quiet_raw", "fan_turbo_raw", "fan_decode_status"):
            self.assertIn(f'cv.Optional("{key}")', CLIMATE_PY)
        self.assertIn("record_fan_diagnostics", CPP)
    def test_power_off_mode_fallback_handles_every_climate_mode(self):
        self.assertIn("case climate::CLIMATE_MODE_HEAT_COOL:", CNT)
        self.assertIn("case climate::CLIMATE_MODE_OFF:", CNT)
if __name__ == "__main__": unittest.main()
