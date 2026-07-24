import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, text_sensor
from esphome.const import CONF_ID

AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]

CONF_CLIMATE_ID = "climate_id"
CONF_UPDATE_INTERVAL = "update_interval"
CONF_LAST_0X32_PAYLOAD = "last_0x32_payload"
CONF_LAST_0X34_PAYLOAD = "last_0x34_payload"
CONF_LAST_0X35_PAYLOAD = "last_0x35_payload"

CONF_INDOOR_REPORT_TARGET_TEMPERATURE = "indoor_report_target_temperature"
CONF_INDOOR_REPORT_CURRENT_TEMPERATURE = "indoor_report_current_temperature"
CONF_INDOOR_COIL_TEMPERATURE_CANDIDATE = "indoor_coil_temperature_candidate"
CONF_INDOOR_SECONDARY_TEMPERATURE_CANDIDATE = "indoor_secondary_temperature_candidate"
CONF_OUTDOOR_OPERATING_VALUE_RAW = "outdoor_operating_value_raw"
CONF_OUTDOOR_AMBIENT_TEMPERATURE_CANDIDATE = "outdoor_ambient_temperature_candidate"
CONF_OUTDOOR_COIL_TEMPERATURE_CANDIDATE = "outdoor_coil_temperature_candidate"
CONF_COMPRESSOR_DISCHARGE_TEMPERATURE_CANDIDATE = "compressor_discharge_temperature_candidate"

# Backward-compatible byte-number names retained from the first report decoder.
CONF_INDOOR_REPORT_BYTE_10_TEMPERATURE_HYPOTHESIS = "indoor_report_byte_10_temperature_hypothesis"
CONF_INDOOR_REPORT_BYTE_25_TEMPERATURE_HYPOTHESIS = "indoor_report_byte_25_temperature_hypothesis"
CONF_OUTDOOR_REPORT_BYTE_10_RAW = "outdoor_report_byte_10_raw"
CONF_OUTDOOR_REPORT_BYTE_13_TEMPERATURE_HYPOTHESIS = "outdoor_report_byte_13_temperature_hypothesis"
CONF_OUTDOOR_REPORT_BYTE_14_TEMPERATURE_HYPOTHESIS = "outdoor_report_byte_14_temperature_hypothesis"
CONF_OUTDOOR_REPORT_BYTE_15_TEMPERATURE_HYPOTHESIS = "outdoor_report_byte_15_temperature_hypothesis"

# Fields whose changes are explicitly watched by the recovered OEM fault parser.
# Their active polarity and code semantics remain unknown, so they are exposed as
# raw diagnostics instead of an inferred "active fault" entity.
CONF_INDOOR_REPORT_BYTE_14_BIT_5 = "indoor_report_byte_14_bit_5"
CONF_INDOOR_REPORT_BYTE_20_RAW = "indoor_report_byte_20_raw"
CONF_INDOOR_REPORT_BYTE_21_RAW = "indoor_report_byte_21_raw"
CONF_INDOOR_REPORT_BYTE_36_RAW = "indoor_report_byte_36_raw"
CONF_INDOOR_REPORT_BYTE_37_RAW = "indoor_report_byte_37_raw"
CONF_INDOOR_REPORT_BYTE_38_RAW = "indoor_report_byte_38_raw"
CONF_OUTDOOR_REPORT_BYTE_17_BIT_2 = "outdoor_report_byte_17_bit_2"
CONF_OUTDOOR_REPORT_BYTES_21_28_RAW = "outdoor_report_bytes_21_28_raw"
CONF_OUTDOOR_REPORT_BYTE_30_RAW = "outdoor_report_byte_30_raw"
CONF_OUTDOOR_REPORT_BYTE_31_BIT_6 = "outdoor_report_byte_31_bit_6"
CONF_OUTDOOR_REPORT_BYTE_36_RAW = "outdoor_report_byte_36_raw"

gree_oem_report_ns = cg.esphome_ns.namespace("gree_oem_report_sensors")
GreeOemReportSensors = gree_oem_report_ns.class_("GreeOemReportSensors", cg.PollingComponent)

sinclair_ac_ns = cg.esphome_ns.namespace("sinclair_ac")
SinclairAC = sinclair_ac_ns.class_("SinclairAC", cg.Component)

