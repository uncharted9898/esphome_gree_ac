import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, text_sensor
from esphome.const import CONF_ID

AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]

CONF_CLIMATE_ID = "climate_id"

CONF_LAST_0X32_PAYLOAD = "last_0x32_payload"
CONF_LAST_0X33_PAYLOAD = "last_0x33_payload"
CONF_LAST_0X34_PAYLOAD = "last_0x34_payload"
CONF_LAST_0X35_PAYLOAD = "last_0x35_payload"
CONF_LAST_0X36_PAYLOAD = "last_0x36_payload"
CONF_LAST_0X3C_PAYLOAD = "last_0x3c_payload"
CONF_LAST_0X40_PAYLOAD = "last_0x40_payload"
CONF_LAST_0X41_PAYLOAD = "last_0x41_payload"
CONF_LAST_0X42_PAYLOAD = "last_0x42_payload"
CONF_LAST_0X53_PAYLOAD = "last_0x53_payload"
CONF_ELECTRICAL_ENERGY_CAPABILITY = "electrical_energy_capability"
CONF_ENERGY_FLOW_CAPABILITY = "energy_flow_capability"
CONF_POWER_DISCOVERY_SUMMARY = "power_discovery_summary"

CONF_STATUS_INDOOR_TEMPERATURE = "status_indoor_temperature"
CONF_STATUS_OUTDOOR_AMBIENT_TEMPERATURE = "status_outdoor_ambient_temperature"
CONF_STATUS_HUMIDITY_SENSOR_FIELD_RAW = "status_humidity_sensor_field_raw"
CONF_STATUS_INDOOR_FAN_PORT_RAW = "status_indoor_fan_port_raw"
CONF_STATUS_ELC_ALL_KWH_CLEAR = "status_elc_all_kwh_clear"
CONF_STATUS_ELC_ERG = "status_elc_erg"
CONF_STATUS_ELC_GEAR_RAW = "status_elc_gear_raw"
CONF_STATUS_ELC_1KWH_RAW = "status_elc_1kwh_raw"

CONF_INDOOR_REPORT_TARGET_TEMPERATURE = "indoor_report_target_temperature"
CONF_INDOOR_REPORT_CURRENT_TEMPERATURE = "indoor_report_current_temperature"
CONF_INDOOR_COIL_TEMPERATURE_CANDIDATE = "indoor_coil_temperature_candidate"
CONF_INDOOR_SECONDARY_TEMPERATURE_CANDIDATE = (
    "indoor_secondary_temperature_candidate"
)
CONF_INDOOR_REPORT_BYTE_10_TEMPERATURE_HYPOTHESIS = (
    "indoor_report_byte_10_temperature_hypothesis"
)
CONF_INDOOR_REPORT_BYTE_25_TEMPERATURE_HYPOTHESIS = (
    "indoor_report_byte_25_temperature_hypothesis"
)
CONF_INDOOR_REPORT_BYTE_25_RAW = "indoor_report_byte_25_raw"
CONF_INDOOR_REPORT_BYTE_6_RAW = "indoor_report_byte_6_raw"
CONF_INDOOR_REPORT_BYTE_6_BIT_3 = "indoor_report_byte_6_bit_3"
CONF_INDOOR_REPORT_BYTE_6_BIT_5 = "indoor_report_byte_6_bit_5"
CONF_INDOOR_REPORT_DF_POINT_RAW = "indoor_report_df_point_raw"
CONF_INDOOR_REPORT_CPS_TEMPERATURE_RAW = "indoor_report_cps_temperature_raw"
CONF_INDOOR_REPORT_BYTE_14_BIT_5 = "indoor_report_byte_14_bit_5"
CONF_INDOOR_REPORT_BYTE_20_RAW = "indoor_report_byte_20_raw"
CONF_INDOOR_REPORT_BYTE_21_RAW = "indoor_report_byte_21_raw"
CONF_INDOOR_REPORT_BYTE_36_RAW = "indoor_report_byte_36_raw"
CONF_INDOOR_REPORT_BYTE_37_RAW = "indoor_report_byte_37_raw"
CONF_INDOOR_REPORT_BYTE_38_RAW = "indoor_report_byte_38_raw"

