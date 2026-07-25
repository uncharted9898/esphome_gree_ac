from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    if new in text:
        return
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}")
    p.write_text(text.replace(old, new, 1))


report = "components/gree_oem_report_sensors/gree_oem_report_sensors.h"
replace_once(report,
'''  static void publish_text_sensor_(text_sensor::TextSensor *sensor, const std::string &value) {
    if (sensor != nullptr && (!sensor->has_state() || sensor->state != value)) {
      sensor->publish_state(value);
    }
  }
''',
'''  static void publish_text_sensor_(text_sensor::TextSensor *sensor, const std::string &value) {
    if (sensor != nullptr && (!sensor->has_state() || sensor->state != value)) {
      sensor->publish_state(value);
    }
  }

  void publish_stabilized_temperature_(sensor::Sensor *sensor,
                                       sinclair_ac::TemperatureStabilizer &stabilizer,
                                       float value) {
    if (sensor == nullptr) return;
    float accepted = value;
    if (stabilizer.process(value, millis(), this->climate_->temperature_stabilization_active(),
                           this->climate_->temperature_stabilization_settle_time_ms(),
                           this->climate_->temperature_stabilization_immediate_delta_c(), accepted)) {
      publish_sensor_(sensor, accepted);
    }
  }
''')
replace_once(report,
'''  void publish_status_report_() {
    const auto *payload = this->climate_->get_retained_payload(0x31);
    if (payload == nullptr || *payload == this->last_status_decoded_payload_) return;
    StatusReportFields fields;
    if (!decode_status_report(*payload, fields)) return;
    this->last_status_decoded_payload_ = *payload;

    publish_sensor_(this->status_humidity_sensor_field_raw_sensor_,
                    fields.humidity_sensor_field_raw);
    publish_sensor_(this->status_indoor_fan_port_raw_sensor_,
                    fields.indoor_fan_port_raw);
    publish_binary_sensor_(this->status_elc_erg_sensor_, fields.elc_erg_flag);
    publish_sensor_(this->status_elc_gear_raw_sensor_, fields.elc_gear_raw);
    publish_sensor_(this->status_indoor_temperature_sensor_,
                    fields.indoor_temperature_c);
    publish_sensor_(this->status_outdoor_ambient_temperature_sensor_,
                    fields.outdoor_ambient_temperature_c);
    publish_sensor_(this->status_elc_1kwh_raw_sensor_, fields.elc_1kwh_raw);
  }
''',
'''  void publish_status_report_() {
    const uint32_t generation = this->climate_->get_retained_payload_generation(0x31);
    if (generation == 0 || generation == this->last_status_generation_) return;
    const auto *payload = this->climate_->get_retained_payload(0x31);
    if (payload == nullptr) return;
    StatusReportFields fields;
    if (!decode_status_report(*payload, fields)) return;
    this->last_status_generation_ = generation;

    publish_sensor_(this->status_humidity_sensor_field_raw_sensor_,
                    fields.humidity_sensor_field_raw);
    publish_sensor_(this->status_indoor_fan_port_raw_sensor_,
                    fields.indoor_fan_port_raw);
    publish_binary_sensor_(this->status_elc_erg_sensor_, fields.elc_erg_flag);
    publish_sensor_(this->status_elc_gear_raw_sensor_, fields.elc_gear_raw);
    publish_stabilized_temperature_(this->status_indoor_temperature_sensor_,
                                    this->status_indoor_temperature_stabilizer_,
                                    fields.indoor_temperature_c);
    publish_stabilized_temperature_(this->status_outdoor_ambient_temperature_sensor_,
                                    this->status_outdoor_temperature_stabilizer_,
                                    fields.outdoor_ambient_temperature_c);
    publish_sensor_(this->status_elc_1kwh_raw_sensor_, fields.elc_1kwh_raw);
  }
''')
replace_once(report,
             '  sinclair_ac::SinclairAC *climate_{nullptr};\n',
             '''  sinclair_ac::SinclairAC *climate_{nullptr};
  sinclair_ac::TemperatureStabilizer status_indoor_temperature_stabilizer_;
  sinclair_ac::TemperatureStabilizer status_outdoor_temperature_stabilizer_;
  uint32_t last_status_generation_{0};
''')
replace_once(report, '  std::vector<uint8_t> last_status_decoded_payload_;\n', '')

