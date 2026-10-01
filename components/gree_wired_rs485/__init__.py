import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import binary_sensor, sensor, text_sensor, uart
from esphome.const import CONF_ID

AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]
DEPENDENCIES = ["uart"]

CONF_FRAME_TIMEOUT = "frame_timeout"
CONF_BUS_IDLE_TIMEOUT = "bus_idle_timeout"
CONF_LOG_FRAMES = "log_frames"
CONF_PASSIVE_SCAN = "passive_scan"
CONF_PASSIVE_SCAN_WINDOW = "passive_scan_window"
CONF_ACTIVE_PROBE = "active_probe"
CONF_ACTIVE_PROBE_INTERVAL = "active_probe_interval"
CONF_REGISTRATION_ATTEMPTS = "registration_attempts"
CONF_HARDWARE_HALF_DUPLEX = "hardware_half_duplex"
CONF_PERSISTENT_CONTROLLER = "persistent_controller"

CONF_BYTES_RECEIVED = "bytes_received"
CONF_VALID_FRAMES = "valid_frames"
CONF_CHECKSUM_FAILURES = "checksum_failures"
CONF_FRAME_TIMEOUTS = "frame_timeouts"
CONF_INVALID_LENGTHS = "invalid_lengths"
CONF_UNEXPECTED_TYPE_FRAMES = "unexpected_type_frames"
CONF_REFERENCE_LAYOUT_FRAMES = "reference_layout_frames"
CONF_ROUTE_VARIANT_FRAMES = "route_variant_frames"
CONF_UNKNOWN_ROUTE_FRAMES = "unknown_route_frames"
CONF_ROUTE_00_FF_FRAMES = "route_00_ff_frames"
CONF_ROUTE_FF_00_FRAMES = "route_ff_00_frames"
CONF_ROUTE_FF_40_FRAMES = "route_ff_40_frames"
CONF_LAST_SOURCE = "last_source"
CONF_LAST_DESTINATION = "last_destination"
CONF_LAST_BODY_LENGTH = "last_body_length"
CONF_RX_TRANSITIONS = "rx_transitions"
CONF_RX_HIGH_PERCENT = "rx_high_percent"
CONF_CONTROLLER_POLLS_SEEN = "controller_polls_seen"
CONF_CONTROLLER_RESPONSES_SENT = "controller_responses_sent"
CONF_REGISTERED_STATUS_FRAMES = "registered_status_frames"
CONF_REGISTERED_SETPOINT_CANDIDATE = "registered_setpoint_candidate"

CONF_LAST_FRAME = "last_frame"
CONF_LAST_PAYLOAD = "last_payload"
CONF_LAST_ROUTE = "last_route"
CONF_LAST_FRAME_CLASS = "last_frame_class"
CONF_LAST_CHANGES = "last_changes"
CONF_LAST_INVALID_FRAME = "last_invalid_frame"
CONF_PROTOCOL = "protocol"
CONF_SERIAL_PROFILE = "serial_profile"
CONF_LAST_RAW_RX = "last_raw_rx"
CONF_FF40_APPENDIX = "ff40_appendix"
CONF_CONTROLLER_STATE = "controller_state"
CONF_FF40_PAYLOAD = "ff40_payload"
CONF_FF40_CHANGES = "ff40_changes"
CONF_LAST_FRAME_ROLE = "last_frame_role"
CONF_POLL_PAYLOAD = "poll_payload"
CONF_POLL_CHANGES = "poll_changes"
CONF_FF40_INDEXED = "ff40_indexed"

CONF_BUS_ACTIVE = "bus_active"
CONF_LISTEN_ONLY = "listen_only"
CONF_RX_LINE_HIGH = "rx_line_high"
CONF_DIRECTION_HIGH = "direction_high"
CONF_ELECTRICAL_ACTIVITY = "electrical_activity"
CONF_DIRECTION_HIGH_SEEN = "direction_high_seen"
CONF_REGISTERED_STATUS = "registered_status"
CONF_RX_LINE_GPIO = "rx_line_gpio"
CONF_DIRECTION_GPIO = "direction_gpio"

gree_wired_ns = cg.esphome_ns.namespace("gree_wired_rs485")
GreeWiredRS485 = gree_wired_ns.class_(
    "GreeWiredRS485", cg.Component, uart.UARTDevice
)
SetControllerSetpointAction = gree_wired_ns.class_(
    "SetControllerSetpointAction", automation.Action
)
SetControllerModePowerRawAction = gree_wired_ns.class_(
    "SetControllerModePowerRawAction", automation.Action
)
SetControllerPowerAction = gree_wired_ns.class_(
    "SetControllerPowerAction", automation.Action
)
SetControllerPayloadByteAction = gree_wired_ns.class_(
    "SetControllerPayloadByteAction", automation.Action
)

