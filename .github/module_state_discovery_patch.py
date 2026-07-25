from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"missing patch anchor in {path}: {old[:120]!r}")
    p.write_text(text.replace(old, new, 1))


# Configuration: state discovery is independent and opt-in. The selected page
# defaults to primary 0x04 / secondary 0x00, which is the live outdoor 0x35 page.
replace_once(
    "components/gree_oem_boot_probe/__init__.py",
    'CONF_SELECTOR_DISCOVERY = "selector_discovery"\n',
    'CONF_SELECTOR_DISCOVERY = "selector_discovery"\n'
    'CONF_MODULE_STATE_DISCOVERY = "module_state_discovery"\n'
    'CONF_MODULE_STATE_PRIMARY_SELECTOR = "module_state_primary_selector"\n'
    'CONF_MODULE_STATE_SECONDARY_SELECTOR = "module_state_secondary_selector"\n',
)
replace_once(
    "components/gree_oem_boot_probe/__init__.py",
    '        cv.Optional(CONF_SELECTOR_DISCOVERY, default=False): cv.boolean,\n',
    '        cv.Optional(CONF_SELECTOR_DISCOVERY, default=False): cv.boolean,\n'
    '        cv.Optional(CONF_MODULE_STATE_DISCOVERY, default=False): cv.boolean,\n'
    '        cv.Optional(CONF_MODULE_STATE_PRIMARY_SELECTOR, default=0x04): cv.int_range(min=0, max=7),\n'
    '        cv.Optional(CONF_MODULE_STATE_SECONDARY_SELECTOR, default=0x00): cv.int_range(min=0, max=7),\n',
)
replace_once(
    "components/gree_oem_boot_probe/__init__.py",
    '    cg.add(var.set_selector_discovery(config[CONF_SELECTOR_DISCOVERY]))\n',
    '    cg.add(var.set_selector_discovery(config[CONF_SELECTOR_DISCOVERY]))\n'
    '    cg.add(var.set_module_state_discovery(config[CONF_MODULE_STATE_DISCOVERY]))\n'
    '    cg.add(var.set_module_state_selectors(\n'
    '        config[CONF_MODULE_STATE_PRIMARY_SELECTOR],\n'
    '        config[CONF_MODULE_STATE_SECONDARY_SELECTOR],\n'
    '    ))\n',
)

# Standalone payload fingerprint/difference utility so its behavior can be
# compiled and tested without ESPHome headers.
Path("components/gree_oem_boot_probe/payload_fingerprint.h").write_text(r'''#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace esphome {
namespace gree_oem_probe {

inline uint32_t payload_fnv1a(const std::vector<uint8_t> &payload) {
  uint32_t hash = 2166136261UL;
  for (const uint8_t value : payload) {
    hash ^= value;
    hash *= 16777619UL;
  }
  return hash;
}

inline std::string payload_changed_indices(const std::vector<uint8_t> &baseline,
                                           const std::vector<uint8_t> &current,
                                           size_t maximum_entries = 24) {
  std::string result;
  size_t emitted = 0;
  const size_t common_size = std::min(baseline.size(), current.size());

  for (size_t index = 0; index < common_size; ++index) {
    if (baseline[index] == current[index]) continue;
    if (emitted >= maximum_entries) {
      if (!result.empty()) result += ',';
      result += "...";
      return result;
    }
    char token[24];
    std::snprintf(token, sizeof(token), "%u", static_cast<unsigned>(index));
    if (!result.empty()) result += ',';
    result += token;
    ++emitted;
  }

  if (baseline.size() != current.size()) {
    char token[48];
    std::snprintf(token, sizeof(token), "size:%u->%u",
                  static_cast<unsigned>(baseline.size()),
                  static_cast<unsigned>(current.size()));
    if (!result.empty()) result += ',';
    result += token;
  }

  return result.empty() ? "none" : result;
}

}  // namespace gree_oem_probe
}  // namespace esphome
''')

path = Path("components/gree_oem_boot_probe/gree_oem_boot_probe.h")
text = path.read_text()

text = text.replace(
    '#include <cstdio>\n',
    '#include <cstdio>\n#include <map>\n#include <string>\n#include <vector>\n',
    1,
)
text = text.replace(
    '#include "rtl_query.h"\n',
    '#include "payload_fingerprint.h"\n#include "rtl_query.h"\n',
    1,
)
text = text.replace(
    '  void set_selector_discovery(bool enabled) { this->selector_discovery_ = enabled; }\n',
    '  void set_selector_discovery(bool enabled) { this->selector_discovery_ = enabled; }\n'
    '  void set_module_state_discovery(bool enabled) { this->module_state_discovery_ = enabled; }\n'
    '  void set_module_state_selectors(uint8_t primary, uint8_t secondary) {\n'
    '    this->module_state_primary_selector_ = primary;\n'
    '    this->module_state_secondary_selector_ = secondary;\n'
    '  }\n',
    1,
)