probe = "components/gree_oem_boot_probe/gree_oem_boot_probe.h"
replace_once(probe,
             '  void set_quiesce_delay(uint32_t delay_ms) { this->quiesce_delay_ms_ = delay_ms; }\n',
             '  void set_quiesce_delay(uint32_t delay_ms) { this->quiesce_delay_ms_ = delay_ms; }\n  void set_scheduled_quiesce_delay(uint32_t delay_ms) { this->scheduled_quiesce_delay_ms_ = delay_ms; }\n')
replace_once(probe,
             '      ESP_LOGI(TAG, "Starting scheduled RTL8720CF outdoor operating query");\n',
             '      ESP_LOGD(TAG, "Starting scheduled RTL8720CF outdoor operating query");\n')
replace_once(probe,
             '    ESP_LOGCONFIG(TAG, "  Query quiesce delay: %u ms", this->quiesce_delay_ms_);\n',
             '    ESP_LOGCONFIG(TAG, "  Query quiesce delay: %u ms", this->quiesce_delay_ms_);\n    ESP_LOGCONFIG(TAG, "  Scheduled query quiesce delay: %u ms", this->scheduled_quiesce_delay_ms_);\n')
replace_once(probe,
'''    this->phase_ = Phase::QUERY_QUIESCE;
    this->next_action_at_ = millis() + (quiesce ? this->quiesce_delay_ms_ : 0);
''',
'''    this->phase_ = Phase::QUERY_QUIESCE;
    const uint32_t delay = cycle == QueryCycle::OUTDOOR_OPERATING
                               ? this->scheduled_quiesce_delay_ms_
                               : (quiesce ? this->quiesce_delay_ms_ : 0);
    this->next_action_at_ = millis() + delay;
''')
replace_once(probe,
'''    if (expected_after != this->pending_expected_generation_) {
      ESP_LOGI(TAG, "PROBE RX matched %s with command 0x%02X", this->pending_description_,
               this->pending_expected_command_);
    } else if (status_after != this->pending_status_generation_) {
      ESP_LOGI(TAG, "PROBE RX fallback after %s: command 0x31", this->pending_description_);
    } else {
''',
'''    if (expected_after != this->pending_expected_generation_) {
      if (this->query_cycle_ == QueryCycle::OUTDOOR_OPERATING) {
        ESP_LOGD(TAG, "PROBE RX matched %s with command 0x%02X", this->pending_description_,
                 this->pending_expected_command_);
      } else {
        ESP_LOGI(TAG, "PROBE RX matched %s with command 0x%02X", this->pending_description_,
                 this->pending_expected_command_);
      }
    } else if (status_after != this->pending_status_generation_) {
      if (this->query_cycle_ == QueryCycle::OUTDOOR_OPERATING) {
        ESP_LOGD(TAG, "PROBE RX fallback after %s: command 0x31", this->pending_description_);
      } else {
        ESP_LOGI(TAG, "PROBE RX fallback after %s: command 0x31", this->pending_description_);
      }
    } else {
''')
replace_once(probe,
'''  void finish_sequence_() {
    if (this->restore_control_) {
      this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::CONTROL);
      ESP_LOGI(TAG, "%s complete; normal climate control enabled",
               this->query_cycle_ == QueryCycle::OUTDOOR_OPERATING
                   ? "Outdoor operating query"
                   : "OEM telemetry discovery sequence");
    } else {
      ESP_LOGI(TAG, "OEM telemetry sequence complete; climate remains receive-only");
    }
''',
'''  void finish_sequence_() {
    if (this->restore_control_) {
      this->climate_->set_protocol_mode(sinclair_ac::ProtocolMode::CONTROL);
      if (this->query_cycle_ == QueryCycle::OUTDOOR_OPERATING) {
        ESP_LOGD(TAG, "Outdoor operating query complete; normal climate control enabled");
      } else {
        ESP_LOGI(TAG, "OEM telemetry discovery sequence complete; normal climate control enabled");
      }
    } else {
      ESP_LOGI(TAG, "OEM telemetry sequence complete; climate remains receive-only");
    }
''')
replace_once(probe,
'''    ++this->tx_sequence_;
    ESP_LOGI(TAG, "PROBE TX #%u cmd=0x%02X bytes=%u: %s", this->tx_sequence_, frame[3],
             static_cast<unsigned>(N), description);
''',
'''    ++this->tx_sequence_;
    if (this->query_cycle_ == QueryCycle::OUTDOOR_OPERATING) {
      ESP_LOGD(TAG, "PROBE TX #%u cmd=0x%02X bytes=%u: %s", this->tx_sequence_, frame[3],
               static_cast<unsigned>(N), description);
    } else {
      ESP_LOGI(TAG, "PROBE TX #%u cmd=0x%02X bytes=%u: %s", this->tx_sequence_, frame[3],
               static_cast<unsigned>(N), description);
    }
''')
replace_once(probe,
             '  uint32_t quiesce_delay_ms_{1800};\n',
             '  uint32_t quiesce_delay_ms_{1800};\n  uint32_t scheduled_quiesce_delay_ms_{150};\n')

