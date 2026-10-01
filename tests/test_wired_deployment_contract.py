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
        self.assertIn("flow_control_pin: GPIO4", text)
        self.assertIn("passive_scan: false", text)
        self.assertIn("#   pin 3 -> Seeed A", text)
        self.assertIn("#   pin 4 -> Seeed B", text)
        self.assertIn("#   120R termination -> OFF", text)
        self.assertIn("#   5V selector -> IN", text)


if __name__ == "__main__":
    unittest.main()
