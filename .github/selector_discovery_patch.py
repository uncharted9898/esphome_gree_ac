from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"missing patch anchor in {path}: {old[:80]!r}")
    p.write_text(text.replace(old, new, 1))


# ESPHome configuration: intentionally opt-in because a full 8x8 sweep takes
# roughly one minute at the conservative 900 ms response window.
replace_once(
    "components/gree_oem_boot_probe/__init__.py",
    'CONF_SCHEDULED_QUIESCE_DELAY = "scheduled_quiesce_delay"\n',
    'CONF_SCHEDULED_QUIESCE_DELAY = "scheduled_quiesce_delay"\nCONF_SELECTOR_DISCOVERY = "selector_discovery"\n',
)
replace_once(
    "components/gree_oem_boot_probe/__init__.py",
    '        cv.Optional(CONF_REPEAT_INTERVAL): cv.All(\n',
    '        cv.Optional(CONF_SELECTOR_DISCOVERY, default=False): cv.boolean,\n        cv.Optional(CONF_REPEAT_INTERVAL): cv.All(\n',
)
replace_once(
    "components/gree_oem_boot_probe/__init__.py",
    '    cg.add(var.set_scheduled_quiesce_delay(config[CONF_SCHEDULED_QUIESCE_DELAY]))\n',
    '    cg.add(var.set_scheduled_quiesce_delay(config[CONF_SCHEDULED_QUIESCE_DELAY]))\n    cg.add(var.set_selector_discovery(config[CONF_SELECTOR_DISCOVERY]))\n',
)

# Expose a monotonically increasing receive generation and last command so the
# probe can correlate requests with any checksum-valid response, including a
# command that has never been decoded before.
replace_once(
    "components/sinclair_ac/esppac.h",
    '        uint32_t get_retained_payload_generation(uint8_t command) const {\n            const auto it = this->payload_generations_.find(command);\n            return it == this->payload_generations_.end() ? 0 : it->second;\n        }\n',
    '        uint32_t get_retained_payload_generation(uint8_t command) const {\n            const auto it = this->payload_generations_.find(command);\n            return it == this->payload_generations_.end() ? 0 : it->second;\n        }\n        uint32_t get_retained_payload_total_generation() const {\n            return this->payload_total_generation_;\n        }\n        uint8_t get_last_retained_command() const { return this->last_retained_command_; }\n',
)
replace_once(
    "components/sinclair_ac/esppac.h",
    '        std::map<uint8_t, uint32_t> payload_generations_;\n',
    '        std::map<uint8_t, uint32_t> payload_generations_;\n        uint32_t payload_total_generation_{0};\n        uint8_t last_retained_command_{0};\n',
)
replace_once(
    "components/sinclair_ac/esppac.cpp",
    '    ++this->payload_generations_[command];\n',
    '    ++this->payload_generations_[command];\n    ++this->payload_total_generation_;\n    this->last_retained_command_ = command;\n',
)

