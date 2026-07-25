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


Path("components/sinclair_ac/temperature_stabilizer.h").write_text(r'''#pragma once

#include <cmath>
#include <cstdint>

namespace esphome {
namespace sinclair_ac {

class TemperatureStabilizer {
 public:
  void reset() {
    has_output_ = false;
    candidate_active_ = false;
    output_ = 0.0f;
    candidate_ = 0.0f;
    candidate_since_ = 0;
  }

  bool process(float sample, uint32_t now, bool enabled, uint32_t settle_time_ms,
               float immediate_delta_c, float &accepted) {
    if (!std::isfinite(sample)) return false;
    if (!has_output_) return accept_(sample, accepted);
    if (!enabled) {
      candidate_active_ = false;
      if (same_(sample, output_)) return false;
      return accept_(sample, accepted);
    }
    if (same_(sample, output_)) {
      candidate_active_ = false;
      return false;
    }
    if (immediate_delta_c <= 0.0f || std::fabs(sample - output_) >= immediate_delta_c)
      return accept_(sample, accepted);
    if (!candidate_active_ || !same_(sample, candidate_)) {
      candidate_active_ = true;
      candidate_ = sample;
      candidate_since_ = now;
      return false;
    }
    if (settle_time_ms == 0 || static_cast<uint32_t>(now - candidate_since_) >= settle_time_ms)
      return accept_(candidate_, accepted);
    return false;
  }

 private:
  static bool same_(float a, float b) { return std::fabs(a - b) < 0.01f; }
  bool accept_(float value, float &accepted) {
    has_output_ = true;
    candidate_active_ = false;
    output_ = value;
    accepted = value;
    return true;
  }

  bool has_output_{false};
  bool candidate_active_{false};
  float output_{0.0f};
  float candidate_{0.0f};
  uint32_t candidate_since_{0};
};

}  // namespace sinclair_ac
}  // namespace esphome
''')

h = "components/sinclair_ac/esppac.h"
replace_once(h, '#include "telemetry_discovery.h"\n', '#include "telemetry_discovery.h"\n#include "temperature_stabilizer.h"\n')
replace_once(h,
             'enum class ProtocolMode : uint8_t { RECEIVE_ONLY, POLL_ONLY, CONTROL };\nenum class FanProfile : uint8_t { AUTO, SINCLAIR_EXTENDED, GREE_4_SPEED };\n',
             'enum class ProtocolMode : uint8_t { RECEIVE_ONLY, POLL_ONLY, CONTROL };\nenum class FanProfile : uint8_t { AUTO, SINCLAIR_EXTENDED, GREE_4_SPEED };\nenum class TemperatureStabilizationMode : uint8_t { OFF, AUTO, ON };\n')
replace_once(h,
             '        void set_fan_profile(FanProfile profile) { this->fan_profile_ = profile; }\n',
             '''        void set_fan_profile(FanProfile profile) { this->fan_profile_ = profile; }
        void set_temperature_stabilization(TemperatureStabilizationMode mode,
                                           uint32_t settle_time_ms,
                                           float immediate_delta_c) {
            this->temperature_stabilization_mode_ = mode;
            this->temperature_stabilization_settle_time_ms_ = settle_time_ms;
            this->temperature_stabilization_immediate_delta_c_ = immediate_delta_c;
            this->current_temperature_stabilizer_.reset();
        }
        bool temperature_stabilization_active() const {
            return this->temperature_stabilization_mode_ == TemperatureStabilizationMode::ON ||
                   (this->temperature_stabilization_mode_ == TemperatureStabilizationMode::AUTO &&
                    this->temperature_stabilization_auto_enabled());
        }
        uint32_t temperature_stabilization_settle_time_ms() const { return this->temperature_stabilization_settle_time_ms_; }
        float temperature_stabilization_immediate_delta_c() const { return this->temperature_stabilization_immediate_delta_c_; }
''')
replace_once(h,
             '        bool plasma_state_{false}; bool sleep_state_{false}; bool xfan_state_{false}; bool save_state_{false};\n',
             '''        bool plasma_state_{false}; bool sleep_state_{false}; bool xfan_state_{false}; bool save_state_{false};
        bool has_plasma_state_{false}; bool has_sleep_state_{false}; bool has_xfan_state_{false}; bool has_save_state_{false};
''')
replace_once(h,
             '        FanProfile fan_profile_{FanProfile::AUTO};\n',
             '''        FanProfile fan_profile_{FanProfile::AUTO};
        TemperatureStabilizationMode temperature_stabilization_mode_{TemperatureStabilizationMode::AUTO};
        uint32_t temperature_stabilization_settle_time_ms_{8000};
        float temperature_stabilization_immediate_delta_c_{2.0f};
        TemperatureStabilizer current_temperature_stabilizer_;
''')
replace_once(h,
             '        bool can_control() const { return this->protocol_mode_ == ProtocolMode::CONTROL; }\n',
             '        bool can_control() const { return this->protocol_mode_ == ProtocolMode::CONTROL; }\n        virtual bool temperature_stabilization_auto_enabled() const { return false; }\n')
