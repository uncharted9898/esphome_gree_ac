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
        rx_pos = text.index("while (this->available())", loop_start)
        finish_pos = text.index("this->finish_registration_response_window_();", loop_start)
        send_pos = text.index("this->send_registration_();", loop_start)

        self.assertLess(rx_pos, finish_pos)
        self.assertLess(finish_pos, send_pos)
        self.assertIn("registration_response_quiet_ms_", text[loop_start:send_pos])
        self.assertIn("this->available() == 0", text[loop_start:send_pos])

    def test_vireo_package_uses_uart_half_duplex_and_field_mapping(self):
        text = PACKAGE.read_text()
        uart_block = text.split("\nuart:\n", 1)[1].split("\nexternal_components:\n", 1)[0]
        self.assertIn("tx_pin: GPIO6", uart_block)
        self.assertIn("rx_pin: GPIO7", uart_block)
        self.assertIn("flow_control_pin: GPIO4", uart_block)
        self.assertIn("passive_scan: false", text)
        self.assertIn("active_probe: true", text)
        self.assertIn("active_probe_interval: 1200ms", text)
        self.assertIn("registration_attempts: 4", text)
        self.assertIn("hardware_half_duplex: true", text)
        self.assertIn("#   pin 3 -> Seeed A", text)
        self.assertIn("#   pin 4 -> Seeed B", text)
        self.assertIn("#   120R termination -> OFF", text)
        self.assertIn("#   5V selector -> IN", text)
        self.assertIn("ESP-IDF hardware half-duplex RS485", text)

    def test_hardware_half_duplex_does_not_manual_toggle_de(self):
        text = HEADER.read_text()
        self.assertIn("hardware_half_duplex_", text)
        self.assertIn('this->hardware_half_duplex_ ? "UART_RS485_HALF_DUPLEX" : "MANUAL_GPIO"', text)
        self.assertIn("if (!this->hardware_half_duplex_ && !this->set_direction_level_(1))", text)
        self.assertIn("if (!this->hardware_half_duplex_) {", text)


if __name__ == "__main__":
    unittest.main()