counter_schema = sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0)
raw_value_schema = sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0)
percentage_schema = sensor.sensor_schema(sensor.Sensor, accuracy_decimals=1)
text_schema = text_sensor.text_sensor_schema(text_sensor.TextSensor)
binary_schema = binary_sensor.binary_sensor_schema(binary_sensor.BinarySensor)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GreeWiredRS485),
            cv.Optional(CONF_FRAME_TIMEOUT, default="75ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=20),
                    max=cv.TimePeriod(seconds=2),
                ),
            ),
            cv.Optional(CONF_BUS_IDLE_TIMEOUT, default="10s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(seconds=1),
                    max=cv.TimePeriod(minutes=5),
                ),
            ),
            cv.Optional(CONF_LOG_FRAMES, default=True): cv.boolean,
            cv.Optional(CONF_PASSIVE_SCAN, default=False): cv.boolean,
            cv.Optional(CONF_PASSIVE_SCAN_WINDOW, default="2s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=250),
                    max=cv.TimePeriod(seconds=10),
                ),
            ),
            cv.Optional(CONF_ACTIVE_PROBE, default=False): cv.boolean,
            cv.Optional(CONF_ACTIVE_PROBE_INTERVAL, default="1200ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=500),
                    max=cv.TimePeriod(seconds=10),
                ),
            ),
            cv.Optional(CONF_REGISTRATION_ATTEMPTS, default=4): cv.int_range(min=1, max=10),
            cv.Optional(CONF_HARDWARE_HALF_DUPLEX, default=False): cv.boolean,
            cv.Optional(CONF_PERSISTENT_CONTROLLER, default=False): cv.boolean,
            cv.Optional(CONF_BYTES_RECEIVED): counter_schema,
            cv.Optional(CONF_VALID_FRAMES): counter_schema,
            cv.Optional(CONF_CHECKSUM_FAILURES): counter_schema,
            cv.Optional(CONF_FRAME_TIMEOUTS): counter_schema,
            cv.Optional(CONF_INVALID_LENGTHS): counter_schema,
            cv.Optional(CONF_UNEXPECTED_TYPE_FRAMES): counter_schema,
            cv.Optional(CONF_REFERENCE_LAYOUT_FRAMES): counter_schema,
            cv.Optional(CONF_ROUTE_VARIANT_FRAMES): counter_schema,
            cv.Optional(CONF_UNKNOWN_ROUTE_FRAMES): counter_schema,
            cv.Optional(CONF_ROUTE_00_FF_FRAMES): counter_schema,
            cv.Optional(CONF_ROUTE_FF_00_FRAMES): counter_schema,
            cv.Optional(CONF_ROUTE_FF_40_FRAMES): counter_schema,
            cv.Optional(CONF_LAST_SOURCE): raw_value_schema,
            cv.Optional(CONF_LAST_DESTINATION): raw_value_schema,
            cv.Optional(CONF_LAST_BODY_LENGTH): raw_value_schema,
            cv.Optional(CONF_RX_TRANSITIONS): counter_schema,
            cv.Optional(CONF_RX_HIGH_PERCENT): percentage_schema,
            cv.Optional(CONF_CONTROLLER_POLLS_SEEN): counter_schema,
            cv.Optional(CONF_CONTROLLER_RESPONSES_SENT): counter_schema,
            cv.Optional(CONF_REGISTERED_STATUS_FRAMES): counter_schema,
            cv.Optional(CONF_REGISTERED_SETPOINT_CANDIDATE): sensor.sensor_schema(
                sensor.Sensor,
                unit_of_measurement="°C",
                accuracy_decimals=1,
                device_class="temperature",
            ),
            cv.Optional(CONF_LAST_FRAME): text_schema,
            cv.Optional(CONF_LAST_PAYLOAD): text_schema,
            cv.Optional(CONF_LAST_ROUTE): text_schema,
            cv.Optional(CONF_LAST_FRAME_CLASS): text_schema,
            cv.Optional(CONF_LAST_CHANGES): text_schema,
            cv.Optional(CONF_LAST_INVALID_FRAME): text_schema,
            cv.Optional(CONF_PROTOCOL): text_schema,
            cv.Optional(CONF_SERIAL_PROFILE): text_schema,
            cv.Optional(CONF_LAST_RAW_RX): text_schema,
            cv.Optional(CONF_FF40_APPENDIX): text_schema,
            cv.Optional(CONF_CONTROLLER_STATE): text_schema,
            cv.Optional(CONF_FF40_PAYLOAD): text_schema,
            cv.Optional(CONF_FF40_CHANGES): text_schema,
            cv.Optional(CONF_LAST_FRAME_ROLE): text_schema,
            cv.Optional(CONF_POLL_PAYLOAD): text_schema,
            cv.Optional(CONF_POLL_CHANGES): text_schema,
            cv.Optional(CONF_FF40_INDEXED): text_schema,
            cv.Optional(CONF_BUS_ACTIVE): binary_schema,
            cv.Optional(CONF_LISTEN_ONLY): binary_schema,
            cv.Optional(CONF_RX_LINE_HIGH): binary_schema,
            cv.Optional(CONF_DIRECTION_HIGH): binary_schema,
            cv.Optional(CONF_ELECTRICAL_ACTIVITY): binary_schema,
            cv.Optional(CONF_DIRECTION_HIGH_SEEN): binary_schema,
            cv.Optional(CONF_REGISTERED_STATUS): binary_schema,
            cv.Optional(CONF_RX_LINE_GPIO): cv.int_range(min=0, max=21),
            cv.Optional(CONF_DIRECTION_GPIO): cv.int_range(min=0, max=21),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "gree_wired_rs485",
    baud_rate=1200,
    require_rx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_frame_timeout(config[CONF_FRAME_TIMEOUT]))
    cg.add(var.set_bus_idle_timeout(config[CONF_BUS_IDLE_TIMEOUT]))
    cg.add(var.set_log_frames(config[CONF_LOG_FRAMES]))
    cg.add(var.set_passive_scan(config[CONF_PASSIVE_SCAN]))
    cg.add(var.set_passive_scan_window(config[CONF_PASSIVE_SCAN_WINDOW]))
    cg.add(var.set_active_probe(config[CONF_ACTIVE_PROBE]))
    cg.add(var.set_active_probe_interval(config[CONF_ACTIVE_PROBE_INTERVAL]))
    cg.add(var.set_registration_attempts(config[CONF_REGISTRATION_ATTEMPTS]))
    cg.add(var.set_hardware_half_duplex(config[CONF_HARDWARE_HALF_DUPLEX]))
    cg.add(var.set_persistent_controller(config[CONF_PERSISTENT_CONTROLLER]))
    if CONF_RX_LINE_GPIO in config:
        cg.add(var.set_rx_line_gpio(config[CONF_RX_LINE_GPIO]))
    if CONF_DIRECTION_GPIO in config:
        cg.add(var.set_direction_gpio(config[CONF_DIRECTION_GPIO]))

    sensor_entities = {
        CONF_BYTES_RECEIVED: "set_bytes_received_sensor",
        CONF_VALID_FRAMES: "set_valid_frames_sensor",
        CONF_CHECKSUM_FAILURES: "set_checksum_failures_sensor",
        CONF_FRAME_TIMEOUTS: "set_frame_timeouts_sensor",
        CONF_INVALID_LENGTHS: "set_invalid_lengths_sensor",
        CONF_UNEXPECTED_TYPE_FRAMES: "set_unexpected_type_frames_sensor",
        CONF_REFERENCE_LAYOUT_FRAMES: "set_reference_layout_frames_sensor",
        CONF_ROUTE_VARIANT_FRAMES: "set_route_variant_frames_sensor",
        CONF_UNKNOWN_ROUTE_FRAMES: "set_unknown_route_frames_sensor",
        CONF_ROUTE_00_FF_FRAMES: "set_route_00_ff_frames_sensor",
        CONF_ROUTE_FF_00_FRAMES: "set_route_ff_00_frames_sensor",
        CONF_ROUTE_FF_40_FRAMES: "set_route_ff_40_frames_sensor",
        CONF_LAST_SOURCE: "set_last_source_sensor",
        CONF_LAST_DESTINATION: "set_last_destination_sensor",
        CONF_LAST_BODY_LENGTH: "set_last_body_length_sensor",
        CONF_RX_TRANSITIONS: "set_rx_transitions_sensor",
        CONF_RX_HIGH_PERCENT: "set_rx_high_percent_sensor",
        CONF_CONTROLLER_POLLS_SEEN: "set_controller_polls_seen_sensor",
        CONF_CONTROLLER_RESPONSES_SENT: "set_controller_responses_sent_sensor",
        CONF_REGISTERED_STATUS_FRAMES: "set_registered_status_frames_sensor",
        CONF_REGISTERED_SETPOINT_CANDIDATE: "set_registered_setpoint_candidate_sensor",
    }
    for key, method in sensor_entities.items():
        if key in config:
            entity = await sensor.new_sensor(config[key])
            cg.add(getattr(var, method)(entity))

    text_entities = {
        CONF_LAST_FRAME: "set_last_frame_sensor",
        CONF_LAST_PAYLOAD: "set_last_payload_sensor",
        CONF_LAST_ROUTE: "set_last_route_sensor",
        CONF_LAST_FRAME_CLASS: "set_last_frame_class_sensor",
        CONF_LAST_CHANGES: "set_last_changes_sensor",
        CONF_LAST_INVALID_FRAME: "set_last_invalid_frame_sensor",
        CONF_PROTOCOL: "set_protocol_sensor",
        CONF_SERIAL_PROFILE: "set_serial_profile_sensor",
        CONF_LAST_RAW_RX: "set_last_raw_rx_sensor",
        CONF_FF40_APPENDIX: "set_ff40_appendix_sensor",
        CONF_CONTROLLER_STATE: "set_controller_state_sensor",
        CONF_FF40_PAYLOAD: "set_ff40_payload_sensor",
        CONF_FF40_CHANGES: "set_ff40_changes_sensor",
        CONF_LAST_FRAME_ROLE: "set_last_frame_role_sensor",
        CONF_POLL_PAYLOAD: "set_poll_payload_sensor",
        CONF_POLL_CHANGES: "set_poll_changes_sensor",
        CONF_FF40_INDEXED: "set_ff40_indexed_sensor",
    }
    for key, method in text_entities.items():
        if key in config:
            entity = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(var, method)(entity))

    binary_entities = {
        CONF_BUS_ACTIVE: "set_bus_active_sensor",
        CONF_LISTEN_ONLY: "set_listen_only_sensor",
        CONF_RX_LINE_HIGH: "set_rx_line_high_sensor",
        CONF_DIRECTION_HIGH: "set_direction_high_sensor",
        CONF_ELECTRICAL_ACTIVITY: "set_electrical_activity_sensor",
        CONF_DIRECTION_HIGH_SEEN: "set_direction_high_seen_sensor",
        CONF_REGISTERED_STATUS: "set_registered_status_sensor",
    }
    for key, method in binary_entities.items():
        if key in config:
            entity = await binary_sensor.new_binary_sensor(config[key])
            cg.add(getattr(var, method)(entity))


