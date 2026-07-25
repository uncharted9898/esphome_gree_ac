from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"missing patch anchor in {path}: {old[:140]!r}")
    p.write_text(text.replace(old, new, 1))


# Configuration. The profiler is deliberately opt-in because it keeps the
# appliance UART in receive-only mode while collecting a multi-page time series.
replace_once(
    "components/gree_oem_boot_probe/__init__.py",
    'CONF_MODULE_STATE_SECONDARY_SELECTOR = "module_state_secondary_selector"\n',
    'CONF_MODULE_STATE_SECONDARY_SELECTOR = "module_state_secondary_selector"\n'
    'CONF_OPERATING_PROFILE = "operating_profile"\n'
    'CONF_OPERATING_PROFILE_CYCLES = "operating_profile_cycles"\n',
)
replace_once(
    "components/gree_oem_boot_probe/__init__.py",
    '        cv.Optional(CONF_MODULE_STATE_SECONDARY_SELECTOR, default=0x00): cv.int_range(min=0, max=7),\n',
    '        cv.Optional(CONF_MODULE_STATE_SECONDARY_SELECTOR, default=0x00): cv.int_range(min=0, max=7),\n'
    '        cv.Optional(CONF_OPERATING_PROFILE, default=False): cv.boolean,\n'
    '        cv.Optional(CONF_OPERATING_PROFILE_CYCLES, default=40): cv.int_range(min=1, max=120),\n',
)
replace_once(
    "components/gree_oem_boot_probe/__init__.py",
    '    cg.add(var.set_module_state_selectors(\n'
    '        config[CONF_MODULE_STATE_PRIMARY_SELECTOR],\n'
    '        config[CONF_MODULE_STATE_SECONDARY_SELECTOR],\n'
    '    ))\n',
    '    cg.add(var.set_module_state_selectors(\n'
    '        config[CONF_MODULE_STATE_PRIMARY_SELECTOR],\n'
    '        config[CONF_MODULE_STATE_SECONDARY_SELECTOR],\n'
    '    ))\n'
    '    cg.add(var.set_operating_profile(config[CONF_OPERATING_PROFILE]))\n'
    '    cg.add(var.set_operating_profile_cycles(config[CONF_OPERATING_PROFILE_CYCLES]))\n',
)

# Standalone evolution tracker. It records the complete baseline/last payload,
# per-byte min/max, cumulative transition XOR mask, and transition count.
Path("components/gree_oem_boot_probe/payload_evolution.h").write_text(r'''#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace esphome {
namespace gree_oem_probe {

inline std::string payload_hex(const std::vector<uint8_t> &payload) {
  static const char digits[] = "0123456789ABCDEF";
  std::string output;
  output.reserve(payload.size() * 3);
  for (size_t index = 0; index < payload.size(); ++index) {
    if (index != 0) output.push_back('.');
    output.push_back(digits[payload[index] >> 4]);
    output.push_back(digits[payload[index] & 0x0F]);
  }
  return output;
}

class PayloadEvolution {
 public:
  std::string observe(const std::vector<uint8_t> &payload,
                      size_t maximum_diff_entries = 24) {
    ++this->samples_;
    if (this->last_.empty()) {
      this->baseline_ = payload;
      this->last_ = payload;
      this->minimum_ = payload;
      this->maximum_ = payload;
      this->xor_mask_.assign(payload.size(), 0);
      this->change_count_.assign(payload.size(), 0);
      return "baseline";
    }

    const size_t old_size = this->last_.size();
    const size_t common_size = std::min(old_size, payload.size());
    std::string differences;
    size_t emitted = 0;

    if (payload.size() > this->minimum_.size()) {
      this->minimum_.resize(payload.size(), 0xFF);
      this->maximum_.resize(payload.size(), 0x00);
      this->xor_mask_.resize(payload.size(), 0x00);
      this->change_count_.resize(payload.size(), 0);
    }

    for (size_t index = 0; index < payload.size(); ++index) {
      this->minimum_[index] = std::min(this->minimum_[index], payload[index]);
      this->maximum_[index] = std::max(this->maximum_[index], payload[index]);
    }

    for (size_t index = 0; index < common_size; ++index) {
      if (this->last_[index] == payload[index]) continue;
      this->xor_mask_[index] |= static_cast<uint8_t>(this->last_[index] ^ payload[index]);
      ++this->change_count_[index];
      if (emitted < maximum_diff_entries) {
        char token[32];
        std::snprintf(token, sizeof(token), "%u:%02X>%02X",
                      static_cast<unsigned>(index), this->last_[index], payload[index]);
        if (!differences.empty()) differences.push_back(',');
        differences += token;
        ++emitted;
      }
    }

    if (old_size != payload.size()) {
      char token[48];
      std::snprintf(token, sizeof(token), "size:%u>%u",
                    static_cast<unsigned>(old_size),
                    static_cast<unsigned>(payload.size()));
      if (!differences.empty()) differences.push_back(',');
      differences += token;
    }
    if (emitted >= maximum_diff_entries) differences += ",...";

    this->last_ = payload;
    return differences.empty() ? "none" : differences;
  }

  size_t samples() const { return this->samples_; }
  size_t size() const { return this->minimum_.size(); }
  const std::vector<uint8_t> &baseline() const { return this->baseline_; }
  const std::vector<uint8_t> &last() const { return this->last_; }
  uint8_t minimum(size_t index) const { return this->minimum_.at(index); }
  uint8_t maximum(size_t index) const { return this->maximum_.at(index); }
  uint8_t xor_mask(size_t index) const { return this->xor_mask_.at(index); }
  uint32_t change_count(size_t index) const { return this->change_count_.at(index); }

 private:
  size_t samples_{0};
  std::vector<uint8_t> baseline_;
  std::vector<uint8_t> last_;
  std::vector<uint8_t> minimum_;
  std::vector<uint8_t> maximum_;
  std::vector<uint8_t> xor_mask_;
  std::vector<uint32_t> change_count_;
};

}  // namespace gree_oem_probe
}  // namespace esphome
''')

