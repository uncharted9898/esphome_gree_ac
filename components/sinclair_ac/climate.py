#based on: https://github.com/DomiStyle/esphome-panasonic-ac

from esphome.const import (
    CONF_ID,
)
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, climate, sensor, select, switch, binary_sensor, text_sensor

AUTO_LOAD = ["switch", "sensor", "select", "binary_sensor", "text_sensor"]
DEPENDENCIES = ["uart"]

sinclair_ac_ns = cg.esphome_ns.namespace("sinclair_ac")
SinclairAC = sinclair_ac_ns.class_(
    "SinclairAC", cg.Component, uart.UARTDevice, climate.Climate
)
sinclair_ac_cnt_ns = sinclair_ac_ns.namespace("CNT")
SinclairACCNT = sinclair_ac_cnt_ns.class_("SinclairACCNT", SinclairAC)

SinclairACSwitch = sinclair_ac_ns.class_(
    "SinclairACSwitch", switch.Switch, cg.Component
)
SinclairACSelect = sinclair_ac_ns.class_(
    "SinclairACSelect", select.Select, cg.Component
)


CONF_HORIZONTAL_SWING_SELECT    = "horizontal_swing_select"
CONF_VERTICAL_SWING_SELECT      = "vertical_swing_select"
CONF_DISPLAY_SELECT             = "display_select"
CONF_DISPLAY_UNIT_SELECT        = "display_unit_select"

CONF_PLASMA_SWITCH              = "plasma_switch"
CONF_SLEEP_SWITCH               = "sleep_switch"
CONF_XFAN_SWITCH                = "xfan_switch"
CONF_SAVE_SWITCH                = "save_switch"

CONF_CURRENT_TEMPERATURE_SENSOR = "current_temperature_sensor"
CONF_TRANSMIT_ENABLED = "transmit_enabled"
CONF_PROTOCOL_MODE = "protocol_mode"
CONF_DEBUG = "debug"
CONF_DIAGNOSTICS = "diagnostics"
CONF_FAN_PROFILE = "fan_profile"
CONF_TELEMETRY_DISCOVERY = "telemetry_discovery"
CONF_SUPPLEMENTAL_QUERIES = "supplemental_queries"
CONF_TEMPERATURE_STABILIZATION = "temperature_stabilization"

