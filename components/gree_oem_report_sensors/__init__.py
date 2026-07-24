import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor
from esphome.const import CONF_ID

AUTO_LOAD = ["sensor", "text_sensor"]

CONF_CLIMATE_ID = "climate_id"
CONF_UPDATE_INTERVAL = "update_interval"
CONF_LAST_0X32_PAYLOAD = "last_0x32_payload"
CONF_LAST_0X34_PAYLOAD = "last_0x34_payload"
CONF_LAST_0X35_PAYLOAD = "last_0x35_payload"
CONF_INDOOR_REPORT_TARGET_TEMPERATURE = "indoor_report_target_temperature"
CONF_INDOOR_REPORT_CURRENT_TEMPERATURE = "indoor_report_current_temperature"
CONF_INDOOR_REPORT_BYTE_10_TEMPERATURE_HYPOTHESIS = "indoor_report_byte_10_temperature_hypothesis"
CONF_INDOOR_REPORT_BYTE_25_TEMPERATURE_HYPOTHESIS = "indoor_report_byte_25_temperature_hypothesis"
CONF_OUTDOOR_REPORT_BYTE_10_RAW = "outdoor_report_byte_10_raw"
CONF_OUTDOOR_REPORT_BYTE_13_TEMPERATURE_HYPOTHESIS = "outdoor_report_byte_13_temperature_hypothesis"
CONF_OUTDOOR_REPORT_BYTE_14_TEMPERATURE_HYPOTHESIS = "outdoor_report_byte_14_temperature_hypothesis"
CONF_OUTDOOR_REPORT_BYTE_15_TEMPERATURE_HYPOTHESIS = "outdoor_report_byte_15_temperature_hypothesis"

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

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GreeOemReportSensors),
        cv.Required(CONF_CLIMATE_ID): cv.use_id(SinclairAC),
        cv.Optional(CONF_LAST_0X32_PAYLOAD): text_sensor.text_sensor_schema(text_sensor.TextSensor),
        cv.Optional(CONF_LAST_0X34_PAYLOAD): text_sensor.text_sensor_schema(text_sensor.TextSensor),
        cv.Optional(CONF_LAST_0X35_PAYLOAD): text_sensor.text_sensor_schema(text_sensor.TextSensor),
        cv.Optional(CONF_INDOOR_REPORT_TARGET_TEMPERATURE): temperature_schema,
        cv.Optional(CONF_INDOOR_REPORT_CURRENT_TEMPERATURE): temperature_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_10_TEMPERATURE_HYPOTHESIS): temperature_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_25_TEMPERATURE_HYPOTHESIS): temperature_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_10_RAW): sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0),
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_13_TEMPERATURE_HYPOTHESIS): temperature_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_14_TEMPERATURE_HYPOTHESIS): temperature_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_15_TEMPERATURE_HYPOTHESIS): temperature_schema,
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
    }
    for key, method in text_entities.items():
        if key in config:
            entity = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(var, method)(entity))

    sensor_entities = {
        CONF_INDOOR_REPORT_TARGET_TEMPERATURE: "set_indoor_report_target_temperature_sensor",
        CONF_INDOOR_REPORT_CURRENT_TEMPERATURE: "set_indoor_report_current_temperature_sensor",
        CONF_INDOOR_REPORT_BYTE_10_TEMPERATURE_HYPOTHESIS: "set_indoor_report_byte_10_temperature_hypothesis_sensor",
        CONF_INDOOR_REPORT_BYTE_25_TEMPERATURE_HYPOTHESIS: "set_indoor_report_byte_25_temperature_hypothesis_sensor",
        CONF_OUTDOOR_REPORT_BYTE_10_RAW: "set_outdoor_report_byte_10_raw_sensor",
        CONF_OUTDOOR_REPORT_BYTE_13_TEMPERATURE_HYPOTHESIS: "set_outdoor_report_byte_13_temperature_hypothesis_sensor",
        CONF_OUTDOOR_REPORT_BYTE_14_TEMPERATURE_HYPOTHESIS: "set_outdoor_report_byte_14_temperature_hypothesis_sensor",
        CONF_OUTDOOR_REPORT_BYTE_15_TEMPERATURE_HYPOTHESIS: "set_outdoor_report_byte_15_temperature_hypothesis_sensor",
    }
    for key, method in sensor_entities.items():
        if key in config:
            entity = await sensor.new_sensor(config[key])
            cg.add(getattr(var, method)(entity))
