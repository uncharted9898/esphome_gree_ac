#based on: https://github.com/DomiStyle/esphome-panasonic-ac

from esphome.const import (
    CONF_ID,
)
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, climate, sensor, select, switch, binary_sensor, text_sensor

AUTO_LOAD = ["switch", "sensor", "select"]
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
CONF_DEBUG = "debug"
CONF_DIAGNOSTICS = "diagnostics"

diagnostic_sensor_schema = sensor.sensor_schema(
    sensor.Sensor, accuracy_decimals=0, state_class="total_increasing"
)
diagnostics_schema = cv.Schema({
    cv.Optional("valid_rx_packets"): diagnostic_sensor_schema,
    cv.Optional("valid_tx_packets"): diagnostic_sensor_schema,
    cv.Optional("unknown_packets"): diagnostic_sensor_schema,
    cv.Optional("checksum_failures"): diagnostic_sensor_schema,
    cv.Optional("invalid_length_packets"): diagnostic_sensor_schema,
    cv.Optional("parser_resynchronizations"): diagnostic_sensor_schema,
    cv.Optional("last_packet_length"): sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0),
    cv.Optional("last_packet_type"): sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0),
    cv.Optional("communication"): binary_sensor.binary_sensor_schema(binary_sensor.BinarySensor),
    cv.Optional("receive_only"): binary_sensor.binary_sensor_schema(binary_sensor.BinarySensor),
    cv.Optional("protocol_state"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("last_packet"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
    cv.Optional("last_unknown_packet"): text_sensor.text_sensor_schema(text_sensor.TextSensor),
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
        cv.Optional(CONF_TRANSMIT_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_DEBUG, default={}): debug_schema,
        cv.Optional(CONF_DIAGNOSTICS): diagnostics_schema,
    }
).extend(uart.UART_DEVICE_SCHEMA)

CONFIG_SCHEMA = cv.All(
    SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SinclairACCNT),
            cv.Optional(CONF_CURRENT_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
        }
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate.register_climate(var, config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_transmit_enabled(config[CONF_TRANSMIT_ENABLED]))
    debug = config[CONF_DEBUG]
    cg.add(var.set_debug(debug["log_rx"], debug["log_tx"], debug["log_unknown_packets"], debug["log_packet_differences"], debug["maximum_hex_length"]))

    if CONF_DIAGNOSTICS in config:
        for key, method in {
            "valid_rx_packets": "set_valid_rx_packets_sensor", "valid_tx_packets": "set_valid_tx_packets_sensor",
            "unknown_packets": "set_unknown_packets_sensor", "checksum_failures": "set_checksum_failures_sensor",
            "invalid_length_packets": "set_invalid_length_sensor", "parser_resynchronizations": "set_parser_resync_sensor",
            "last_packet_length": "set_last_packet_length_sensor", "last_packet_type": "set_last_packet_type_sensor",
        }.items():
            if key in config[CONF_DIAGNOSTICS]:
                entity = await sensor.new_sensor(config[CONF_DIAGNOSTICS][key])
                cg.add(getattr(var, method)(entity))
        for key, method in {"communication": "set_communication_sensor", "receive_only": "set_receive_only_sensor"}.items():
            if key in config[CONF_DIAGNOSTICS]:
                entity = await binary_sensor.new_binary_sensor(config[CONF_DIAGNOSTICS][key])
                cg.add(getattr(var, method)(entity))
        for key, method in {"protocol_state": "set_protocol_state_sensor", "last_packet": "set_last_packet_sensor", "last_unknown_packet": "set_last_unknown_packet_sensor"}.items():
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