# Probe implementation.
path = Path("components/gree_oem_boot_probe/gree_oem_boot_probe.h")
text = path.read_text()
text = text.replace('#include <cstdint>\n', '#include <cstdint>\n#include <cstdio>\n', 1)
text = text.replace(
    '  void set_scheduled_quiesce_delay(uint32_t delay_ms) { this->scheduled_quiesce_delay_ms_ = delay_ms; }\n',
    '  void set_scheduled_quiesce_delay(uint32_t delay_ms) { this->scheduled_quiesce_delay_ms_ = delay_ms; }\n  void set_selector_discovery(bool enabled) { this->selector_discovery_ = enabled; }\n',
    1,
)
text = text.replace(
    '      case Phase::QUERY_ENERGY:\n        if (this->should_query_energy_()) {\n          this->energy_discovery_attempted_ = true;\n          this->send_query_and_advance_(QUERY_ENERGY_MONTH, 0x40,\n                                        "secondary electrical selector bit 2 -> 0x40",\n                                        Phase::RESPONSE_DRAIN);\n        } else {\n          this->finish_pending_query_();\n          ESP_LOGI(TAG, "Skipping 0x40: 0x32 does not advertise ElcEn");\n          this->phase_ = Phase::RESPONSE_DRAIN;\n          this->next_action_at_ = now + RESPONSE_DRAIN_MS;\n        }\n        break;\n',
    '      case Phase::QUERY_ENERGY:\n        if (this->should_query_energy_()) {\n          this->energy_discovery_attempted_ = true;\n          this->send_query_and_advance_(QUERY_ENERGY_MONTH, 0x40,\n                                        "secondary electrical selector bit 2 -> 0x40",\n                                        Phase::QUERY_SELECTOR_DISCOVERY);\n        } else {\n          this->finish_pending_query_();\n          ESP_LOGI(TAG, "Skipping 0x40: 0x32 does not advertise ElcEn");\n          this->phase_ = Phase::QUERY_SELECTOR_DISCOVERY;\n          this->next_action_at_ = now;\n        }\n        break;\n      case Phase::QUERY_SELECTOR_DISCOVERY:\n        if (!this->selector_discovery_) {\n          this->finish_pending_query_();\n          this->phase_ = Phase::RESPONSE_DRAIN;\n          this->next_action_at_ = now + RESPONSE_DRAIN_MS;\n        } else if (this->selector_discovery_index_ >= SELECTOR_DISCOVERY_CASES) {\n          this->finish_pending_query_();\n          ESP_LOGI(TAG, "Read-only selector discovery complete: %u combinations tested",\n                   static_cast<unsigned>(SELECTOR_DISCOVERY_CASES));\n          this->phase_ = Phase::RESPONSE_DRAIN;\n          this->next_action_at_ = now + RESPONSE_DRAIN_MS;\n        } else {\n          this->send_next_selector_discovery_query_();\n        }\n        break;\n',
    1,
)
text = text.replace(
    '    ESP_LOGCONFIG(TAG, "  Recovered report queries: %s", YESNO(this->query_recovered_data_));\n',
    '    ESP_LOGCONFIG(TAG, "  Recovered report queries: %s", YESNO(this->query_recovered_data_));\n    ESP_LOGCONFIG(TAG, "  Exhaustive read-only selector discovery: %s", YESNO(this->selector_discovery_));\n',
    1,
)
text = text.replace(
    '    QUERY_ENERGY,\n    RESPONSE_DRAIN,\n',
    '    QUERY_ENERGY,\n    QUERY_SELECTOR_DISCOVERY,\n    RESPONSE_DRAIN,\n',
    1,
)
text = text.replace(
    '    this->pending_query_active_ = false;\n    this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::RECEIVE_ONLY);\n',
    '    this->pending_query_active_ = false;\n    this->pending_any_response_ = false;\n    this->selector_discovery_index_ = 0;\n    this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::RECEIVE_ONLY);\n',
    1,
)
text = text.replace(
    '  void finish_pending_query_() {\n    if (!this->pending_query_active_) return;\n\n    const uint32_t expected_after =\n',
    '  void finish_pending_query_() {\n    if (!this->pending_query_active_) return;\n\n    if (this->pending_any_response_) {\n      const uint32_t total_after = this->climate_->get_retained_payload_total_generation();\n      if (total_after != this->pending_total_generation_) {\n        const uint8_t command = this->climate_->get_last_retained_command();\n        const auto *payload = this->climate_->get_retained_payload(command);\n        ESP_LOGI(TAG,\n                 "SELECTOR RX primary=0x%02X secondary=0x%02X state=0x%02X -> "\n                 "cmd=0x%02X payload=%u bytes",\n                 this->pending_primary_selector_, this->pending_secondary_selector_,\n                 this->pending_module_state_, command,\n                 payload == nullptr ? 0U : static_cast<unsigned>(payload->size()));\n      } else {\n        ESP_LOGW(TAG,\n                 "SELECTOR RX timeout primary=0x%02X secondary=0x%02X state=0x%02X",\n                 this->pending_primary_selector_, this->pending_secondary_selector_,\n                 this->pending_module_state_);\n      }\n      this->pending_any_response_ = false;\n      this->pending_query_active_ = false;\n      return;\n    }\n\n    const uint32_t expected_after =\n',
    1,
)
insert_anchor = '  template<size_t N>\n  void send_query_and_advance_(const std::array<uint8_t, N> &frame, uint8_t expected_command,\n'
insert_code = '''  void send_next_selector_discovery_query_() {
    this->finish_pending_query_();
    const uint8_t primary = static_cast<uint8_t>(this->selector_discovery_index_ / 8U);
    const uint8_t secondary = static_cast<uint8_t>(this->selector_discovery_index_ % 8U);
    ++this->selector_discovery_index_;

    this->pending_primary_selector_ = primary;
    this->pending_secondary_selector_ = secondary;
    this->pending_module_state_ = 0x01;
    this->pending_total_generation_ = this->climate_->get_retained_payload_total_generation();
    this->pending_any_response_ = true;
    this->pending_query_active_ = true;

    std::snprintf(this->selector_description_, sizeof(this->selector_description_),
                  "read-only selector primary=0x%02X secondary=0x%02X state=0x01",
                  primary, secondary);
    const auto frame = build_rtl_report_query(primary, secondary, 0x3B, 0x01);
    this->send_(frame, this->selector_description_);
    this->phase_ = Phase::QUERY_SELECTOR_DISCOVERY;
    this->next_action_at_ = millis() + this->query_spacing_ms_();
  }

'''
if insert_anchor not in text:
    raise SystemExit("missing send_query insertion anchor")