replace_once(h,
             '        void update_current_temperature(float temperature);\n',
             '        void update_current_temperature(float temperature);\n        bool update_current_temperature_from_report(float temperature);\n')

cpp = "components/sinclair_ac/esppac.cpp"
replace_once(cpp,
'''void SinclairAC::update_current_temperature(float temperature)
{
    if (temperature > TEMPERATURE_THRESHOLD) {
        ESP_LOGW(TAG, "Received out of range inside temperature: %f", temperature);
        return;
    }

    this->current_temperature = temperature;
}
''',
'''void SinclairAC::update_current_temperature(float temperature)
{
    if (!std::isfinite(temperature) || temperature > TEMPERATURE_THRESHOLD) {
        ESP_LOGW(TAG, "Received out of range inside temperature: %f", temperature);
        return;
    }
    this->current_temperature = temperature;
}

bool SinclairAC::update_current_temperature_from_report(float temperature)
{
    if (!std::isfinite(temperature) || temperature > TEMPERATURE_THRESHOLD) {
        ESP_LOGW(TAG, "Received out of range inside temperature: %f", temperature);
        return false;
    }
    float accepted = temperature;
    if (!this->current_temperature_stabilizer_.process(
            temperature, millis(), this->temperature_stabilization_active(),
            this->temperature_stabilization_settle_time_ms_,
            this->temperature_stabilization_immediate_delta_c_, accepted)) return false;
    if (std::isfinite(this->current_temperature) && std::fabs(this->current_temperature - accepted) < 0.01f) return false;
    this->current_temperature = accepted;
    return true;
}
''')

for name in ("plasma", "sleep", "xfan", "save"):
    old = f'''void SinclairAC::update_{name}(bool {name})
{{
    this->{name}_state_ = {name};

    if (this->{name}_switch_ != nullptr)
    {{
        this->{name}_switch_->publish_state(this->{name}_state_);
    }}
}}
'''
    new = f'''void SinclairAC::update_{name}(bool {name})
{{
    const bool changed = !this->has_{name}_state_ || this->{name}_state_ != {name};
    this->has_{name}_state_ = true;
    this->{name}_state_ = {name};
    if (changed && this->{name}_switch_ != nullptr) this->{name}_switch_->publish_state({name});
}}
'''
    replace_once(cpp, old, new)

cnt_h = "components/sinclair_ac/esppac_cnt.h"
replace_once(cnt_h,
'''        bool supplemental_query_may_start() const override {
            return this->request_lifecycle_.may_send() && !this->pending_control_.active &&
                   !this->active_control_.active && !this->control_send_queued_;
        }
''',
'''        bool supplemental_query_may_start() const override {
            return this->request_lifecycle_.may_send() && !this->pending_control_.active &&
                   !this->active_control_.active && !this->control_send_queued_;
        }
        bool temperature_stabilization_auto_enabled() const override { return this->uses_gree_fan_layout(); }
''')
replace_once(cnt_h, '        uint32_t last_candidate_telemetry_byte_44_publish_{0};\n', '')

cnt = "components/sinclair_ac/esppac_cnt.cpp"
replace_once(cnt,
'''    if (payload.size() > 44) {
        const bool refresh_due = millis() - this->last_candidate_telemetry_byte_44_publish_ >= 60000;
        const bool changed = !this->has_published_candidate_telemetry_byte_44_ ||
                             payload[44] != this->published_candidate_telemetry_byte_44_;
        if (changed || refresh_due) {
            if (this->candidate_telemetry_byte_44_raw_sensor_) {
                this->candidate_telemetry_byte_44_raw_sensor_->publish_state(payload[44]);
            }
            if (this->candidate_byte_44_temperature_hypothesis_sensor_) {
                this->candidate_byte_44_temperature_hypothesis_sensor_->publish_state(
                    decode_current_temperature_field(payload[44], this->uses_gree_fan_layout()));
            }
            this->published_candidate_telemetry_byte_44_ = payload[44];
            this->has_published_candidate_telemetry_byte_44_ = true;
            this->last_candidate_telemetry_byte_44_publish_ = millis();
        }
    }
''',
'''    if (payload.size() > 44) {
        const bool changed = !this->has_published_candidate_telemetry_byte_44_ ||
                             payload[44] != this->published_candidate_telemetry_byte_44_;
        if (changed) {
            if (this->candidate_telemetry_byte_44_raw_sensor_) this->candidate_telemetry_byte_44_raw_sensor_->publish_state(payload[44]);
            if (this->candidate_byte_44_temperature_hypothesis_sensor_) {
                this->candidate_byte_44_temperature_hypothesis_sensor_->publish_state(
                    decode_current_temperature_field(payload[44], this->uses_gree_fan_layout()));
            }
            this->published_candidate_telemetry_byte_44_ = payload[44];
            this->has_published_candidate_telemetry_byte_44_ = true;
        }
    }
''')
replace_once(cnt,
'''        const float newCurrentTemperature =
            decode_current_temperature_field(raw_current_temperature, this->uses_gree_fan_layout());
        if (this->current_temperature != newCurrentTemperature) hasChanged = true;
        this->update_current_temperature(newCurrentTemperature);
''',
'''        const float newCurrentTemperature =
            decode_current_temperature_field(raw_current_temperature, this->uses_gree_fan_layout());
        if (this->update_current_temperature_from_report(newCurrentTemperature)) hasChanged = true;
''')