probe_py = "components/gree_oem_boot_probe/__init__.py"
replace_once(probe_py,
             'CONF_QUIESCE_DELAY = "quiesce_delay"\n',
             'CONF_QUIESCE_DELAY = "quiesce_delay"\nCONF_SCHEDULED_QUIESCE_DELAY = "scheduled_quiesce_delay"\n')
replace_once(probe_py,
'''        cv.Optional(CONF_QUIESCE_DELAY, default="1800ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=1600), max=cv.TimePeriod(seconds=5)),
        ),
''',
'''        cv.Optional(CONF_QUIESCE_DELAY, default="1800ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=1600), max=cv.TimePeriod(seconds=5)),
        ),
        cv.Optional(CONF_SCHEDULED_QUIESCE_DELAY, default="150ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=50), max=cv.TimePeriod(seconds=1)),
        ),
''')
replace_once(probe_py,
             '    cg.add(var.set_quiesce_delay(config[CONF_QUIESCE_DELAY]))\n',
             '    cg.add(var.set_quiesce_delay(config[CONF_QUIESCE_DELAY]))\n    cg.add(var.set_scheduled_quiesce_delay(config[CONF_SCHEDULED_QUIESCE_DELAY]))\n')

Path("tests/test_temperature_stabilizer.py").write_text(r'''import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


class TemperatureStabilizerTest(unittest.TestCase):
    def test_cpp_behavior(self):
        root = Path(__file__).resolve().parents[1]
        source = textwrap.dedent(r"""
            #include <cassert>
            #include "components/sinclair_ac/temperature_stabilizer.h"
            using esphome::sinclair_ac::TemperatureStabilizer;
            int main() {
              TemperatureStabilizer s;
              float out = 0.0f;
              assert(s.process(22.0f, 100, true, 8000, 2.0f, out) && out == 22.0f);
              assert(!s.process(21.0f, 200, true, 8000, 2.0f, out));
              assert(!s.process(22.0f, 300, true, 8000, 2.0f, out));
              assert(!s.process(21.0f, 1000, true, 8000, 2.0f, out));
              assert(!s.process(21.0f, 8999, true, 8000, 2.0f, out));
              assert(s.process(21.0f, 9000, true, 8000, 2.0f, out) && out == 21.0f);
              assert(s.process(23.0f, 9100, true, 8000, 2.0f, out) && out == 23.0f);
              assert(s.process(22.0f, 9200, false, 8000, 2.0f, out) && out == 22.0f);
              s.reset();
              assert(s.process(20.0f, 0xFFFFFF00u, true, 500, 2.0f, out));
              assert(!s.process(21.0f, 0xFFFFFF10u, true, 500, 2.0f, out));
              assert(s.process(21.0f, 0x00000110u, true, 500, 2.0f, out));
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