path = Path("components/gree_oem_boot_probe/gree_oem_boot_probe.h")
text = path.read_text()
text = text.replace(
    '#include "payload_fingerprint.h"\n',
    '#include "payload_evolution.h"\n#include "payload_fingerprint.h"\n',
    1,
)
text = text.replace(
    '  void set_module_state_discovery(bool enabled) { this->module_state_discovery_ = enabled; }\n',
    '  void set_module_state_discovery(bool enabled) { this->module_state_discovery_ = enabled; }\n'
    '  void set_operating_profile(bool enabled) { this->operating_profile_ = enabled; }\n'
    '  void set_operating_profile_cycles(uint16_t cycles) { this->operating_profile_cycles_ = cycles; }\n',
    1,
)

old_selector_tail = '''          if (this->module_state_discovery_) {
            this->begin_module_state_discovery_();
          } else {
            this->phase_ = Phase::RESPONSE_DRAIN;
            this->next_action_at_ = now + RESPONSE_DRAIN_MS;
          }
'''
new_selector_tail = '''          if (this->module_state_discovery_) {
            this->begin_module_state_discovery_();
          } else if (this->operating_profile_) {
            this->begin_operating_profile_();
          } else {
            this->phase_ = Phase::RESPONSE_DRAIN;
            this->next_action_at_ = now + RESPONSE_DRAIN_MS;
          }
'''
if old_selector_tail not in text:
    raise SystemExit("missing selector completion block")
text = text.replace(old_selector_tail, new_selector_tail, 1)

old_state_tail = '''          this->phase_ = Phase::RESPONSE_DRAIN;
          this->next_action_at_ = now + RESPONSE_DRAIN_MS;
        }
        break;
      case Phase::RESPONSE_DRAIN:
'''
new_state_tail = '''          if (this->operating_profile_) {
            this->begin_operating_profile_();
          } else {
            this->phase_ = Phase::RESPONSE_DRAIN;
            this->next_action_at_ = now + RESPONSE_DRAIN_MS;
          }
        }
        break;
      case Phase::QUERY_PROFILE_STATUS:
        this->send_profile_query_and_advance_(QUERY_EXTENDED_STATUS, 0x31,
                                              "profile extended status 0x31",
                                              Phase::QUERY_PROFILE_COMBINED);
        break;
      case Phase::QUERY_PROFILE_COMBINED:
        this->send_profile_query_and_advance_(QUERY_REPORT_COMBINED, 0x33,
                                              "profile combined report 0x33",
                                              Phase::QUERY_PROFILE_INDOOR);
        break;
      case Phase::QUERY_PROFILE_INDOOR:
        this->send_profile_query_and_advance_(QUERY_REPORT_INDOOR, 0x34,
                                              "profile indoor report 0x34",
                                              Phase::QUERY_PROFILE_OUTDOOR);
        break;
      case Phase::QUERY_PROFILE_OUTDOOR:
        this->send_profile_query_and_advance_(QUERY_REPORT_OUTDOOR, 0x35,
                                              "profile outdoor report 0x35",
                                              Phase::QUERY_PROFILE_CYCLE_END);
        break;
      case Phase::QUERY_PROFILE_CYCLE_END:
        this->finish_pending_query_();
        ++this->operating_profile_cycle_index_;
        if (this->operating_profile_cycle_index_ >= this->operating_profile_cycles_) {
          this->log_operating_profile_summary_();
          this->phase_ = Phase::RESPONSE_DRAIN;
          this->next_action_at_ = now + RESPONSE_DRAIN_MS;
        } else {
          this->phase_ = Phase::QUERY_PROFILE_STATUS;
          this->next_action_at_ = now;
        }
        break;
      case Phase::RESPONSE_DRAIN:
'''
if old_state_tail not in text:
    raise SystemExit("missing module-state tail anchor")