old_selector_phase = '''      case Phase::QUERY_SELECTOR_DISCOVERY:
        if (!this->selector_discovery_) {
          this->finish_pending_query_();
          this->phase_ = Phase::RESPONSE_DRAIN;
          this->next_action_at_ = now + RESPONSE_DRAIN_MS;
        } else if (this->selector_discovery_index_ >= SELECTOR_DISCOVERY_CASES) {
          this->finish_pending_query_();
          ESP_LOGI(TAG, "Read-only selector discovery complete: %u combinations tested",
                   static_cast<unsigned>(SELECTOR_DISCOVERY_CASES));
          this->phase_ = Phase::RESPONSE_DRAIN;
          this->next_action_at_ = now + RESPONSE_DRAIN_MS;
        } else {
          this->send_next_selector_discovery_query_();
        }
        break;
'''
new_selector_phase = '''      case Phase::QUERY_SELECTOR_DISCOVERY:
        if (this->selector_discovery_ &&
            this->selector_discovery_index_ < SELECTOR_DISCOVERY_CASES) {
          this->send_next_selector_discovery_query_();
        } else {
          this->finish_pending_query_();
          if (this->selector_discovery_) {
            ESP_LOGI(TAG, "Read-only selector discovery complete: %u combinations tested",
                     static_cast<unsigned>(SELECTOR_DISCOVERY_CASES));
          }
          if (this->module_state_discovery_) {
            this->begin_module_state_discovery_();
          } else {
            this->phase_ = Phase::RESPONSE_DRAIN;
            this->next_action_at_ = now + RESPONSE_DRAIN_MS;
          }
        }
        break;
      case Phase::QUERY_MODULE_STATE_DISCOVERY:
        if (this->module_state_discovery_index_ < MODULE_STATE_DISCOVERY_CASES) {
          this->send_next_module_state_discovery_query_();
        } else {
          this->finish_pending_query_();
          ESP_LOGI(TAG,
                   "Read-only module-state discovery complete: %u states tested "
                   "for primary=0x%02X secondary=0x%02X",
                   static_cast<unsigned>(MODULE_STATE_DISCOVERY_CASES),
                   this->module_state_primary_selector_,
                   this->module_state_secondary_selector_);
          this->phase_ = Phase::RESPONSE_DRAIN;
          this->next_action_at_ = now + RESPONSE_DRAIN_MS;
        }
        break;
'''
if old_selector_phase not in text:
    raise SystemExit("missing selector phase block")
text = text.replace(old_selector_phase, new_selector_phase, 1)

text = text.replace(
    '    ESP_LOGCONFIG(TAG, "  Exhaustive read-only selector discovery: %s", YESNO(this->selector_discovery_));\n',
    '    ESP_LOGCONFIG(TAG, "  Exhaustive read-only selector discovery: %s", YESNO(this->selector_discovery_));\n'
    '    ESP_LOGCONFIG(TAG, "  Read-only module-state discovery: %s", YESNO(this->module_state_discovery_));\n'
    '    ESP_LOGCONFIG(TAG, "  Module-state selector: primary=0x%02X secondary=0x%02X",\n'
    '                  this->module_state_primary_selector_, this->module_state_secondary_selector_);\n',
    1,
)
text = text.replace(
    '  enum class QueryCycle : uint8_t { FULL_DISCOVERY, OUTDOOR_OPERATING };\n',
    '  enum class QueryCycle : uint8_t { FULL_DISCOVERY, OUTDOOR_OPERATING };\n'
    '  enum class DiscoveryKind : uint8_t { SELECTOR, MODULE_STATE };\n',
    1,
)
text = text.replace(
    '    QUERY_SELECTOR_DISCOVERY,\n    RESPONSE_DRAIN,\n',
    '    QUERY_SELECTOR_DISCOVERY,\n    QUERY_MODULE_STATE_DISCOVERY,\n    RESPONSE_DRAIN,\n',
    1,
)
text = text.replace(
    '  static constexpr uint8_t SELECTOR_DISCOVERY_CASES = 64;\n',
    '  static constexpr uint8_t SELECTOR_DISCOVERY_CASES = 64;\n'
    '  static constexpr uint8_t MODULE_STATE_DISCOVERY_CASES = 8;\n',
    1,
)
text = text.replace(
    '    this->selector_discovery_index_ = 0;\n',
    '    this->selector_discovery_index_ = 0;\n'
    '    this->module_state_discovery_index_ = 0;\n'
    '    this->discovery_baselines_.clear();\n',
    1,
)