CONF_OUTDOOR_OPERATING_VALUE_RAW = "outdoor_operating_value_raw"
CONF_OUTDOOR_AMBIENT_TEMPERATURE_CANDIDATE = (
    "outdoor_ambient_temperature_candidate"
)
CONF_OUTDOOR_COIL_TEMPERATURE_CANDIDATE = "outdoor_coil_temperature_candidate"
CONF_COMPRESSOR_DISCHARGE_TEMPERATURE_CANDIDATE = (
    "compressor_discharge_temperature_candidate"
)
CONF_COMPRESSOR_FREQUENCY = "compressor_frequency"
CONF_OUTDOOR_FAN_SPEED_RAW = "outdoor_fan_speed_raw"
CONF_EXPANSION_VALVE_POSITION = "expansion_valve_position"
CONF_EXPANSION_VALVE_CLOSING = "expansion_valve_closing"
CONF_OUTDOOR_OPERATING_STATE_RAW = "outdoor_operating_state_raw"
CONF_OUTDOOR_REPORT_BYTE_4_RAW = "outdoor_report_byte_4_raw"
CONF_OUTDOOR_REPORT_BYTE_4_BIT_4 = "outdoor_report_byte_4_bit_4"
CONF_OUTDOOR_REPORT_BYTE_5_RAW = "outdoor_report_byte_5_raw"
CONF_OUTDOOR_REPORT_BYTE_6_RAW = "outdoor_report_byte_6_raw"
CONF_OUTDOOR_REPORT_BYTE_9_RAW = "outdoor_report_byte_9_raw"
CONF_OUTDOOR_REPORT_BYTE_10_RAW = "outdoor_report_byte_10_raw"
CONF_OUTDOOR_REPORT_BYTE_13_TEMPERATURE_HYPOTHESIS = (
    "outdoor_report_byte_13_temperature_hypothesis"
)
CONF_OUTDOOR_REPORT_BYTE_14_TEMPERATURE_HYPOTHESIS = (
    "outdoor_report_byte_14_temperature_hypothesis"
)
CONF_OUTDOOR_REPORT_BYTE_15_TEMPERATURE_HYPOTHESIS = (
    "outdoor_report_byte_15_temperature_hypothesis"
)
CONF_OUTDOOR_REPORT_BYTE_17_BIT_2 = "outdoor_report_byte_17_bit_2"
CONF_OUTDOOR_REPORT_BYTES_21_28_RAW = "outdoor_report_bytes_21_28_raw"
CONF_OUTDOOR_REPORT_BYTE_30_RAW = "outdoor_report_byte_30_raw"
CONF_OUTDOOR_REPORT_BYTE_31_BIT_6 = "outdoor_report_byte_31_bit_6"
CONF_OUTDOOR_REPORT_BYTE_36_RAW = "outdoor_report_byte_36_raw"
CONF_ENERGY_FLOW_RAW = "energy_flow_raw"

gree_oem_report_ns = cg.esphome_ns.namespace("gree_oem_report_sensors")
GreeOemReportSensors = gree_oem_report_ns.class_(
    "GreeOemReportSensors", cg.PollingComponent
)

sinclair_ac_ns = cg.esphome_ns.namespace("sinclair_ac")
SinclairAC = sinclair_ac_ns.class_("SinclairAC", cg.Component)

