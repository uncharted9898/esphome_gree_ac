import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, text_sensor, uart
from esphome.const import CONF_ID

AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]
DEPENDENCIES = ["uart"]

CONF_FRAME_TIMEOUT = "frame_timeout"
CONF_BUS_IDLE_TIMEOUT = "bus_idle_timeout"
CONF_LOG_FRAMES = "log_frames"

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

CONF_LAST_FRAME = "last_frame"
CONF_LAST_PAYLOAD = "last_payload"
CONF_LAST_ROUTE = "last_route"
CONF_LAST_FRAME_CLASS = "last_frame_class"
CONF_LAST_CHANGES = "last_changes"
CONF_LAST_INVALID_FRAME = "last_invalid_frame"
CONF_PROTOCOL = "protocol"

CONF_BUS_ACTIVE = "bus_active"
CONF_LISTEN_ONLY = "listen_only"

gree_wired_ns = cg.esphome_ns.namespace("gree_wired_rs485")
GreeWiredRS485 = gree_wired_ns.class_(
    "GreeWiredRS485", cg.Component, uart.UARTDevice
)

counter_schema = sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0)
raw_value_schema = sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0)
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
            cv.Optional(CONF_LAST_FRAME): text_schema,
            cv.Optional(CONF_LAST_PAYLOAD): text_schema,
            cv.Optional(CONF_LAST_ROUTE): text_schema,
            cv.Optional(CONF_LAST_FRAME_CLASS): text_schema,
            cv.Optional(CONF_LAST_CHANGES): text_schema,
            cv.Optional(CONF_LAST_INVALID_FRAME): text_schema,
            cv.Optional(CONF_PROTOCOL): text_schema,
            cv.Optional(CONF_BUS_ACTIVE): binary_schema,
            cv.Optional(CONF_LISTEN_ONLY): binary_schema,
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
    }
    for key, method in text_entities.items():
        if key in config:
            entity = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(var, method)(entity))

    binary_entities = {
        CONF_BUS_ACTIVE: "set_bus_active_sensor",
        CONF_LISTEN_ONLY: "set_listen_only_sensor",
    }
    for key, method in binary_entities.items():
        if key in config:
            entity = await binary_sensor.new_binary_sensor(config[key])
            cg.add(getattr(var, method)(entity))
