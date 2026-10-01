from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "components/gree_wired_rs485/gree_wired_rs485.h"
PACKAGE = ROOT / "packages/gree-vireo-xiao-rs485-listen-only.yaml"


class WiredDeploymentContractTests(unittest.TestCase):
    def test_active_probe_emulates_controller_registration(self):
        text = HEADER.read_text()
        self.assertIn("should_send_registration_", text)
        self.assertIn("send_registration_", text)
        self.assertIn("registration_attempt_limit_", text)
        self.assertIn("0x7E, 0x7E, 0xFF, 0x00, 0x11, 0x22", text)
        self.assertIn("frame[26]", text)
        self.assertIn("this->write_array(frame.data(), frame.size());", text)
        self.assertIn("this->force_receive_mode_();", text)
        self.assertIn('ESP_LOGI(TAG, "RX raw burst', text)

    def test_rx_is_drained_before_followup_registration(self):
        text = HEADER.read_text()
        loop_start = text.index("  void loop() override {")
        loop_end = text.index("\n  void dump_config() override", loop_start) if "\n  void dump_config() override" in text[loop_start:] else text.index("\n private:", loop_start)
        loop = text[loop_start:loop_end]

        rx_pos = loop.index("while (this->available())")
        finish_pos = loop.index("this->finish_registration_response_window_();")
        send_pos = loop.index("this->send_registration_();")
        self.assertLess(rx_pos, finish_pos)
        self.assertLess(finish_pos, send_pos)
        self.assertIn("registration_response_quiet_ms_", loop)
        self.assertIn("this->available() == 0", loop)

    def test_vireo_package_uses_software_direction_and_field_mapping(self):
        text = PACKAGE.read_text()
        uart_block = text.split("\nuart:\n", 1)[1].split("\nexternal_components:\n", 1)[0]
        self.assertIn("tx_pin: GPIO6", uart_block)
        self.assertNotIn("flow_control_pin:", uart_block)
        self.assertIn("rx_pin: GPIO7", uart_block)
        self.assertIn("passive_scan: false", text)
        self.assertIn("active_probe: true", text)
        self.assertIn("active_probe_interval: 1200ms", text)
        self.assertIn("registration_attempts: 4", text)
        self.assertIn("#   pin 3 -> Seeed A", text)
        self.assertIn("#   pin 4 -> Seeed B", text)
        self.assertIn("#   120R termination -> OFF", text)
        self.assertIn("#   5V selector -> IN", text)
        self.assertIn("GPIO4 is NOT registered as UART RTS/flow-control", text)

    def test_direction_guard_runs_before_uart_and_reasserts_low(self):
        text = HEADER.read_text()
        self.assertIn(
            "return setup_priority::POWER - 1.0f;",
            text,
        )
        self.assertIn("this->force_receive_mode_();", text)
        self.assertIn("gpio_set_direction(gpio, GPIO_MODE_OUTPUT)", text)
        self.assertGreaterEqual(text.count("gpio_set_level(gpio, 0)"), 2)


if __name__ == "__main__":
    unittest.main()
