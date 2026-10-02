from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "components/gree_wired_rs485/gree_wired_rs485.h"
REGISTRATION = ROOT / "components/gree_wired_rs485/controller_registration.h"
CONTROLLER = ROOT / "components/gree_wired_rs485/wired_controller_state.h"
PACKAGE = ROOT / "packages/gree-vireo-xiao-rs485-listen-only.yaml"


class WiredDeploymentContractTests(unittest.TestCase):
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
        self.assertIn("active_probe: true", text)
        self.assertIn("active_probe_interval: 1200ms", text)
        self.assertIn("registration_attempts: 4", text)
        self.assertIn("silent_bootstrap_probe: true", text)
        self.assertIn("silent_bootstrap_delay: 5s", text)
        self.assertIn("hardware_half_duplex: false", text)
        self.assertIn("persistent_controller: false", text)
        self.assertIn("last_frame_role:", text)
        self.assertIn("poll_payload:", text)
        self.assertIn("poll_changes:", text)
        self.assertIn("ff40_indexed:", text)
        self.assertIn("registered_setpoint_candidate:", text)
        self.assertIn("#   pin 3 -> Seeed A", text)
        self.assertIn("#   pin 4 -> Seeed B", text)
        self.assertIn("#   120R termination -> OFF", text)
        self.assertIn("#   5V selector -> IN", text)
        self.assertIn("Deterministic manual RS485 direction", text)

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
        self.assertIn("ROUTE_00_FF", process)

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
        self.assertIn("controller::set_unit_signature", learn)
        self.assertNotIn("REGISTRATION_TEMPLATE", send)
        self.assertIn("armed=%s", text)
        self.assertIn("sig=%s unit=%02X%02X%02X", text)

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

    def test_registration_rx_window_records_turnaround_evidence(self):
        text = HEADER.read_text()
        self.assertIn('#include "registration_rx_window.h"', text)
        self.assertIn("registration_rx_window_.observe_byte", text)
        self.assertIn("pending_at_rx_enable", text)
        self.assertIn('"REG RX window opened attempt=%u pending=%u probe_us=%lu de=%d"', text)
        self.assertIn('"REG window %u/%u unvalidated_rx=%u pending_at_rx_enable=%u "', text)
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
        self.assertNotIn('"REG response %u/%u bytes=%u: %s"', text)

    def test_hardware_half_duplex_does_not_manual_toggle_de(self):
        text = HEADER.read_text()
        self.assertIn("hardware_half_duplex_", text)
        self.assertIn('this->hardware_half_duplex_ ? "UART_RS485_HALF_DUPLEX" : "MANUAL_GPIO"', text)
        self.assertIn("if (!this->hardware_half_duplex_ && !this->set_direction_level_(1))", text)
        self.assertIn("if (!this->hardware_half_duplex_) {", text)


if __name__ == "__main__":
    unittest.main()