temperature_schema = sensor.sensor_schema(
    sensor.Sensor,
    unit_of_measurement="°C",
    accuracy_decimals=1,
    device_class="temperature",
    state_class="measurement",
)
raw_byte_schema = sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0)
bit_schema = binary_sensor.binary_sensor_schema(binary_sensor.BinarySensor)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GreeOemReportSensors),
        cv.Required(CONF_CLIMATE_ID): cv.use_id(SinclairAC),
        cv.Optional(CONF_LAST_0X32_PAYLOAD): text_sensor.text_sensor_schema(text_sensor.TextSensor),
        cv.Optional(CONF_LAST_0X34_PAYLOAD): text_sensor.text_sensor_schema(text_sensor.TextSensor),
        cv.Optional(CONF_LAST_0X35_PAYLOAD): text_sensor.text_sensor_schema(text_sensor.TextSensor),
        cv.Optional(CONF_INDOOR_REPORT_TARGET_TEMPERATURE): temperature_schema,
        cv.Optional(CONF_INDOOR_REPORT_CURRENT_TEMPERATURE): temperature_schema,
        cv.Optional(CONF_INDOOR_COIL_TEMPERATURE_CANDIDATE): temperature_schema,
        cv.Optional(CONF_INDOOR_SECONDARY_TEMPERATURE_CANDIDATE): temperature_schema,
        cv.Optional(CONF_OUTDOOR_OPERATING_VALUE_RAW): raw_byte_schema,
        cv.Optional(CONF_OUTDOOR_AMBIENT_TEMPERATURE_CANDIDATE): temperature_schema,
        cv.Optional(CONF_OUTDOOR_COIL_TEMPERATURE_CANDIDATE): temperature_schema,
        cv.Optional(CONF_COMPRESSOR_DISCHARGE_TEMPERATURE_CANDIDATE): temperature_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_10_TEMPERATURE_HYPOTHESIS): temperature_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_25_TEMPERATURE_HYPOTHESIS): temperature_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_10_RAW): raw_byte_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_13_TEMPERATURE_HYPOTHESIS): temperature_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_14_TEMPERATURE_HYPOTHESIS): temperature_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_15_TEMPERATURE_HYPOTHESIS): temperature_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_14_BIT_5): bit_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_20_RAW): raw_byte_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_21_RAW): raw_byte_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_36_RAW): raw_byte_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_37_RAW): raw_byte_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_38_RAW): raw_byte_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_17_BIT_2): bit_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTES_21_28_RAW): text_sensor.text_sensor_schema(text_sensor.TextSensor),
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_30_RAW): raw_byte_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_31_BIT_6): bit_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_36_RAW): raw_byte_schema,
    }
).extend(cv.polling_component_schema("1s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    climate_var = await cg.get_variable(config[CONF_CLIMATE_ID])
    cg.add(var.set_climate(climate_var))

    text_entities = {
        CONF_LAST_0X32_PAYLOAD: "set_last_0x32_payload_sensor",
        CONF_LAST_0X34_PAYLOAD: "set_last_0x34_payload_sensor",
        CONF_LAST_0X35_PAYLOAD: "set_last_0x35_payload_sensor",
        CONF_OUTDOOR_REPORT_BYTES_21_28_RAW: "set_outdoor_report_bytes_21_28_raw_sensor",
    }
    for key, method in text_entities.items():
        if key in config:
            entity = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(var, method)(entity))

    sensor_entities = {
        CONF_INDOOR_REPORT_TARGET_TEMPERATURE: "set_indoor_report_target_temperature_sensor",
        CONF_INDOOR_REPORT_CURRENT_TEMPERATURE: "set_indoor_report_current_temperature_sensor",
        CONF_INDOOR_COIL_TEMPERATURE_CANDIDATE: "set_indoor_coil_temperature_candidate_sensor",
        CONF_INDOOR_SECONDARY_TEMPERATURE_CANDIDATE: "set_indoor_secondary_temperature_candidate_sensor",
        CONF_OUTDOOR_OPERATING_VALUE_RAW: "set_outdoor_operating_value_raw_sensor",
        CONF_OUTDOOR_AMBIENT_TEMPERATURE_CANDIDATE: "set_outdoor_ambient_temperature_candidate_sensor",
        CONF_OUTDOOR_COIL_TEMPERATURE_CANDIDATE: "set_outdoor_coil_temperature_candidate_sensor",
        CONF_COMPRESSOR_DISCHARGE_TEMPERATURE_CANDIDATE: "set_compressor_discharge_temperature_candidate_sensor",
        CONF_INDOOR_REPORT_BYTE_10_TEMPERATURE_HYPOTHESIS: "set_indoor_report_byte_10_temperature_hypothesis_sensor",
        CONF_INDOOR_REPORT_BYTE_25_TEMPERATURE_HYPOTHESIS: "set_indoor_report_byte_25_temperature_hypothesis_sensor",
        CONF_OUTDOOR_REPORT_BYTE_10_RAW: "set_outdoor_report_byte_10_raw_sensor",
        CONF_OUTDOOR_REPORT_BYTE_13_TEMPERATURE_HYPOTHESIS: "set_outdoor_report_byte_13_temperature_hypothesis_sensor",
        CONF_OUTDOOR_REPORT_BYTE_14_TEMPERATURE_HYPOTHESIS: "set_outdoor_report_byte_14_temperature_hypothesis_sensor",
        CONF_OUTDOOR_REPORT_BYTE_15_TEMPERATURE_HYPOTHESIS: "set_outdoor_report_byte_15_temperature_hypothesis_sensor",
        CONF_INDOOR_REPORT_BYTE_20_RAW: "set_indoor_report_byte_20_raw_sensor",
        CONF_INDOOR_REPORT_BYTE_21_RAW: "set_indoor_report_byte_21_raw_sensor",
        CONF_INDOOR_REPORT_BYTE_36_RAW: "set_indoor_report_byte_36_raw_sensor",
        CONF_INDOOR_REPORT_BYTE_37_RAW: "set_indoor_report_byte_37_raw_sensor",
        CONF_INDOOR_REPORT_BYTE_38_RAW: "set_indoor_report_byte_38_raw_sensor",
        CONF_OUTDOOR_REPORT_BYTE_30_RAW: "set_outdoor_report_byte_30_raw_sensor",
        CONF_OUTDOOR_REPORT_BYTE_36_RAW: "set_outdoor_report_byte_36_raw_sensor",
    }
    for key, method in sensor_entities.items():
        if key in config:
            entity = await sensor.new_sensor(config[key])
            cg.add(getattr(var, method)(entity))

    binary_sensor_entities = {
        CONF_INDOOR_REPORT_BYTE_14_BIT_5: "set_indoor_report_byte_14_bit_5_sensor",
        CONF_OUTDOOR_REPORT_BYTE_17_BIT_2: "set_outdoor_report_byte_17_bit_2_sensor",
        CONF_OUTDOOR_REPORT_BYTE_31_BIT_6: "set_outdoor_report_byte_31_bit_6_sensor",
    }
    for key, method in binary_sensor_entities.items():
        if key in config:
            entity = await binary_sensor.new_binary_sensor(config[key])
            cg.add(getattr(var, method)(entity))