climate = "components/sinclair_ac/climate.py"
replace_once(climate, 'CONF_SUPPLEMENTAL_QUERIES = "supplemental_queries"\n', 'CONF_SUPPLEMENTAL_QUERIES = "supplemental_queries"\nCONF_TEMPERATURE_STABILIZATION = "temperature_stabilization"\n')
replace_once(climate,
'''supplemental_queries_schema = cv.Schema({
    # Disabled by default. Templates are captured OEM requests, never invented.
    cv.Optional("enabled", default=False): cv.boolean,
    cv.Optional("interval", default="10s"): cv.positive_time_period_milliseconds,
    cv.Optional("max_attempts", default=1): cv.int_range(min=1, max=3),
    cv.Optional("queries", default=[]): cv.All(cv.ensure_list(supplemental_query_schema), cv.Length(max=8)),
})
''',
'''supplemental_queries_schema = cv.Schema({
    # Disabled by default. Templates are captured OEM requests, never invented.
    cv.Optional("enabled", default=False): cv.boolean,
    cv.Optional("interval", default="10s"): cv.positive_time_period_milliseconds,
    cv.Optional("max_attempts", default=1): cv.int_range(min=1, max=3),
    cv.Optional("queries", default=[]): cv.All(cv.ensure_list(supplemental_query_schema), cv.Length(max=8)),
})
temperature_stabilization_schema = cv.Schema({
    cv.Optional("mode", default="auto"): cv.one_of("off", "auto", "on", lower=True),
    cv.Optional("settle_time", default="8s"): cv.positive_time_period_milliseconds,
    cv.Optional("immediate_delta", default=2.0): cv.float_range(min=0.5, max=10.0),
})
''')
replace_once(climate,
             '        cv.Optional(CONF_SUPPLEMENTAL_QUERIES, default={}): supplemental_queries_schema,\n',
             '        cv.Optional(CONF_SUPPLEMENTAL_QUERIES, default={}): supplemental_queries_schema,\n        cv.Optional(CONF_TEMPERATURE_STABILIZATION, default={}): temperature_stabilization_schema,\n')
replace_once(climate,
             '    cg.add(var.set_fan_profile({"sinclair_extended": cg.RawExpression("sinclair_ac::FanProfile::SINCLAIR_EXTENDED"), "gree_4_speed": cg.RawExpression("sinclair_ac::FanProfile::GREE_4_SPEED"), "auto": cg.RawExpression("sinclair_ac::FanProfile::AUTO")}[config[CONF_FAN_PROFILE]]))\n',
'''    cg.add(var.set_fan_profile({"sinclair_extended": cg.RawExpression("sinclair_ac::FanProfile::SINCLAIR_EXTENDED"), "gree_4_speed": cg.RawExpression("sinclair_ac::FanProfile::GREE_4_SPEED"), "auto": cg.RawExpression("sinclair_ac::FanProfile::AUTO")}[config[CONF_FAN_PROFILE]]))
    stabilization = config[CONF_TEMPERATURE_STABILIZATION]
    cg.add(var.set_temperature_stabilization(
        {"off": cg.RawExpression("sinclair_ac::TemperatureStabilizationMode::OFF"),
         "auto": cg.RawExpression("sinclair_ac::TemperatureStabilizationMode::AUTO"),
         "on": cg.RawExpression("sinclair_ac::TemperatureStabilizationMode::ON")}[stabilization["mode"]],
        stabilization["settle_time"], stabilization["immediate_delta"]))
''')
