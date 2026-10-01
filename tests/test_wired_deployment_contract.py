from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "components/gree_wired_rs485/gree_wired_rs485.h"
PACKAGE = ROOT / "packages/gree-vireo-xiao-rs485-listen-only.yaml"


class WiredDeploymentContractTests(unittest.TestCase):
    def test_active_probe_is_bounded_and_uses_recovered_discovery_frame(self):
        text = HEADER.read_text()
        self.assertIn("if (!this->active_probe_ || this->active_probe_sent_) return false;", text)
        self.assertIn("static const uint8_t DISCOVERY_POLL[]", text)
        self.assertIn(
            "0x7E, 0x7E, 0x00, 0xFF, 0x11, 0x0E",
            text,
        )
        self.assertIn("this->write_array(DISCOVERY_POLL, sizeof(DISCOVERY_POLL));", text)
        self.assertIn("this->force_receive_mode_();", text)

    def test_vireo_package_uses_software_direction_and_field_mapping(self):
        text = PACKAGE.read_text()
        uart_block = text.split("\nuart:\n", 1)[1].split("\nexternal_components:\n", 1)[0]
        self.assertIn("tx_pin: GPIO6", uart_block)
        self.assertNotIn("flow_control_pin:", uart_block)
        self.assertIn("rx_pin: GPIO7", uart_block)
        self.assertIn("passive_scan: false", text)
        self.assertIn("active_probe: true", text)
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
