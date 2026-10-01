from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "components/gree_wired_rs485/gree_wired_rs485.h"
PACKAGE = ROOT / "packages/gree-vireo-xiao-rs485-listen-only.yaml"


class WiredDeploymentContractTests(unittest.TestCase):
    def test_component_has_no_uart_write_call(self):
        text = HEADER.read_text()
        forbidden = (
            r"\bthis->write(?:_byte|_array|_str)?\s*\(",
            r"\bparent_->write(?:_byte|_array|_str)?\s*\(",
        )
        for pattern in forbidden:
            self.assertIsNone(re.search(pattern, text), pattern)

    def test_vireo_package_stays_receive_only_and_field_mapped(self):
        text = PACKAGE.read_text()
        uart_block = text.split("\nuart:\n", 1)[1].split("\nexternal_components:\n", 1)[0]
        self.assertNotIn("tx_pin:", uart_block)
        self.assertNotIn("flow_control_pin:", uart_block)
        self.assertIn("rx_pin: GPIO7", uart_block)
        self.assertIn("passive_scan: false", text)
        self.assertIn("#   pin 3 -> Seeed A", text)
        self.assertIn("#   pin 4 -> Seeed B", text)
        self.assertIn("#   120R termination -> OFF", text)
        self.assertIn("#   5V selector -> IN", text)
        self.assertIn("GPIO4 is forced LOW", text)

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
