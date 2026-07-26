import pathlib
import unittest


class SelectorDiscoverySourceTest(unittest.TestCase):
    def test_read_only_exhaustive_selector_sweep_is_wired(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        header = (root / "components/gree_oem_boot_probe/gree_oem_boot_probe.h").read_text()
        config = (root / "components/gree_oem_boot_probe/__init__.py").read_text()
        self.assertIn("SELECTOR_DISCOVERY_CASES = 64", header)
        self.assertIn("build_rtl_report_query(primary, secondary, 0x01)", header)
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

    def test_rtl_startup_uses_full_command_03_envelope(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        header = (root / "components/gree_oem_boot_probe/gree_oem_boot_probe.h").read_text()
        query = (root / "components/gree_oem_boot_probe/rtl_query.h").read_text()
        self.assertIn("build_rtl_startup_sync", query)
        self.assertIn("RTL startup command 0x03 #1/4", header)
        self.assertIn("RTL startup command 0x03 #4/4", header)
        self.assertNotIn("LINK_SYNC_CONNECTED", header)
        self.assertNotIn("std::array<uint8_t, 17>", header)

    def test_energy_flow_path_and_research_sweep_are_wired(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        header = (root / "components/gree_oem_boot_probe/gree_oem_boot_probe.h").read_text()
        query = (root / "components/gree_oem_boot_probe/rtl_query.h").read_text()
        config = (root / "components/gree_oem_boot_probe/__init__.py").read_text()
        example = (root / "examples/gree-livo-gen3-full-power-discovery.yaml").read_text()
        self.assertIn("build_rtl_energy_flow_query", query)
        self.assertIn("QUERY_ENERGY_FLOW_FRAME", header)
        self.assertIn("pending_expected_command_ == 0x53", header)
        self.assertIn('CONF_QUERY_ENERGY_FLOW = "query_energy_flow"', config)
        self.assertIn(
            'CONF_FORCE_ELECTRICAL_PAGE_DISCOVERY = "force_electrical_page_discovery"',
            config,
        )
        self.assertIn('CONF_FORCE_ENERGY_FLOW_DISCOVERY = "force_energy_flow_discovery"', config)
        self.assertIn("force_electrical_page_discovery: true", example)
        self.assertIn("force_energy_flow_discovery: true", example)
        self.assertIn("selector_discovery: true", example)
        self.assertIn("module_state_discovery: true", example)

    def test_firmware_audit_is_read_only_and_covers_energy_flow(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        workflow = (root / ".github/workflows/rtl8720cf-firmware-audit.yml").read_text()
        self.assertIn("permissions:\n  contents: read", workflow)
        self.assertNotIn("contents: write", workflow)
        self.assertNotIn("git push", workflow)
        self.assertIn("53-byte EnergyFlow command-0x09 builder signature", workflow)
        self.assertIn("grep -q 'EnergyFlow'", workflow)


if __name__ == "__main__":
    unittest.main()