temperature_schema = sensor.sensor_schema(
    sensor.Sensor,
    unit_of_measurement="°C",
    accuracy_decimals=1,
    device_class="temperature",
    state_class="measurement",
)
frequency_schema = sensor.sensor_schema(
    sensor.Sensor,
    unit_of_measurement="Hz",
    accuracy_decimals=0,
    state_class="measurement",
)
raw_schema = sensor.sensor_schema(sensor.Sensor, accuracy_decimals=0)
bit_schema = binary_sensor.binary_sensor_schema(binary_sensor.BinarySensor)
payload_schema = text_sensor.text_sensor_schema(text_sensor.TextSensor)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GreeOemReportSensors),
        cv.Required(CONF_CLIMATE_ID): cv.use_id(SinclairAC),
        cv.Optional(CONF_LAST_0X32_PAYLOAD): payload_schema,
        cv.Optional(CONF_LAST_0X33_PAYLOAD): payload_schema,
        cv.Optional(CONF_LAST_0X34_PAYLOAD): payload_schema,
        cv.Optional(CONF_LAST_0X35_PAYLOAD): payload_schema,
        cv.Optional(CONF_LAST_0X36_PAYLOAD): payload_schema,
        cv.Optional(CONF_LAST_0X3C_PAYLOAD): payload_schema,
        cv.Optional(CONF_LAST_0X40_PAYLOAD): payload_schema,
        cv.Optional(CONF_LAST_0X41_PAYLOAD): payload_schema,
        cv.Optional(CONF_LAST_0X42_PAYLOAD): payload_schema,
        cv.Optional(CONF_LAST_0X53_PAYLOAD): payload_schema,
        cv.Optional(CONF_ELECTRICAL_ENERGY_CAPABILITY): payload_schema,
        cv.Optional(CONF_ENERGY_FLOW_CAPABILITY): payload_schema,
        cv.Optional(CONF_POWER_DISCOVERY_SUMMARY): payload_schema,
        cv.Optional(CONF_STATUS_INDOOR_TEMPERATURE): temperature_schema,
        cv.Optional(CONF_STATUS_OUTDOOR_AMBIENT_TEMPERATURE): temperature_schema,
        cv.Optional(CONF_STATUS_HUMIDITY_SENSOR_FIELD_RAW): raw_schema,
        cv.Optional(CONF_STATUS_INDOOR_FAN_PORT_RAW): raw_schema,
        cv.Optional(CONF_STATUS_ELC_ALL_KWH_CLEAR): bit_schema,
        cv.Optional(CONF_STATUS_ELC_ERG): bit_schema,
        cv.Optional(CONF_STATUS_ELC_GEAR_RAW): raw_schema,
        cv.Optional(CONF_STATUS_ELC_1KWH_RAW): raw_schema,
        cv.Optional(CONF_INDOOR_REPORT_TARGET_TEMPERATURE): temperature_schema,
        cv.Optional(CONF_INDOOR_REPORT_CURRENT_TEMPERATURE): temperature_schema,
        cv.Optional(CONF_INDOOR_COIL_TEMPERATURE_CANDIDATE): temperature_schema,
        cv.Optional(CONF_INDOOR_SECONDARY_TEMPERATURE_CANDIDATE): temperature_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_10_TEMPERATURE_HYPOTHESIS): temperature_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_25_TEMPERATURE_HYPOTHESIS): temperature_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_25_RAW): raw_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_6_RAW): raw_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_6_BIT_3): bit_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_6_BIT_5): bit_schema,
        cv.Optional(CONF_INDOOR_REPORT_DF_POINT_RAW): raw_schema,
        cv.Optional(CONF_INDOOR_REPORT_CPS_TEMPERATURE_RAW): raw_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_14_BIT_5): bit_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_20_RAW): raw_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_21_RAW): raw_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_36_RAW): raw_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_37_RAW): raw_schema,
        cv.Optional(CONF_INDOOR_REPORT_BYTE_38_RAW): raw_schema,
        cv.Optional(CONF_OUTDOOR_OPERATING_VALUE_RAW): raw_schema,
        cv.Optional(CONF_OUTDOOR_AMBIENT_TEMPERATURE_CANDIDATE): temperature_schema,
        cv.Optional(CONF_OUTDOOR_COIL_TEMPERATURE_CANDIDATE): temperature_schema,
        cv.Optional(CONF_COMPRESSOR_DISCHARGE_TEMPERATURE_CANDIDATE): temperature_schema,
        cv.Optional(CONF_COMPRESSOR_FREQUENCY): frequency_schema,
        cv.Optional(CONF_OUTDOOR_FAN_SPEED_RAW): raw_schema,
        cv.Optional(CONF_EXPANSION_VALVE_POSITION): raw_schema,
        cv.Optional(CONF_EXPANSION_VALVE_CLOSING): bit_schema,
        cv.Optional(CONF_OUTDOOR_OPERATING_STATE_RAW): raw_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_4_RAW): raw_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_4_BIT_4): bit_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_5_RAW): raw_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_6_RAW): raw_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_9_RAW): raw_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_10_RAW): raw_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_13_TEMPERATURE_HYPOTHESIS): temperature_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_14_TEMPERATURE_HYPOTHESIS): temperature_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_15_TEMPERATURE_HYPOTHESIS): temperature_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_17_BIT_2): bit_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTES_21_28_RAW): payload_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_30_RAW): raw_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_31_BIT_6): bit_schema,
        cv.Optional(CONF_OUTDOOR_REPORT_BYTE_36_RAW): raw_schema,
        cv.Optional(CONF_ENERGY_FLOW_RAW): raw_schema,
    }
).extend(cv.polling_component_schema("1s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    climate_var = await cg.get_variable(config[CONF_CLIMATE_ID])
    cg.add(var.set_climate(climate_var))

    text_entities = {
        CONF_LAST_0X32_PAYLOAD: "set_last_0x32_payload_sensor",
        CONF_LAST_0X33_PAYLOAD: "set_last_0x33_payload_sensor",
        CONF_LAST_0X34_PAYLOAD: "set_last_0x34_payload_sensor",
        CONF_LAST_0X35_PAYLOAD: "set_last_0x35_payload_sensor",
        CONF_LAST_0X36_PAYLOAD: "set_last_0x36_payload_sensor",
        CONF_LAST_0X3C_PAYLOAD: "set_last_0x3c_payload_sensor",
        CONF_LAST_0X40_PAYLOAD: "set_last_0x40_payload_sensor",
        CONF_LAST_0X41_PAYLOAD: "set_last_0x41_payload_sensor",
        CONF_LAST_0X42_PAYLOAD: "set_last_0x42_payload_sensor",
        CONF_LAST_0X53_PAYLOAD: "set_last_0x53_payload_sensor",
        CONF_ELECTRICAL_ENERGY_CAPABILITY: "set_electrical_energy_capability_sensor",
        CONF_ENERGY_FLOW_CAPABILITY: "set_energy_flow_capability_sensor",
        CONF_POWER_DISCOVERY_SUMMARY: "set_power_discovery_summary_sensor",
        CONF_OUTDOOR_REPORT_BYTES_21_28_RAW: "set_outdoor_report_bytes_21_28_raw_sensor",
    }
    for key, method in text_entities.items():
        if key in config:
            entity = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(var, method)(entity))

    sensor_entities = {
        CONF_STATUS_INDOOR_TEMPERATURE: "set_status_indoor_temperature_sensor",
        CONF_STATUS_OUTDOOR_AMBIENT_TEMPERATURE: "set_status_outdoor_ambient_temperature_sensor",
        CONF_STATUS_HUMIDITY_SENSOR_FIELD_RAW: "set_status_humidity_sensor_field_raw_sensor",
        CONF_STATUS_INDOOR_FAN_PORT_RAW: "set_status_indoor_fan_port_raw_sensor",
        CONF_STATUS_ELC_GEAR_RAW: "set_status_elc_gear_raw_sensor",
        CONF_STATUS_ELC_1KWH_RAW: "set_status_elc_1kwh_raw_sensor",
        CONF_INDOOR_REPORT_TARGET_TEMPERATURE: "set_indoor_report_target_temperature_sensor",
        CONF_INDOOR_REPORT_CURRENT_TEMPERATURE: "set_indoor_report_current_temperature_sensor",
        CONF_INDOOR_COIL_TEMPERATURE_CANDIDATE: "set_indoor_coil_temperature_candidate_sensor",
        CONF_INDOOR_SECONDARY_TEMPERATURE_CANDIDATE: "set_indoor_secondary_temperature_candidate_sensor",
        CONF_INDOOR_REPORT_BYTE_10_TEMPERATURE_HYPOTHESIS: "set_indoor_report_byte_10_temperature_hypothesis_sensor",
        CONF_INDOOR_REPORT_BYTE_25_TEMPERATURE_HYPOTHESIS: "set_indoor_report_byte_25_temperature_hypothesis_sensor",
        CONF_INDOOR_REPORT_BYTE_25_RAW: "set_indoor_report_byte_25_raw_sensor",
        CONF_INDOOR_REPORT_BYTE_6_RAW: "set_indoor_report_byte_6_raw_sensor",
        CONF_INDOOR_REPORT_DF_POINT_RAW: "set_indoor_report_df_point_raw_sensor",
        CONF_INDOOR_REPORT_CPS_TEMPERATURE_RAW: "set_indoor_report_cps_temperature_raw_sensor",
        CONF_INDOOR_REPORT_BYTE_20_RAW: "set_indoor_report_byte_20_raw_sensor",
        CONF_INDOOR_REPORT_BYTE_21_RAW: "set_indoor_report_byte_21_raw_sensor",
        CONF_INDOOR_REPORT_BYTE_36_RAW: "set_indoor_report_byte_36_raw_sensor",
        CONF_INDOOR_REPORT_BYTE_37_RAW: "set_indoor_report_byte_37_raw_sensor",
        CONF_INDOOR_REPORT_BYTE_38_RAW: "set_indoor_report_byte_38_raw_sensor",
        CONF_OUTDOOR_OPERATING_VALUE_RAW: "set_outdoor_operating_value_raw_sensor",
        CONF_OUTDOOR_AMBIENT_TEMPERATURE_CANDIDATE: "set_outdoor_ambient_temperature_candidate_sensor",
        CONF_OUTDOOR_COIL_TEMPERATURE_CANDIDATE: "set_outdoor_coil_temperature_candidate_sensor",
        CONF_COMPRESSOR_DISCHARGE_TEMPERATURE_CANDIDATE: "set_compressor_discharge_temperature_candidate_sensor",
        CONF_COMPRESSOR_FREQUENCY: "set_compressor_frequency_sensor",
        CONF_OUTDOOR_FAN_SPEED_RAW: "set_outdoor_fan_speed_raw_sensor",
        CONF_EXPANSION_VALVE_POSITION: "set_expansion_valve_position_sensor",
        CONF_OUTDOOR_OPERATING_STATE_RAW: "set_outdoor_operating_state_raw_sensor",
        CONF_OUTDOOR_REPORT_BYTE_4_RAW: "set_outdoor_report_byte_4_raw_sensor",
        CONF_OUTDOOR_REPORT_BYTE_5_RAW: "set_outdoor_report_byte_5_raw_sensor",
        CONF_OUTDOOR_REPORT_BYTE_6_RAW: "set_outdoor_report_byte_6_raw_sensor",
        CONF_OUTDOOR_REPORT_BYTE_9_RAW: "set_outdoor_report_byte_9_raw_sensor",
        CONF_OUTDOOR_REPORT_BYTE_10_RAW: "set_outdoor_report_byte_10_raw_sensor",
        CONF_OUTDOOR_REPORT_BYTE_13_TEMPERATURE_HYPOTHESIS: "set_outdoor_report_byte_13_temperature_hypothesis_sensor",
        CONF_OUTDOOR_REPORT_BYTE_14_TEMPERATURE_HYPOTHESIS: "set_outdoor_report_byte_14_temperature_hypothesis_sensor",
        CONF_OUTDOOR_REPORT_BYTE_15_TEMPERATURE_HYPOTHESIS: "set_outdoor_report_byte_15_temperature_hypothesis_sensor",
        CONF_OUTDOOR_REPORT_BYTE_30_RAW: "set_outdoor_report_byte_30_raw_sensor",
        CONF_OUTDOOR_REPORT_BYTE_36_RAW: "set_outdoor_report_byte_36_raw_sensor",
        CONF_ENERGY_FLOW_RAW: "set_energy_flow_raw_sensor",
    }
    for key, method in sensor_entities.items():
        if key in config:
            entity = await sensor.new_sensor(config[key])
            cg.add(getattr(var, method)(entity))

    binary_entities = {
        CONF_STATUS_ELC_ALL_KWH_CLEAR: "set_status_elc_all_kwh_clear_sensor",
        CONF_STATUS_ELC_ERG: "set_status_elc_erg_sensor",
        CONF_EXPANSION_VALVE_CLOSING: "set_expansion_valve_closing_sensor",
        CONF_INDOOR_REPORT_BYTE_6_BIT_3: "set_indoor_report_byte_6_bit_3_sensor",
        CONF_INDOOR_REPORT_BYTE_6_BIT_5: "set_indoor_report_byte_6_bit_5_sensor",
        CONF_INDOOR_REPORT_BYTE_14_BIT_5: "set_indoor_report_byte_14_bit_5_sensor",
        CONF_OUTDOOR_REPORT_BYTE_4_BIT_4: "set_outdoor_report_byte_4_bit_4_sensor",
        CONF_OUTDOOR_REPORT_BYTE_17_BIT_2: "set_outdoor_report_byte_17_bit_2_sensor",
        CONF_OUTDOOR_REPORT_BYTE_31_BIT_6: "set_outdoor_report_byte_31_bit_6_sensor",
    }
    for key, method in binary_entities.items():
        if key in config:
            entity = await binary_sensor.new_binary_sensor(config[key])
            cg.add(getattr(var, method)(entity))