diagnostic_sensor_schema = sensor.sensor_schema(
    sensor.Sensor, accuracy_decimals=0, state_class="total_increasing"
)
measurement_diagnostic_sensor_schema = sensor.sensor_schema(
    sensor.Sensor, accuracy_decimals=0
)
diagnostics_schema = cv.Schema({
    cv.Optional("valid_rx_packets"): diagnostic_sensor_schema,
    cv.Optional("valid_tx_packets"): diagnostic_sensor_schema,
    cv.Optional("unknown_packets"): diagnostic_sensor_schema,
    cv.Optional("checksum_failures"): diagnostic_sensor_schema,
    cv.Optional("invalid_length_packets"): diagnostic_sensor_schema,
    cv.Optional("parser_resynchronizations"): diagnostic_sensor_schema,
    cv.Optional("too_short_frames"): diagnostic_sensor_schema,
    cv.Optional("frame_timeouts"): diagnostic_sensor_schema,
    # Unresolved raw candidate; intentionally unitless and diagnostic-only.
    cv.Optional("candidate_telemetry_byte_44_raw"): sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0),
    cv.Optional("candidate_byte_44_temperature_hypothesis"): sensor.sensor_schema(sensor.Sensor, accuracy_decimals=1),
    cv.Optional("polls_sent"): diagnostic_sensor_schema,
    cv.Optional("poll_responses"): diagnostic_sensor_schema,
    cv.Optional("poll_response_timeouts"): diagnostic_sensor_schema,
    # This value is reset after a successful poll, so it is not a total.
    cv.Optional("consecutive_poll_timeouts"): measurement_diagnostic_sensor_schema,
    cv.Optional("last_poll_response_ms"): measurement_diagnostic_sensor_schema,
    cv.Optional("min_poll_response_ms"): measurement_diagnostic_sensor_schema,
    cv.Optional("max_poll_response_ms"): measurement_diagnostic_sensor_schema,
    cv.Optional("average_poll_response_ms"): measurement_diagnostic_sensor_schema,
    cv.Optional("command_attempts"): diagnostic_sensor_schema,
    cv.Optional("command_response_timeouts"): diagnostic_sensor_schema,
    cv.Optional("command_mismatches"): diagnostic_sensor_schema,
    cv.Optional("last_packet_length"): sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0),
    cv.Optional("last_packet_type"): sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0),
    cv.Optional("communication"): binary_sensor.binary_sensor_schema(binary_sensor.BinarySensor),
    cv.Optional("receive_only"): binary_sensor.binary_sensor_schema(binary_sensor.BinarySensor),
    cv.Optional("poll_only"): binary_sensor.binary_sensor_schema(binary_sensor.BinarySensor),
    cv.Optional("protocol_mode"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("protocol_state"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("last_packet"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("last_unknown_packet"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("fan_speed_field_1_raw"): sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0),
    cv.Optional("fan_speed_field_1_low_3_bits"): sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0),
    cv.Optional("fan_speed_field_2_raw"): sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0),
    cv.Optional("fan_quiet_raw"): sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0),
    cv.Optional("fan_turbo_raw"): sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0),
    cv.Optional("fan_decode_status"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("fan_decode_profile"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("last_0x31_payload"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("last_0x33_payload"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("last_0x44_payload"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("last_0x40_payload"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("last_unknown_payload"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("last_command_result"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("last_command_failure_reason"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("discovery_summary"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("capture_export_csv"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
})
telemetry_discovery_schema = cv.Schema({
    cv.Optional("enabled", default=False): cv.boolean,
    cv.Optional("expose_raw_payload", default=True): cv.boolean,
    cv.Optional("expose_raw_bytes", default=False): cv.boolean,
    cv.Optional("log_changes_only", default=True): cv.boolean,
    cv.Optional("history_depth", default=16): cv.int_range(min=1, max=64),
})
supplemental_query_schema = cv.Schema({
    cv.Required("name"): cv.string,
    cv.Required("request_command"): cv.int_range(min=0, max=255),
    cv.Required("expected_response_command"): cv.int_range(min=0, max=255),
    cv.Required("raw_payload"): cv.All(cv.ensure_list(cv.int_range(min=0, max=255)), cv.Length(min=1, max=195)),
})
supplemental_queries_schema = cv.Schema({
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

debug_schema = cv.Schema({
    cv.Optional("log_rx", default=False): cv.boolean,
    cv.Optional("log_tx", default=False): cv.boolean,
    cv.Optional("log_unknown_packets", default=True): cv.boolean,
    cv.Optional("log_packet_differences", default=False): cv.boolean,
    cv.Optional("maximum_hex_length", default=128): cv.int_range(min=16, max=512),
})

HORIZONTAL_SWING_OPTIONS = [
    "0 - OFF",
    "1 - Swing - Full",
    "2 - Constant - Left",
    "3 - Constant - Mid-Left",
    "4 - Constant - Middle",
    "5 - Constant - Mid-Right",
    "6 - Constant - Right",
]


VERTICAL_SWING_OPTIONS = [
    "00 - OFF",
    "01 - Swing - Full",
    "02 - Swing - Down",
    "03 - Swing - Mid-Down",
    "04 - Swing - Middle",
    "05 - Swing - Mid-Up",
    "06 - Swing - Up",
    "07 - Constant - Down",
    "08 - Constant - Mid-Down",
    "09 - Constant - Middle",
    "10 - Constant - Mid-Up",
    "11 - Constant - Up",
]

DISPLAY_OPTIONS = [
    "0 - OFF",
    "1 - Auto",
    "2 - Set temperature",
    "3 - Actual temperature",
    "4 - Outside temperature",
]

DISPLAY_UNIT_OPTIONS = [
    "C",
    "F",
]

switch_schema = switch.switch_schema(switch.Switch).extend(cv.COMPONENT_SCHEMA).extend(
    {cv.GenerateID(): cv.declare_id(SinclairACSwitch)}
)
select_schema = select.select_schema(select.Select).extend(
    {cv.GenerateID(CONF_ID): cv.declare_id(SinclairACSelect)}
)

SCHEMA = climate.climate_schema(climate.Climate).extend(
    {
        cv.Optional(CONF_HORIZONTAL_SWING_SELECT): select_schema,
        cv.Optional(CONF_VERTICAL_SWING_SELECT): select_schema,
        cv.Optional(CONF_DISPLAY_SELECT): select_schema,
        cv.Optional(CONF_DISPLAY_UNIT_SELECT): select_schema,
        cv.Optional(CONF_PLASMA_SWITCH): switch_schema,
        cv.Optional(CONF_SLEEP_SWITCH): switch_schema,
        cv.Optional(CONF_XFAN_SWITCH): switch_schema,
        cv.Optional(CONF_SAVE_SWITCH): switch_schema,
        cv.Optional(CONF_TRANSMIT_ENABLED): cv.boolean,
        cv.Optional(CONF_PROTOCOL_MODE): cv.one_of("receive_only", "poll_only", "control", lower=True),
        cv.Optional(CONF_FAN_PROFILE, default="auto"): cv.one_of("sinclair_extended", "gree_4_speed", "auto", lower=True),
        cv.Optional(CONF_TELEMETRY_DISCOVERY, default={}): telemetry_discovery_schema,
        cv.Optional(CONF_SUPPLEMENTAL_QUERIES, default={}): supplemental_queries_schema,
        cv.Optional(CONF_TEMPERATURE_STABILIZATION, default={}): temperature_stabilization_schema,
        cv.Optional(CONF_DEBUG, default={}): debug_schema,
        cv.Optional(CONF_DIAGNOSTICS): diagnostics_schema,
    }
).extend(uart.UART_DEVICE_SCHEMA)

def _validate_mode(config):
    if CONF_TRANSMIT_ENABLED in config and CONF_PROTOCOL_MODE in config:
        raise cv.Invalid("transmit_enabled and protocol_mode cannot be used together")
    if CONF_TRANSMIT_ENABLED in config:
        import logging
        logging.getLogger(__name__).warning("transmit_enabled is deprecated; use protocol_mode instead")
        config[CONF_PROTOCOL_MODE] = "control" if config[CONF_TRANSMIT_ENABLED] else "receive_only"
    if CONF_PROTOCOL_MODE not in config:
        config[CONF_PROTOCOL_MODE] = "control"
    return config

CONFIG_SCHEMA = cv.All(
    SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SinclairACCNT),
            cv.Optional(CONF_CURRENT_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
        }
    ),
    _validate_mode,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate.register_climate(var, config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_protocol_mode({"receive_only": cg.RawExpression("sinclair_ac::ProtocolMode::RECEIVE_ONLY"), "poll_only": cg.RawExpression("sinclair_ac::ProtocolMode::POLL_ONLY"), "control": cg.RawExpression("sinclair_ac::ProtocolMode::CONTROL")}[config[CONF_PROTOCOL_MODE]]))
    debug = config[CONF_DEBUG]
    cg.add(var.set_debug(debug["log_rx"], debug["log_tx"], debug["log_unknown_packets"], debug["log_packet_differences"], debug["maximum_hex_length"]))
    cg.add(var.set_fan_profile({"sinclair_extended": cg.RawExpression("sinclair_ac::FanProfile::SINCLAIR_EXTENDED"), "gree_4_speed": cg.RawExpression("sinclair_ac::FanProfile::GREE_4_SPEED"), "auto": cg.RawExpression("sinclair_ac::FanProfile::AUTO")}[config[CONF_FAN_PROFILE]]))
    stabilization = config[CONF_TEMPERATURE_STABILIZATION]
    cg.add(var.set_temperature_stabilization(
        {"off": cg.RawExpression("sinclair_ac::TemperatureStabilizationMode::OFF"),
         "auto": cg.RawExpression("sinclair_ac::TemperatureStabilizationMode::AUTO"),
         "on": cg.RawExpression("sinclair_ac::TemperatureStabilizationMode::ON")}[stabilization["mode"]],
        stabilization["settle_time"], stabilization["immediate_delta"]))
    discovery = config[CONF_TELEMETRY_DISCOVERY]
    cg.add(var.set_telemetry_discovery(discovery["enabled"], discovery["expose_raw_payload"], discovery["expose_raw_bytes"], discovery["log_changes_only"], discovery["history_depth"]))
    queries = config[CONF_SUPPLEMENTAL_QUERIES]
    cg.add(var.set_supplemental_queries(queries["enabled"], queries["max_attempts"]))

    if CONF_DIAGNOSTICS in config:
        for key, method in {
            "valid_rx_packets": "set_valid_rx_packets_sensor", "valid_tx_packets": "set_valid_tx_packets_sensor",
            "unknown_packets": "set_unknown_packets_sensor", "checksum_failures": "set_checksum_failures_sensor",
            "invalid_length_packets": "set_invalid_length_sensor", "too_short_frames": "set_too_short_sensor", "frame_timeouts": "set_frame_timeout_sensor", "candidate_telemetry_byte_44_raw": "set_candidate_telemetry_byte_44_raw_sensor", "candidate_byte_44_temperature_hypothesis": "set_candidate_byte_44_temperature_hypothesis_sensor", "parser_resynchronizations": "set_parser_resync_sensor",
            "polls_sent": "set_polls_sent_sensor", "poll_responses": "set_poll_responses_sensor", "poll_response_timeouts": "set_poll_response_timeouts_sensor", "consecutive_poll_timeouts": "set_consecutive_poll_timeouts_sensor", "last_poll_response_ms": "set_last_poll_response_ms_sensor", "min_poll_response_ms": "set_min_poll_response_ms_sensor", "max_poll_response_ms": "set_max_poll_response_ms_sensor", "average_poll_response_ms": "set_average_poll_response_ms_sensor", "command_attempts": "set_command_attempts_sensor", "command_response_timeouts": "set_command_response_timeouts_sensor", "command_mismatches": "set_command_mismatches_sensor",
            "last_packet_length": "set_last_packet_length_sensor", "last_packet_type": "set_last_packet_type_sensor",
            "fan_speed_field_1_raw": "set_fan_speed_field_1_raw_sensor",
            "fan_speed_field_1_low_3_bits": "set_fan_speed_field_1_low_3_bits_sensor",
            "fan_speed_field_2_raw": "set_fan_speed_field_2_raw_sensor",
            "fan_quiet_raw": "set_fan_quiet_raw_sensor", "fan_turbo_raw": "set_fan_turbo_raw_sensor",
        }.items():
            if key in config[CONF_DIAGNOSTICS]:
                entity = await sensor.new_sensor(config[CONF_DIAGNOSTICS][key])
                cg.add(getattr(var, method)(entity))
        for key, method in {"communication": "set_communication_sensor", "receive_only": "set_receive_only_sensor", "poll_only": "set_poll_only_sensor"}.items():
            if key in config[CONF_DIAGNOSTICS]:
                entity = await binary_sensor.new_binary_sensor(config[CONF_DIAGNOSTICS][key])
                cg.add(getattr(var, method)(entity))
        for key, method in {"protocol_mode": "set_protocol_mode_sensor", "protocol_state": "set_protocol_state_sensor", "last_packet": "set_last_packet_sensor", "last_unknown_packet": "set_last_unknown_packet_sensor", "fan_decode_status": "set_fan_decode_status_sensor", "fan_decode_profile": "set_fan_decode_profile_sensor", "last_0x31_payload": "set_last_0x31_payload_sensor", "last_0x33_payload": "set_last_0x33_payload_sensor", "last_0x44_payload": "set_last_0x44_payload_sensor", "last_0x40_payload": "set_last_0x40_payload_sensor", "last_unknown_payload": "set_last_unknown_payload_sensor", "last_command_result": "set_last_command_result_sensor", "last_command_failure_reason": "set_last_command_failure_reason_sensor", "discovery_summary": "set_discovery_summary_sensor", "capture_export_csv": "set_capture_export_sensor"}.items():
            if key in config[CONF_DIAGNOSTICS]:
                entity = await text_sensor.new_text_sensor(config[CONF_DIAGNOSTICS][key])
                cg.add(getattr(var, method)(entity))

    if CONF_HORIZONTAL_SWING_SELECT in config:
        conf = config[CONF_HORIZONTAL_SWING_SELECT]
        hswing_select = await select.new_select(conf, options=HORIZONTAL_SWING_OPTIONS)
        await cg.register_component(hswing_select, conf)
        cg.add(var.set_horizontal_swing_select(hswing_select))

    if CONF_VERTICAL_SWING_SELECT in config:
        conf = config[CONF_VERTICAL_SWING_SELECT]
        vswing_select = await select.new_select(conf, options=VERTICAL_SWING_OPTIONS)
        await cg.register_component(vswing_select, conf)
        cg.add(var.set_vertical_swing_select(vswing_select))
    
    if CONF_DISPLAY_SELECT in config:
        conf = config[CONF_DISPLAY_SELECT]
        display_select = await select.new_select(conf, options=DISPLAY_OPTIONS)
        await cg.register_component(display_select, conf)
        cg.add(var.set_display_select(display_select))
    
    if CONF_DISPLAY_UNIT_SELECT in config:
        conf = config[CONF_DISPLAY_UNIT_SELECT]
        display_unit_select = await select.new_select(conf, options=DISPLAY_UNIT_OPTIONS)
        await cg.register_component(display_unit_select, conf)
        cg.add(var.set_display_unit_select(display_unit_select))

    if CONF_CURRENT_TEMPERATURE_SENSOR in config:
        sens = await cg.get_variable(config[CONF_CURRENT_TEMPERATURE_SENSOR])
        cg.add(var.set_current_temperature_sensor(sens))
        
    for s in [CONF_PLASMA_SWITCH, CONF_SLEEP_SWITCH, CONF_XFAN_SWITCH, CONF_SAVE_SWITCH]:
        if s in config:
            conf = config[s]
            a_switch = cg.new_Pvariable(conf[CONF_ID])
            await cg.register_component(a_switch, conf)
            await switch.register_switch(a_switch, conf)
            cg.add(getattr(var, f"set_{s}")(a_switch))
