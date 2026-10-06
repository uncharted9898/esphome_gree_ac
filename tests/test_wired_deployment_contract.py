from pathlib import Path
import re
import stat
import unittest


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "components/gree_wired_rs485/gree_wired_rs485.h"
REGISTRATION = ROOT / "components/gree_wired_rs485/controller_registration.h"
CONTROLLER = ROOT / "components/gree_wired_rs485/wired_controller_state.h"
PACKAGE = ROOT / "packages/gree-vireo-xiao-rs485-listen-only.yaml"
LEGACY_OVERLAY = ROOT / "packages/gree-vireo-xiao-rs485-legacy-gkh-xk76-probe.yaml"
PASSIVE_SCAN_OVERLAY = ROOT / "packages/gree-vireo-xiao-rs485-passive-profile-scan.yaml"
VALIDATE_SCRIPT = ROOT / "scripts_validate.sh"


class WiredDeploymentContractTests(unittest.TestCase):
    def test_validation_entrypoint_remains_executable(self):
        mode = VALIDATE_SCRIPT.stat().st_mode
        self.assertTrue(
            mode & stat.S_IXUSR,
            "scripts_validate.sh must retain its executable bit for GitHub Actions",
        )

    def test_active_probe_emulates_controller_registration(self):
        text = HEADER.read_text()
        registration = REGISTRATION.read_text()
        controller = CONTROLLER.read_text()
        self.assertIn("should_send_registration_", text)
        self.assertIn("send_registration_", text)
        self.assertIn("registration_attempt_limit_", text)
        self.assertIn("controller::encode", text)
        self.assertIn("registration::counter_for_attempt", text)
        self.assertIn("BODY_LENGTH = 0x22", controller)
        self.assertIn("frame.push_back(0xFF)", controller)
        self.assertIn("frame.push_back(0x00)", controller)
        self.assertIn("frame.push_back(protocol::MESSAGE_TYPE)", controller)
        self.assertIn("set_accept_counter(encoded, accept_counter_value)", controller)
        self.assertIn("controller::encode", registration)
        self.assertIn("this->write_array(frame.data(), frame.size());", text)
        self.assertIn("this->force_receive_mode_();", text)
        self.assertIn('ESP_LOGI(TAG, "RX raw burst', text)

    def test_rx_is_drained_before_followup_registration(self):
        text = HEADER.read_text()
        loop_start = text.index("  void loop() override {")
        rx_pos = text.index("while (this->available())", loop_start)
        finish_pos = text.index("this->finish_registration_response_window_();", loop_start)
        send_pos = text.index("this->send_registration_();", loop_start)

        self.assertLess(rx_pos, finish_pos)
        self.assertLess(finish_pos, send_pos)
        self.assertIn("registration_response_quiet_ms_", text[loop_start:send_pos])
        self.assertIn("this->available() == 0", text[loop_start:send_pos])

    def test_vireo_package_uses_manual_de_and_field_mapping(self):
        text = PACKAGE.read_text()
        uart_block = text.split("\nuart:\n", 1)[1].split("\nexternal_components:\n", 1)[0]
        self.assertIn("tx_pin: GPIO6", uart_block)
        self.assertIn("rx_pin: GPIO7", uart_block)
        self.assertNotIn("flow_control_pin:", uart_block)
        self.assertIn("passive_scan: false", text)
        self.assertIn("active_probe: false", text)
        self.assertIn("legacy_gkh_xk76_probe: false", text)
        self.assertIn("active_probe_interval: 1200ms", text)
        self.assertIn("registration_attempts: 4", text)
        self.assertIn("silent_bootstrap_probe: false", text)
        self.assertIn("silent_bootstrap_delay: 5s", text)
        self.assertIn("hardware_half_duplex: false", text)
        self.assertIn("rx_idle_pullup: true", text)
        self.assertIn("persistent_controller: false", text)
        self.assertIn("last_frame_role:", text)
        self.assertIn("poll_payload:", text)
        self.assertIn("poll_changes:", text)
        self.assertIn("ff40_indexed:", text)
        self.assertIn("registered_setpoint_candidate:", text)
        self.assertIn("Legacy 00-FF Polls Seen", text)
        self.assertIn("Legacy Controller Responses Sent", text)
        self.assertIn("Legacy Registered Status Frames", text)
        self.assertIn("Legacy Registered Setpoint Candidate", text)
        self.assertIn("Legacy FF40 Registered Appendix", text)
        self.assertIn("Legacy Controller State Codec", text)
        self.assertIn("Legacy Registered FF40 Layout", text)
        self.assertIn("#   pin 3 -> Seeed A", text)
        self.assertIn("#   pin 4 -> Seeed B", text)
        self.assertIn("#   120R termination -> OFF", text)
        self.assertIn("#   5V selector -> IN", text)
        self.assertIn("Deterministic manual RS485 direction", text)

    def test_vireo_receiver_path_self_test_is_receive_only(self):
        text = HEADER.read_text()
        config = (ROOT / "components/gree_wired_rs485/__init__.py").read_text()
        package = PACKAGE.read_text()

        self.assertIn("probe_rx_receiver_path_();", text)
        self.assertIn("RO_ACTIVE_HIGH_FAILSAFE_OR_BUS_HIGH", text)
        self.assertIn("RO_ACTIVE_LOW", text)
        self.assertIn("RO_HIGH_Z_OR_DISCONNECTED", text)
        self.assertIn("full fail-safe", text)
        self.assertIn("does not prove bus traffic", text)
        self.assertIn("gpio_pulldown_en(gpio)", text)
        self.assertIn("gpio_pullup_en(gpio)", text)
        self.assertIn("test never enabled RS485 TX", text)
        self.assertNotIn("this->write_array", text[text.index("  void probe_rx_receiver_path_()"):text.index("\n  bool set_direction_level_", text.index("  void probe_rx_receiver_path_()"))])
        self.assertIn('CONF_RX_RECEIVER_PATH = "rx_receiver_path"', config)
        self.assertIn("RS485 Receiver Path", package)

    def test_vireo_protocol_surface_labels_legacy_profile_as_unproven(self):
        text = HEADER.read_text()
        self.assertIn("Legacy reference decode profile", text)
        self.assertIn("XE71/Vireo application protocol is unproven", text)
        self.assertIn("candidate legacy wired profile", text)
        self.assertIn("XE71/Vireo unproven", text)
        self.assertIn("unit=UNLEARNED", text)
        self.assertIn("provenance=legacy_codec_reference", text)
        self.assertIn(
            "provenance=legacy_codec_reference unit=UNLEARNED live_data=NO",
            text,
        )
        self.assertIn("provenance=legacy_codec_staged", text)
        self.assertIn("provenance=learned_legacy_bus", text)
        self.assertIn("provenance=legacy_reference_probe", text)
        self.assertIn("LEGACY_REF:%02X%02X%02X", text)

    def test_vireo_deployment_is_receive_only_after_failed_legacy_probe(self):
        text = PACKAGE.read_text()
        self.assertIn("active_probe: false", text)
        self.assertIn("legacy_gkh_xk76_probe: false", text)
        self.assertIn("silent_bootstrap_probe: false", text)
        self.assertIn("persistent_controller: false", text)
        self.assertIn("rx_idle_pullup: true", text)

    def test_legacy_gkh_xk76_probe_is_an_explicit_second_gate(self):
        text = HEADER.read_text()
        config = (ROOT / "components/gree_wired_rs485/__init__.py").read_text()
        self.assertIn('CONF_LEGACY_GKH_XK76_PROBE = "legacy_gkh_xk76_probe"', config)
        self.assertIn("active_probe transmits the legacy GKH/XK76", config)
        self.assertIn("legacy_gkh_xk76_probe requires active_probe: true", config)
        self.assertIn("persistent_controller uses the legacy GKH/XK76", config)
        self.assertIn("legacy_gkh_xk76_probe_enabled_()", text)
        self.assertIn("active_probe_ && this->legacy_gkh_xk76_probe_", text)
        self.assertIn("TX legacy GKH/XK76 registration", text)

    def test_passive_profile_scan_does_not_lock_on_raw_garbage(self):
        text = HEADER.read_text()
        self.assertIn("PASSIVE_SCAN_PROFILE_COUNT = 12", text)
        self.assertIn("set_passive_scan_start_profile", text)
        self.assertIn("boot_profile=%s", text)
        self.assertIn("apply_scan_profile_(this->scan_profile_index_, false)", text)
        self.assertIn("apply_scan_profile_(this->scan_profile_index_, true)", text)
        self.assertIn("scan_profile_valid_start_", text)
        apply_start = text.index("  void apply_scan_profile_")
        apply_end = text.index("\n  void process_frame_", apply_start)
        apply = text[apply_start:apply_end]
        self.assertIn("if (reload_uart)", apply)
        self.assertIn("this->parent_->load_settings(false);", apply)
        setup_start = text.index("  void setup() override {")
        setup_end = text.index("\n  void on_shutdown()", setup_start)
        setup = text[setup_start:setup_end]
        self.assertNotIn("load_settings(false)", setup)
        self.assertIn("profile_valid_frames", text)
        self.assertIn("if (profile_valid_frames > 0)", text)
        self.assertNotIn("if (profile_bytes >= 4)", text)
        for profile in (
            "1200-8N1", "1200-8E1", "2400-8N1", "2400-8E1",
            "4800-8N1", "4800-8E1", "9600-8N1", "9600-8E1",
            "19200-8N1", "19200-8E1", "38400-8N1", "38400-8E1",
        ):
            self.assertIn(profile, text)

    def test_passive_scan_boot_profile_is_configurable(self):
        config = (ROOT / "components/gree_wired_rs485/__init__.py").read_text()
        self.assertIn(
            'CONF_PASSIVE_SCAN_START_PROFILE = "passive_scan_start_profile"',
            config,
        )
        for profile in (
            "1200-8N1", "1200-8E1", "2400-8N1", "2400-8E1",
            "4800-8N1", "4800-8E1", "9600-8N1", "9600-8E1",
            "19200-8N1", "19200-8E1", "38400-8N1", "38400-8E1",
        ):
            self.assertIn(f'"{profile}"', config)

    def test_passive_profile_scan_overlay_cannot_transmit(self):
        text = PASSIVE_SCAN_OVERLAY.read_text()
        self.assertIn("id: gree_com_manual", text)
        self.assertIn("passive_scan: true", text)
        self.assertIn('gree_wired_scan_start_profile: "1200-8N1"', text)
        self.assertIn("passive_scan_start_profile: ${gree_wired_scan_start_profile}", text)
        self.assertIn("active_probe: false", text)
        self.assertIn("legacy_gkh_xk76_probe: false", text)
        self.assertIn("silent_bootstrap_probe: false", text)
        self.assertIn("persistent_controller: false", text)

    def test_legacy_probe_overlay_requires_explicit_acknowledgement(self):
        text = LEGACY_OVERLAY.read_text()
        self.assertIn("id: gree_com_manual", text)
        self.assertIn("active_probe: true", text)
        self.assertIn("legacy_gkh_xk76_probe: true", text)
        self.assertIn("silent_bootstrap_probe: true", text)
        self.assertIn("persistent_controller: false", text)
        self.assertIn("GKH/XK76", text)
        self.assertIn("not an XE71/Vireo protocol claim", text)

    def test_silent_bootstrap_requires_complete_electrical_silence(self):
        text = HEADER.read_text()
        self.assertIn("registration::silent_bootstrap_ready", text)
        self.assertIn("silent_bootstrap_probe_", text)
        self.assertIn("silent_bootstrap_delay_ms_", text)
        self.assertIn("this->bytes_received_", text)
        self.assertIn("this->rx_line_activity_.total_transitions()", text)
        self.assertIn("registration::REFERENCE_UNIT_SIGNATURE", text)
        self.assertIn("silent_bootstrap_armed_", text)
        self.assertIn("no UART bytes and no RX", text)

    def test_registration_is_target_startup_gated(self):
        text = HEADER.read_text()
        should_start = text.index("  bool should_send_registration_")
        should_end = text.index("\n  void learn_registration_signature_", should_start)
        should = text[should_start:should_end]
        self.assertIn("registration_armed_", should)
        self.assertIn("registration_unit_signature_learned_", should)
        self.assertIn("silent_bootstrap_armed_", should)
        self.assertNotIn("setup_started_at_", should)
        self.assertNotIn("registration_start_delay_ms_", text)

        process_start = text.index("  void process_frame_")
        process_end = text.index("\n  void publish_counters_", process_start)
        process = text[process_start:process_end]
        self.assertIn("learn_registration_signature_(frame)", process)
        self.assertIn("observe_startup_poll_", process)
        self.assertIn(
            "frame.role == protocol::FrameRole::CONTROLLER_POLL_0E",
            process,
        )
        self.assertNotIn(
            "if (frame.route == protocol::RouteKind::ROUTE_00_FF) {\n"
            "      this->observe_startup_poll_",
            process,
        )

        send_start = text.index("  void send_registration_() {")
        send_end = text.index("\n  int read_gpio_level_", send_start)
        send = text[send_start:send_end]
        self.assertIn("controller::encode", send)
        self.assertIn("registration::counter_for_attempt", send)
        self.assertIn("controller::set_accept_counter", send)
        self.assertIn("controller_state_", send)
        learn_start = text.index("  void learn_registration_signature_")
        learn_end = text.index("\n  void observe_startup_poll_", learn_start)
        learn = text[learn_start:learn_end]
        self.assertIn("legacy_gkh_xk76_probe_enabled_()", learn)
        self.assertIn("protocol::FrameRole::INDOOR_STATUS_16", learn)
        self.assertIn("protocol::FrameRole::INDOOR_STATUS_17", learn)
        self.assertNotIn("protocol::FrameRole::INDOOR_STATUS_REGISTERED_29", learn)
        self.assertIn("controller::set_unit_signature", learn)
        self.assertNotIn("REGISTRATION_TEMPLATE", send)
        self.assertIn("armed=%s", text)
        self.assertIn("established=%s unit=%s", text)
        self.assertIn('"UNLEARNED"', text)
        self.assertIn('"LEGACY_REF:%02X%02X%02X"', text)

    def test_passive_controller_codec_does_not_expose_reference_defaults(self):
        text = HEADER.read_text()
        start = text.index("  void publish_controller_state_() {")
        end = text.index("\n  void observe_ff40_status_", start)
        publish = text[start:end]

        passive_marker = (
            'this->controller_state_sensor_->publish_state(\n'
            '          "provenance=legacy_codec_reference unit=UNLEARNED live_data=NO");'
        )
        passive_pos = publish.index(passive_marker)
        passive_return = publish.index("return;", passive_pos)
        detailed_mode = publish.index("mode_power=0x%02X")
        self.assertLess(passive_pos, passive_return)
        self.assertLess(passive_return, detailed_mode)

    def test_registration_success_requires_expanded_ff40(self):
        text = HEADER.read_text()
        process_start = text.index("  void process_frame_")
        process_end = text.index("\n  void publish_counters_", process_start)
        process = text[process_start:process_end]
        self.assertIn("registration_waiting_for_response_", text)
        self.assertIn("ROUTE_FF_40", process)
        self.assertIn("observe_ff40_status_(frame)", process)
        self.assertIn("decoded.registered_layout", text)
        self.assertIn("registered_status_sensor_->publish_state(decoded.registered_layout)", text)
        self.assertIn('decoded.registered_layout && !decoded.appendix.empty()', text)
        self.assertIn("ff40_payload_sensor_", text)
        self.assertIn("ff40_changes_sensor_", text)
        self.assertIn("registration_accept_evidence_ = true", text)
        self.assertNotIn("frame.body_length > 0x17", process)

        finish_start = text.index("  void finish_registration_response_window_")
        finish_end = text.index("\n  void send_registration_", finish_start)
        finish = text[finish_start:finish_end]
        self.assertIn("if (this->registration_accept_evidence_)", finish)
        self.assertNotIn(
            "this->valid_frames_ > this->registration_valid_frames_at_send_",
            finish,
        )

    def test_persistent_controller_runtime_is_opt_in_and_poll_driven(self):
        text = HEADER.read_text()
        package = PACKAGE.read_text()
        self.assertIn("set_persistent_controller", text)
        self.assertIn("persistent_controller_{false}", text)
        self.assertIn("persistent_controller: false", package)
        self.assertIn("controller_polls_seen_", text)
        self.assertIn("controller_responses_sent_", text)
        self.assertIn("runtime_response_pending_", text)

        observe_start = text.index("  void observe_startup_poll_")
        observe_end = text.index("\n  void finish_registration_response_window_", observe_start)
        observe = text[observe_start:observe_end]
        self.assertIn("registration_established_", observe)
        self.assertIn("persistent_controller_", observe)
        self.assertIn("runtime_response_pending_ = true", observe)

        runtime_start = text.index("  void send_runtime_controller_response_")
        runtime_end = text.index("\n  int read_gpio_level_", runtime_start)
        runtime = text[runtime_start:runtime_end]
        self.assertIn("controller::encode", runtime)
        self.assertIn("registration::counter_for_attempt", runtime)
        self.assertIn("controller::set_accept_counter", runtime)
        self.assertIn("accept_counter", runtime)
        self.assertIn("controller_responses_sent_", runtime)

    def test_controller_actions_stage_state_without_direct_tx(self):
        text = HEADER.read_text()
        self.assertIn("class SetControllerSetpointAction", text)
        self.assertIn("class SetControllerModePowerRawAction", text)
        self.assertIn("class SetControllerPowerAction", text)
        self.assertIn("class SetControllerPayloadByteAction", text)
        self.assertIn("set_controller_setpoint_celsius", text)
        self.assertIn("set_controller_mode_power_raw", text)
        self.assertIn("set_controller_power", text)
        self.assertIn("set_controller_payload_byte", text)

        setpoint_start = text.index("class SetControllerSetpointAction")
        power_start = text.index("class SetControllerPowerAction")
        payload_start = text.index("class SetControllerPayloadByteAction")
        mode_start = text.index("class SetControllerModePowerRawAction")
        action_tail = text[setpoint_start:]
        self.assertNotIn("write_array", action_tail)
        self.assertNotIn("flush()", action_tail)
        self.assertLess(setpoint_start, power_start)
        self.assertLess(power_start, payload_start)
        self.assertLess(payload_start, mode_start)

    def test_startup_valid_frames_are_retained_and_replayed(self):
        text = HEADER.read_text()
        self.assertIn("startup_frame_trace_", text)
        self.assertIn("startup_frame_trace_limit_{16}", text)
        self.assertIn("startup_trace_replayed_", text)
        self.assertIn("first_valid_frame_at_", text)
        self.assertIn("startup_trace_replay_delay_ms_{5000}", text)
        self.assertIn("startup_trace_window_complete", text)
        self.assertIn('"STARTUP retained valid frame trace count=%u"', text)
        self.assertIn('"STARTUP retained[%u] %s"', text)

        process_start = text.index("  void process_frame_")
        process_end = text.index("\n  void publish_counters_", process_start)
        process = text[process_start:process_end]
        self.assertIn("startup_frame_trace_.size()", process)
        self.assertIn("frame_role", process)
        self.assertIn("raw_hex", process)

    def test_registration_tx_evidence_is_persisted(self):
        text = HEADER.read_text()
        send_start = text.index("  void send_registration_() {")
        send_end = text.index("\n  int read_gpio_level_", send_start)
        send = text[send_start:send_end]

        start_pos = send.index("const uint32_t tx_started_at = millis();")
        write_pos = send.index("this->write_array(frame.data(), frame.size());")
        flush_pos = send.index("const auto flush_result = this->flush();")
        elapsed_pos = send.index("this->last_registration_tx_elapsed_ms_ =")
        self.assertLess(start_pos, write_pos)
        self.assertLess(write_pos, flush_pos)
        self.assertLess(flush_pos, elapsed_pos)

        self.assertIn("last_registration_tx_seen_", text)
        self.assertIn("last_registration_tx_flush_ok_", text)
        self.assertIn("last_registration_de_before_", text)
        self.assertIn("last_registration_de_after_", text)
        self.assertIn("TX complete registration", send)
        self.assertIn("expected_wire=~333ms", send)
        self.assertIn("tx_ms=%lu tx_flush=%s tx_de=%d>%d", text)

    def test_passive_edge_cadence_survives_wrong_uart_profile(self):
        text = HEADER.read_text()
        self.assertIn("rx_isr_previous_edge_us_", text)
        self.assertIn("rx_isr_cadence_samples_total_", text)
        self.assertIn("rx_isr_cadence_min_gap_us_", text)
        self.assertIn("rx_isr_cadence_max_gap_us_", text)
        self.assertIn("rx_isr_cadence_last_gap_us_", text)
        self.assertIn("cadence_gap_us <= 20000U", text)
        self.assertIn("edge_cadence_samples=%lu", text)
        self.assertIn("edge_min_gap_us=%lu", text)
        self.assertIn("edge_max_gap_us=%lu", text)
        self.assertIn("edge_last_gap_us=%lu", text)

    def test_registration_rx_window_records_turnaround_evidence(self):
        text = HEADER.read_text()
        self.assertIn('#include "registration_rx_window.h"', text)
        self.assertIn("registration_rx_window_.observe_byte", text)
        self.assertIn("registration_tx_residue_capture_", text)
        self.assertIn("pending_after_release", text)
        self.assertIn("arm_to_de_us=%lu", text)
        self.assertIn("de_to_probe_us=%lu", text)
        self.assertIn('"REG RX window opened attempt=%u pending_after_release=%u "', text)
        self.assertIn('"REG window %u/%u unvalidated_rx=%u pending_after_release=%u "', text)
        self.assertIn("tx_rx_edges=%lu", text)
        self.assertIn("turnaround_edges=%lu", text)
        self.assertIn("phase_gap_edges=%lu", text)
        self.assertIn("tx_residue=%u:%s", text)
        self.assertIn("first_drain_us=%lu", text)
        self.assertIn("last_drain_us=%lu", text)
        self.assertIn("drain_span_us=%lu", text)
        self.assertIn("valid_delta=%lu", text)
        self.assertIn("rx_edges=%lu", text)
        self.assertIn("first_edge_us=%lu", text)
        self.assertIn("edge_span_us=%lu", text)
        self.assertIn("edge_min_gap_us=%lu", text)
        self.assertIn("edge_max_gap_us=%lu", text)
        self.assertIn("registration_attempt_trace_", text)
        self.assertIn("REG retained window trace count=%u", text)
        self.assertIn("registration_trace_replay_delay_ms_{30000}", text)
        self.assertIn("setup_rx_edge_monitor_", text)
        self.assertIn("GPIO_INTR_ANYEDGE", text)
        self.assertIn("rx_edge_isr_", text)
        self.assertIn("rx_transition_total_()", text)
        self.assertIn("tx_rx_edges_window", text)
        self.assertIn("turnaround_edges_window", text)
        self.assertIn("tx_residue_window", text)
        self.assertIn("rx_edge_response_phase_", text)
        self.assertIn("rx_edge_response_count_", text)
        self.assertIn("gpio_pullup_en(gpio)", text)
        self.assertIn("gpio_pulldown_dis(gpio)", text)
        self.assertNotIn('"REG response %u/%u bytes=%u: %s"', text)

        send_start = text.index("  void send_registration_() {")
        send_end = text.index("\n  void send_runtime_controller_response_", send_start)
        send = text[send_start:send_end]
        flush_pos = send.index("const auto flush_result = this->flush();")
        residue_pos = send.index("const size_t tx_residue_pending = this->available();")
        arm_pos = send.index("const uint32_t rx_window_armed_at_us = micros();")
        release_pos = send.index("!this->set_direction_level_(0)")
        released_at_pos = send.index("const uint32_t de_released_at_us = micros();")
        pending_pos = send.index("const size_t pending_after_release = this->available();")
        open_pos = send.index("this->registration_rx_window_.open(")
        complete_log_pos = send.index('"TX complete registration %u/%u')
        self.assertLess(flush_pos, residue_pos)
        self.assertLess(residue_pos, arm_pos)
        self.assertLess(arm_pos, release_pos)
        self.assertLess(release_pos, released_at_pos)
        self.assertLess(released_at_pos, pending_pos)
        self.assertLess(pending_pos, open_pos)
        self.assertLess(open_pos, complete_log_pos)

    def test_hardware_half_duplex_does_not_manual_toggle_de(self):
        text = HEADER.read_text()
        self.assertIn("hardware_half_duplex_", text)
        self.assertIn('this->hardware_half_duplex_ ? "UART_RS485_HALF_DUPLEX" : "MANUAL_GPIO"', text)
        self.assertIn("if (!this->hardware_half_duplex_ && !this->set_direction_level_(1))", text)
        self.assertIn("if (!this->hardware_half_duplex_) {", text)


if __name__ == "__main__":
    unittest.main()