text = text.replace(old_state_tail, new_state_tail, 1)

text = text.replace(
    '    ESP_LOGCONFIG(TAG, "  Read-only module-state discovery: %s", YESNO(this->module_state_discovery_));\n',
    '    ESP_LOGCONFIG(TAG, "  Read-only module-state discovery: %s", YESNO(this->module_state_discovery_));\n'
    '    ESP_LOGCONFIG(TAG, "  Full operating payload profile: %s", YESNO(this->operating_profile_));\n'
    '    ESP_LOGCONFIG(TAG, "  Operating profile cycles: %u", this->operating_profile_cycles_);\n',
    1,
)
text = text.replace(
    '    QUERY_MODULE_STATE_DISCOVERY,\n    RESPONSE_DRAIN,\n',
    '    QUERY_MODULE_STATE_DISCOVERY,\n'
    '    QUERY_PROFILE_STATUS,\n'
    '    QUERY_PROFILE_COMBINED,\n'
    '    QUERY_PROFILE_INDOOR,\n'
    '    QUERY_PROFILE_OUTDOOR,\n'
    '    QUERY_PROFILE_CYCLE_END,\n'
    '    RESPONSE_DRAIN,\n',
    1,
)
text = text.replace(
    '    this->discovery_baselines_.clear();\n',
    '    this->discovery_baselines_.clear();\n'
    '    this->operating_profile_cycle_index_ = 0;\n'
    '    this->operating_profiles_.clear();\n',
    1,
)

# Handle expected-command profile replies before the selector/state path.
profile_finish_anchor = '''    if (this->pending_any_response_) {
'''
profile_finish_code = '''    if (this->pending_profile_response_) {
      const uint32_t generation_after =
          this->climate_->get_retained_payload_generation(this->pending_expected_command_);
      if (generation_after != this->pending_expected_generation_) {
        const auto *payload = this->climate_->get_retained_payload(this->pending_expected_command_);
        if (payload != nullptr) {
          auto &evolution = this->operating_profiles_[this->pending_expected_command_];
          const std::string differences = evolution.observe(*payload);
          const uint32_t hash = payload_fnv1a(*payload);
          if (differences == "none") {
            ESP_LOGI(TAG,
                     "PROFILE RX cycle=%u cmd=0x%02X payload=%u hash=0x%08X diff=[none]",
                     this->operating_profile_cycle_index_, this->pending_expected_command_,
                     static_cast<unsigned>(payload->size()), static_cast<unsigned>(hash));
          } else {
            const std::string raw = payload_hex(*payload);
            ESP_LOGI(TAG,
                     "PROFILE RX cycle=%u cmd=0x%02X payload=%u hash=0x%08X "
                     "diff=[%s] raw=%s",
                     this->operating_profile_cycle_index_, this->pending_expected_command_,
                     static_cast<unsigned>(payload->size()), static_cast<unsigned>(hash),
                     differences.c_str(), raw.c_str());
          }
        }
      } else {
        ESP_LOGW(TAG, "PROFILE RX timeout cycle=%u cmd=0x%02X",
                 this->operating_profile_cycle_index_, this->pending_expected_command_);
      }
      this->pending_profile_response_ = false;
      this->pending_query_active_ = false;
      return;
    }

'''
if profile_finish_anchor not in text:
    raise SystemExit("missing finish pending anchor")
text = text.replace(profile_finish_anchor, profile_finish_code + profile_finish_anchor, 1)