old_any_response = '''    if (this->pending_any_response_) {
      const uint32_t total_after = this->climate_->get_retained_payload_total_generation();
      if (total_after != this->pending_total_generation_) {
        const uint8_t command = this->climate_->get_last_retained_command();
        const auto *payload = this->climate_->get_retained_payload(command);
        ESP_LOGI(TAG,
                 "SELECTOR RX primary=0x%02X secondary=0x%02X state=0x%02X -> "
                 "cmd=0x%02X payload=%u bytes",
                 this->pending_primary_selector_, this->pending_secondary_selector_,
                 this->pending_module_state_, command,
                 payload == nullptr ? 0U : static_cast<unsigned>(payload->size()));
      } else {
        ESP_LOGW(TAG,
                 "SELECTOR RX timeout primary=0x%02X secondary=0x%02X state=0x%02X",
                 this->pending_primary_selector_, this->pending_secondary_selector_,
                 this->pending_module_state_);
      }
      this->pending_any_response_ = false;
      this->pending_query_active_ = false;
      return;
    }
'''
new_any_response = '''    if (this->pending_any_response_) {
      const char *kind = this->pending_discovery_kind_ == DiscoveryKind::MODULE_STATE
                             ? "STATE"
                             : "SELECTOR";
      const uint32_t total_after = this->climate_->get_retained_payload_total_generation();
      if (total_after != this->pending_total_generation_) {
        const uint8_t command = this->climate_->get_last_retained_command();
        const auto *payload = this->climate_->get_retained_payload(command);
        if (payload != nullptr) {
          const uint32_t hash = payload_fnv1a(*payload);
          const auto baseline = this->discovery_baselines_.find(command);
          uint32_t baseline_hash = hash;
          std::string differences = "baseline";
          if (baseline == this->discovery_baselines_.end()) {
            this->discovery_baselines_[command] = *payload;
          } else {
            baseline_hash = payload_fnv1a(baseline->second);
            differences = payload_changed_indices(baseline->second, *payload);
          }
          ESP_LOGI(TAG,
                   "%s RX primary=0x%02X secondary=0x%02X state=0x%02X -> "
                   "cmd=0x%02X payload=%u hash=0x%08X baseline=0x%08X diff=[%s]",
                   kind, this->pending_primary_selector_, this->pending_secondary_selector_,
                   this->pending_module_state_, command,
                   static_cast<unsigned>(payload->size()), static_cast<unsigned>(hash),
                   static_cast<unsigned>(baseline_hash), differences.c_str());
        } else {
          ESP_LOGW(TAG,
                   "%s RX primary=0x%02X secondary=0x%02X state=0x%02X -> "
                   "cmd=0x%02X retained payload missing",
                   kind, this->pending_primary_selector_, this->pending_secondary_selector_,
                   this->pending_module_state_, command);
        }
      } else {
        ESP_LOGW(TAG,
                 "%s RX timeout primary=0x%02X secondary=0x%02X state=0x%02X",
                 kind, this->pending_primary_selector_, this->pending_secondary_selector_,
                 this->pending_module_state_);
      }
      this->pending_any_response_ = false;
      this->pending_query_active_ = false;
      return;
    }
'''
if old_any_response not in text:
    raise SystemExit("missing pending response block")
text = text.replace(old_any_response, new_any_response, 1)

text = text.replace(
    '    this->pending_primary_selector_ = primary;\n',
    '    this->pending_discovery_kind_ = DiscoveryKind::SELECTOR;\n'
    '    this->pending_primary_selector_ = primary;\n',
    1,
)