CONF_VALUE = "value"
CONF_INDEX = "index"

_SETPOINT_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(GreeWiredRS485),
        cv.Required(CONF_VALUE): cv.templatable(cv.float_range(min=0.0, max=127.5)),
    }
)

_MODE_POWER_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(GreeWiredRS485),
        cv.Required(CONF_VALUE): cv.templatable(cv.int_range(min=0, max=255)),
    }
)


@automation.register_action(
    "gree_wired_rs485.set_controller_setpoint",
    SetControllerSetpointAction,
    _SETPOINT_ACTION_SCHEMA,
    synchronous=True,
)
async def set_controller_setpoint_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    value = await cg.templatable(config[CONF_VALUE], args, cg.float_)
    cg.add(var.set_value(value))
    return var


@automation.register_action(
    "gree_wired_rs485.set_controller_mode_power_raw",
    SetControllerModePowerRawAction,
    _MODE_POWER_ACTION_SCHEMA,
    synchronous=True,
)
async def set_controller_mode_power_raw_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    value = await cg.templatable(config[CONF_VALUE], args, cg.uint8)
    cg.add(var.set_value(value))
    return var


_POWER_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(GreeWiredRS485),
        cv.Required(CONF_VALUE): cv.templatable(cv.boolean),
    }
)

_PAYLOAD_BYTE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(GreeWiredRS485),
        cv.Required(CONF_INDEX): cv.templatable(cv.int_range(min=0, max=32)),
        cv.Required(CONF_VALUE): cv.templatable(cv.int_range(min=0, max=255)),
    }
)


@automation.register_action(
    "gree_wired_rs485.set_controller_power",
    SetControllerPowerAction,
    _POWER_ACTION_SCHEMA,
    synchronous=True,
)
async def set_controller_power_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    value = await cg.templatable(config[CONF_VALUE], args, cg.bool_)
    cg.add(var.set_value(value))
    return var


@automation.register_action(
    "gree_wired_rs485.set_controller_payload_byte",
    SetControllerPayloadByteAction,
    _PAYLOAD_BYTE_ACTION_SCHEMA,
    synchronous=True,
)
async def set_controller_payload_byte_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    index = await cg.templatable(config[CONF_INDEX], args, cg.uint8)
    value = await cg.templatable(config[CONF_VALUE], args, cg.uint8)
    cg.add(var.set_index(index))
    cg.add(var.set_value(value))
    return var