insert_anchor = '''  template<size_t N>
  void send_query_and_advance_(const std::array<uint8_t, N> &frame, uint8_t expected_command,
'''
profile_helpers = '''  void begin_operating_profile_() {
    this->operating_profile_cycle_index_ = 0;
    this->operating_profiles_.clear();
    this->phase_ = Phase::QUERY_PROFILE_STATUS;
    this->next_action_at_ = millis();
    ESP_LOGI(TAG,
             "Starting full Livo payload evolution profile: %u cycles, "
             "pages 0x31/0x33/0x34/0x35",
             this->operating_profile_cycles_);
  }

  template<size_t N>
  void send_profile_query_and_advance_(const std::array<uint8_t, N> &frame,
                                       uint8_t expected_command,
                                       const char *description, Phase next) {
    this->finish_pending_query_();
    this->pending_expected_command_ = expected_command;
    this->pending_expected_generation_ =
        this->climate_->get_retained_payload_generation(expected_command);
    this->pending_description_ = description;
    this->pending_profile_response_ = true;
    this->pending_query_active_ = true;
    this->send_(frame, description);
    this->phase_ = next;
    this->next_action_at_ = millis() + this->query_spacing_ms_();
  }

  void log_operating_profile_summary_() const {
    ESP_LOGI(TAG, "PROFILE COMPLETE cycles=%u commands=%u",
             this->operating_profile_cycle_index_,
             static_cast<unsigned>(this->operating_profiles_.size()));
    for (const auto &entry : this->operating_profiles_) {
      const uint8_t command = entry.first;
      const PayloadEvolution &evolution = entry.second;
      ESP_LOGI(TAG, "PROFILE BASELINE cmd=0x%02X samples=%u raw=%s", command,
               static_cast<unsigned>(evolution.samples()),
               payload_hex(evolution.baseline()).c_str());
      ESP_LOGI(TAG, "PROFILE LAST cmd=0x%02X raw=%s", command,
               payload_hex(evolution.last()).c_str());
      for (size_t start = 0; start < evolution.size(); start += 6) {
        std::string line;
        const size_t end = std::min(start + 6, evolution.size());
        for (size_t index = start; index < end; ++index) {
          char token[72];
          std::snprintf(token, sizeof(token),
                        "%u=%02X..%02X/x%02X/c%u",
                        static_cast<unsigned>(index), evolution.minimum(index),
                        evolution.maximum(index), evolution.xor_mask(index),
                        static_cast<unsigned>(evolution.change_count(index)));
          if (!line.empty()) line.push_back(' ');
          line += token;
        }
        ESP_LOGI(TAG, "PROFILE MAP cmd=0x%02X bytes=%u-%u %s", command,
                 static_cast<unsigned>(start), static_cast<unsigned>(end - 1),
                 line.c_str());
      }
    }
  }

'''
if insert_anchor not in text:
    raise SystemExit("missing helper insertion anchor")
text = text.replace(insert_anchor, profile_helpers + insert_anchor, 1)

text = text.replace(
    '  uint8_t pending_module_state_{0};\n',
    '  uint8_t pending_module_state_{0};\n'
    '  uint16_t operating_profile_cycles_{40};\n'
    '  uint16_t operating_profile_cycle_index_{0};\n',
    1,
)
text = text.replace(
    '  bool module_state_discovery_{false};\n',
    '  bool module_state_discovery_{false};\n'
    '  bool operating_profile_{false};\n',
    1,
)
text = text.replace(
    '  bool pending_any_response_{false};\n',
    '  bool pending_any_response_{false};\n'
    '  bool pending_profile_response_{false};\n',
    1,
)
text = text.replace(
    '  std::map<uint8_t, std::vector<uint8_t>> discovery_baselines_;\n',
    '  std::map<uint8_t, std::vector<uint8_t>> discovery_baselines_;\n'
    '  std::map<uint8_t, PayloadEvolution> operating_profiles_;\n',
    1,
)
path.write_text(text)

# Expand source-level wiring checks.
replace_once(
    "tests/test_selector_discovery_source.py",
    '        self.assertIn("module_state_primary_selector", config)\n',
    '        self.assertIn("module_state_primary_selector", config)\n'
    '        self.assertIn("QUERY_PROFILE_STATUS", header)\n'
    '        self.assertIn("PROFILE MAP cmd=0x%02X", header)\n'
    '        self.assertIn(\'CONF_OPERATING_PROFILE = "operating_profile"\', config)\n'
    '        self.assertIn("operating_profile_cycles", config)\n',
)

Path("tests/test_payload_evolution.py").write_text(r'''import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


class PayloadEvolutionTest(unittest.TestCase):
    def test_cpp_behavior(self):
        root = Path(__file__).resolve().parents[1]
        source = textwrap.dedent(r"""
            #include <cassert>
            #include <cstdint>
            #include <vector>
            #include "components/gree_oem_boot_probe/payload_evolution.h"
            using esphome::gree_oem_probe::PayloadEvolution;
            using esphome::gree_oem_probe::payload_hex;
            int main() {
              PayloadEvolution profile;
              const std::vector<uint8_t> a{0x04, 0x20, 0x40};
              const std::vector<uint8_t> b{0x04, 0x28, 0x3F};
              assert(profile.observe(a) == "baseline");
              assert(profile.observe(a) == "none");
              assert(profile.observe(b) == "1:20>28,2:40>3F");
              assert(profile.samples() == 3);
              assert(profile.minimum(1) == 0x20);
              assert(profile.maximum(1) == 0x28);
              assert(profile.xor_mask(1) == 0x08);
              assert(profile.change_count(1) == 1);
              assert(payload_hex(b) == "04.28.3F");
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
