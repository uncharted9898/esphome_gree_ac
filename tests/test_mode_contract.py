"""Source-level safety contracts that do not require ESPHome headers or hardware."""
from pathlib import Path
import unittest
ROOT = Path(__file__).parents[1]
CNT = (ROOT / "components/sinclair_ac/esppac_cnt.cpp").read_text()
CPP = (ROOT / "components/sinclair_ac/esppac.cpp").read_text()
CLIMATE_PY = (ROOT / "components/sinclair_ac/climate.py").read_text()
PROTOCOL_STATE = (ROOT / "components/sinclair_ac/protocol_state.h").read_text()
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
        self.assertIn("handle_packet(); /* Reports are acknowledgements as well as state updates. */", CNT)
        self.assertIn("this->handle_pending_control_response(payload); /* Verify after decoded state has been updated. */", CNT)
        self.assertIn("if (!this->wait_response_ || !this->pending_control_.active) return;", CNT)
        self.assertIn("const bool matches = this->pending_control_matches_report(payload);", CNT)
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
    def test_gree_profile_uses_combined_mode_fan_low_bits(self):
        self.assertIn("FanProfile::GREE_4_SPEED", CNT)
        self.assertIn("REPORT_GREE_FAN_MASK", CNT)
        self.assertIn("switch (gree_fan)", CNT)
        self.assertIn('"gree_4_speed:medium"', CNT)
        self.assertNotIn("fan_speed1_raw == 0x08", CNT)
        self.assertIn("packet[protocol::REPORT_FAN_SPD2_BYTE] |= fanSpeed2 & protocol::REPORT_GREE_FAN_MASK;", CNT)
    def test_temperature_byte_42_is_excluded_from_discovery_changes(self):
        self.assertIn("command == 0x31", CPP)
        self.assertIn("payload.size() > 42", CPP)
        self.assertIn("i != 42", CPP)
        self.assertIn("REPORT_TEMP_ACT_BYTE  = 42", (ROOT / "components/sinclair_ac/esppac_cnt.h").read_text())
    def test_polling_waits_for_response_and_uses_report_baseline(self):
        self.assertIn("if (this->wait_response_)", CNT)
        self.assertIn("Poll/command response timed out", CNT)
        self.assertIn("this->last_report_payload_ = payload", CNT)
        self.assertIn("std::copy_n(this->last_report_payload_", CNT)
    def test_pending_command_snapshot_isolated_from_reports_and_verified_semantically(self):
        self.assertIn("struct PendingControlState", (ROOT / "components/sinclair_ac/esppac_cnt.h").read_text())
        self.assertIn("this->pending_control_.mode = *call.get_mode();", CNT)
        self.assertIn("const auto command_mode = encode_pending ? this->pending_control_.mode : this->mode;", CNT)
        self.assertIn("command_fan_mode = this->pending_control_.custom_fan_mode;", CNT)
        self.assertIn("command_fan_mode = this->get_custom_fan_mode().c_str();", CNT)
        self.assertIn("if (++this->pending_control_.retries >= 3)", CNT)
        self.assertIn('"command_failed_mismatch"', CNT)
        self.assertIn('"Command applied: requested fields match report"', CNT)
        self.assertIn('"Command clear verified"', CNT)
    def test_protocol_state_transitions_do_not_flood_healthy_polls(self):
        # Poll-only startup reports initialization, then waiting, then the first valid RX restores ready.
        self.assertIn('this->publish_protocol_state("initializing");', CPP)
        self.assertIn('protocol_state_after_transmit(this->update_, this->state_)', CNT)
        self.assertIn('this->publish_protocol_state("ready");', CPP)

        # Routine polls retain ready; the helper returns no state to publish.
        self.assertNotIn('this->publish_protocol_state("waiting_for_poll_response");', CNT)
        self.assertIn('assert(protocol_state_after_transmit(ACUpdate::NoUpdate, ACState::Ready) == nullptr);', (ROOT / "tests/test_protocol_frame.cpp").read_text())

        # A missed response is visible and a subsequent valid RX recovers ready.
        self.assertIn('this->publish_protocol_state("response_timeout");', CNT)

        # Control transactions retain their explicit apply/clear/verified lifecycle.
        for state in ("command_apply_waiting", "command_clear_waiting"):
            self.assertIn(f'"{state}"', PROTOCOL_STATE)
        self.assertIn('this->publish_protocol_state("command_verified");', CNT)
    def test_discovery_schema_retains_known_and_unknown_raw_payloads(self):
        for key in ("fan_profile", "telemetry_discovery", "last_0x31_payload", "last_0x33_payload", "last_0x44_payload", "last_0x40_payload", "last_unknown_payload"):
            self.assertIn(key, CLIMATE_PY)
        self.assertIn("void SinclairAC::retain_payload", CPP)
    def test_power_off_mode_fallback_handles_every_climate_mode(self):
        self.assertIn("case climate::CLIMATE_MODE_HEAT_COOL:", CNT)
        self.assertIn("case climate::CLIMATE_MODE_OFF:", CNT)
if __name__ == "__main__": unittest.main()