insert_anchor = '''  template<size_t N>
  void send_query_and_advance_(const std::array<uint8_t, N> &frame, uint8_t expected_command,
'''
insert_code = '''  void begin_module_state_discovery_() {
    this->module_state_discovery_index_ = 0;
    this->discovery_baselines_.clear();
    this->phase_ = Phase::QUERY_MODULE_STATE_DISCOVERY;
    this->next_action_at_ = millis();
    ESP_LOGI(TAG,
             "Starting read-only module-state discovery for primary=0x%02X "
             "secondary=0x%02X",
             this->module_state_primary_selector_, this->module_state_secondary_selector_);
  }

  void send_next_module_state_discovery_query_() {
    this->finish_pending_query_();
    const uint8_t module_state = this->module_state_discovery_index_++;

    this->pending_discovery_kind_ = DiscoveryKind::MODULE_STATE;
    this->pending_primary_selector_ = this->module_state_primary_selector_;
    this->pending_secondary_selector_ = this->module_state_secondary_selector_;
    this->pending_module_state_ = module_state;
    this->pending_total_generation_ = this->climate_->get_retained_payload_total_generation();
    this->pending_any_response_ = true;
    this->pending_query_active_ = true;

    std::snprintf(this->selector_description_, sizeof(this->selector_description_),
                  "read-only module state primary=0x%02X secondary=0x%02X state=0x%02X",
                  this->pending_primary_selector_, this->pending_secondary_selector_,
                  module_state);
    const auto frame = build_rtl_report_query(this->pending_primary_selector_,
                                              this->pending_secondary_selector_,
                                              0x3B, module_state);
    this->send_(frame, this->selector_description_);
    this->phase_ = Phase::QUERY_MODULE_STATE_DISCOVERY;
    this->next_action_at_ = millis() + this->query_spacing_ms_();
  }

'''
if insert_anchor not in text:
    raise SystemExit("missing query helper insertion anchor")
text = text.replace(insert_anchor, insert_code + insert_anchor, 1)

text = text.replace(
    '  QueryCycle query_cycle_{QueryCycle::FULL_DISCOVERY};\n',
    '  QueryCycle query_cycle_{QueryCycle::FULL_DISCOVERY};\n'
    '  DiscoveryKind pending_discovery_kind_{DiscoveryKind::SELECTOR};\n',
    1,
)
text = text.replace(
    '  uint8_t selector_discovery_index_{0};\n',
    '  uint8_t selector_discovery_index_{0};\n'
    '  uint8_t module_state_discovery_index_{0};\n'
    '  uint8_t module_state_primary_selector_{0x04};\n'
    '  uint8_t module_state_secondary_selector_{0x00};\n',
    1,
)
text = text.replace(
    '  bool selector_discovery_{false};\n',
    '  bool selector_discovery_{false};\n'
    '  bool module_state_discovery_{false};\n',
    1,
)
text = text.replace(
    '  bool energy_discovery_attempted_{false};\n',
    '  bool energy_discovery_attempted_{false};\n'
    '  std::map<uint8_t, std::vector<uint8_t>> discovery_baselines_;\n',
    1,
)
path.write_text(text)

# Expand source-level wiring checks.
replace_once(
    "tests/test_selector_discovery_source.py",
    '        self.assertIn("default=False", config)\n',
    '        self.assertIn("default=False", config)\n'
    '        self.assertIn("MODULE_STATE_DISCOVERY_CASES = 8", header)\n'
    '        self.assertIn("build_rtl_report_query(this->pending_primary_selector_", header)\n'
    '        self.assertIn("payload_changed_indices", header)\n'
    '        self.assertIn(\'CONF_MODULE_STATE_DISCOVERY = "module_state_discovery"\', config)\n'
    '        self.assertIn("module_state_primary_selector", config)\n',
)

# Compile the standalone payload helper under the same strict C++ flags as the
# existing temperature stabilizer test.
Path("tests/test_payload_fingerprint.py").write_text(r'''import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


class PayloadFingerprintTest(unittest.TestCase):
    def test_cpp_behavior(self):
        root = Path(__file__).resolve().parents[1]
        source = textwrap.dedent(r"""
            #include <cassert>
            #include <cstdint>
            #include <vector>
            #include "components/gree_oem_boot_probe/payload_fingerprint.h"
            using esphome::gree_oem_probe::payload_changed_indices;
            using esphome::gree_oem_probe::payload_fnv1a;
            int main() {
              const std::vector<uint8_t> baseline{0x04, 0x00, 0x40, 0x00};
              const std::vector<uint8_t> same{0x04, 0x00, 0x40, 0x00};
              const std::vector<uint8_t> changed{0x04, 0x00, 0x41, 0x00};
              const std::vector<uint8_t> longer{0x04, 0x00, 0x40, 0x00, 0x01};
              assert(payload_fnv1a(baseline) == payload_fnv1a(same));
              assert(payload_fnv1a(baseline) != payload_fnv1a(changed));
              assert(payload_changed_indices(baseline, same) == "none");
              assert(payload_changed_indices(baseline, changed) == "2");
              assert(payload_changed_indices(baseline, longer) == "size:4->5");
              return 0;
            }
        """)
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "test.cpp"
            exe = Path(tmp) / "test"
            src.write_text(source)
            subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-I", str(root), str(src), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    unittest.main()
''')
