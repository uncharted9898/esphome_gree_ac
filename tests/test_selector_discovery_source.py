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
        self.assertIn("MODULE_STATE_DISCOVERY_CASES = 8", header)
        self.assertIn("build_rtl_report_query(this->pending_primary_selector_", header)
        self.assertIn("payload_changed_indices", header)
        self.assertIn('CONF_MODULE_STATE_DISCOVERY = "module_state_discovery"', config)
        self.assertIn("module_state_primary_selector", config)
        self.assertIn("QUERY_PROFILE_STATUS", header)
        self.assertIn("PROFILE MAP cmd=0x%02X", header)
        self.assertIn('CONF_OPERATING_PROFILE = "operating_profile"', config)
        self.assertIn("operating_profile_cycles", config)


if __name__ == "__main__":
    unittest.main()
