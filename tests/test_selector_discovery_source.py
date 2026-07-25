import pathlib
import unittest


class SelectorDiscoverySourceTest(unittest.TestCase):
    def test_read_only_exhaustive_selector_sweep_is_wired(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        header = (root / "components/gree_oem_boot_probe/gree_oem_boot_probe.h").read_text()
        config = (root / "components/gree_oem_boot_probe/__init__.py").read_text()
        self.assertIn("SELECTOR_DISCOVERY_CASES = 64", header)
        self.assertIn("build_rtl_report_query(primary, secondary, 0x3B, 0x01)", header)
        self.assertIn("get_retained_payload_total_generation", header)
        self.assertIn('CONF_SELECTOR_DISCOVERY = "selector_discovery"', config)
        self.assertIn("default=False", config)


if __name__ == "__main__":
    unittest.main()