text = text.replace(insert_anchor, insert_code + insert_anchor, 1)
text = text.replace(
    '  static constexpr uint32_t RESPONSE_DRAIN_MS = 1500;\n',
    '  static constexpr uint32_t RESPONSE_DRAIN_MS = 1500;\n  static constexpr uint8_t SELECTOR_DISCOVERY_CASES = 64;\n',
    1,
)
text = text.replace(
    '  uint32_t pending_status_generation_{0};\n',
    '  uint32_t pending_status_generation_{0};\n  uint32_t pending_total_generation_{0};\n',
    1,
)
text = text.replace(
    '  const char *pending_description_{nullptr};\n',
    '  const char *pending_description_{nullptr};\n  char selector_description_[80]{};\n',
    1,
)
text = text.replace(
    '  uint8_t pending_expected_command_{0};\n',
    '  uint8_t pending_expected_command_{0};\n  uint8_t selector_discovery_index_{0};\n  uint8_t pending_primary_selector_{0};\n  uint8_t pending_secondary_selector_{0};\n  uint8_t pending_module_state_{0};\n',
    1,
)
text = text.replace(
    '  bool query_recovered_data_{true};\n',
    '  bool query_recovered_data_{true};\n  bool selector_discovery_{false};\n',
    1,
)
text = text.replace(
    '  bool pending_query_active_{false};\n',
    '  bool pending_query_active_{false};\n  bool pending_any_response_{false};\n',
    1,
)
path.write_text(text)

# Lightweight source-level regression test. The existing suite also compiles
# rtl_query.h independently with -Wall -Wextra -Werror.
Path("tests/test_selector_discovery_source.py").write_text('''import pathlib\nimport unittest\n\n\nclass SelectorDiscoverySourceTest(unittest.TestCase):\n    def test_read_only_exhaustive_selector_sweep_is_wired(self):\n        root = pathlib.Path(__file__).resolve().parents[1]\n        header = (root / "components/gree_oem_boot_probe/gree_oem_boot_probe.h").read_text()\n        config = (root / "components/gree_oem_boot_probe/__init__.py").read_text()\n        self.assertIn("SELECTOR_DISCOVERY_CASES = 64", header)\n        self.assertIn("build_rtl_report_query(primary, secondary, 0x3B, 0x01)", header)\n        self.assertIn("get_retained_payload_total_generation", header)\n        self.assertIn('CONF_SELECTOR_DISCOVERY = "selector_discovery"', config)\n        self.assertIn("default=False", config)\n\n\nif __name__ == "__main__":\n    unittest.main()\n''')
