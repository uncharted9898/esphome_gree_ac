import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import text_sensor, uart

AUTO_LOAD = ["text_sensor"]

# sinclair_ac is a climate platform rather than a top-level ESPHome component
# domain, so listing it in DEPENDENCIES causes a false validation failure. The
# required climate_id use_id below already guarantees a SinclairAC instance.
DEPENDENCIES = ["uart"]

CONF_UART_ID = "uart_id"
CONF_CLIMATE_ID = "climate_id"
CONF_RESTORE_CONTROL = "restore_control"
CONF_FRAME_SPACING = "frame_spacing"
CONF_START_DELAY = "start_delay"
CONF_QUERY_RECOVERED_DATA = "query_recovered_data"
CONF_REPEAT_INTERVAL = "repeat_interval"
CONF_QUIESCE_DELAY = "quiesce_delay"
CONF_SCHEDULED_QUIESCE_DELAY = "scheduled_quiesce_delay"
CONF_SELECTOR_DISCOVERY = "selector_discovery"
CONF_MODULE_STATE_DISCOVERY = "module_state_discovery"
CONF_MODULE_STATE_PRIMARY_SELECTOR = "module_state_primary_selector"
CONF_MODULE_STATE_SECONDARY_SELECTOR = "module_state_secondary_selector"
CONF_OPERATING_PROFILE = "operating_profile"
CONF_OPERATING_PROFILE_CYCLES = "operating_profile_cycles"
CONF_QUERY_ENERGY_FLOW = "query_energy_flow"
CONF_FORCE_ELECTRICAL_PAGE_DISCOVERY = "force_electrical_page_discovery"
CONF_FORCE_ENERGY_FLOW_DISCOVERY = "force_energy_flow_discovery"
CONF_ENERGY_FLOW_QUERY_RESULT = "energy_flow_query_result"

gree_oem_probe_ns = cg.esphome_ns.namespace("gree_oem_probe")
GreeOemBootProbe = gree_oem_probe_ns.class_("GreeOemBootProbe", cg.Component)

sinclair_ac_ns = cg.esphome_ns.namespace("sinclair_ac")
SinclairAC = sinclair_ac_ns.class_("SinclairAC", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GreeOemBootProbe),
        cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Required(CONF_CLIMATE_ID): cv.use_id(SinclairAC),
        cv.Optional(CONF_RESTORE_CONTROL, default=True): cv.boolean,
        cv.Optional(CONF_START_DELAY, default="100ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_FRAME_SPACING, default="450ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=325), max=cv.TimePeriod(seconds=5)),
        ),
        cv.Optional(CONF_QUERY_RECOVERED_DATA, default=True): cv.boolean,
        cv.Optional(CONF_QUIESCE_DELAY, default="1800ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=1600), max=cv.TimePeriod(seconds=5)),
        ),
        cv.Optional(CONF_SCHEDULED_QUIESCE_DELAY, default="150ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=50), max=cv.TimePeriod(seconds=1)),
        ),
        cv.Optional(CONF_SELECTOR_DISCOVERY, default=False): cv.boolean,
        cv.Optional(CONF_MODULE_STATE_DISCOVERY, default=False): cv.boolean,
        cv.Optional(CONF_MODULE_STATE_PRIMARY_SELECTOR, default=0x04): cv.int_range(min=0, max=7),
        cv.Optional(CONF_MODULE_STATE_SECONDARY_SELECTOR, default=0x00): cv.int_range(min=0, max=7),
        cv.Optional(CONF_OPERATING_PROFILE, default=False): cv.boolean,
        cv.Optional(CONF_OPERATING_PROFILE_CYCLES, default=40): cv.int_range(min=1, max=120),
        cv.Optional(CONF_QUERY_ENERGY_FLOW, default=True): cv.boolean,
        cv.Optional(CONF_FORCE_ELECTRICAL_PAGE_DISCOVERY, default=False): cv.boolean,
        cv.Optional(CONF_FORCE_ENERGY_FLOW_DISCOVERY, default=False): cv.boolean,
        cv.Optional(CONF_ENERGY_FLOW_QUERY_RESULT): text_sensor.text_sensor_schema(
            text_sensor.TextSensor
        ),
        cv.Optional(CONF_REPEAT_INTERVAL): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(seconds=30)),
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    uart_var = await cg.get_variable(config[CONF_UART_ID])
    climate_var = await cg.get_variable(config[CONF_CLIMATE_ID])
    cg.add(var.set_uart(uart_var))
    cg.add(var.set_climate(climate_var))
    cg.add(var.set_restore_control(config[CONF_RESTORE_CONTROL]))
    cg.add(var.set_start_delay(config[CONF_START_DELAY]))
    cg.add(var.set_frame_spacing(config[CONF_FRAME_SPACING]))
    cg.add(var.set_query_recovered_data(config[CONF_QUERY_RECOVERED_DATA]))
    cg.add(var.set_quiesce_delay(config[CONF_QUIESCE_DELAY]))
    cg.add(var.set_scheduled_quiesce_delay(config[CONF_SCHEDULED_QUIESCE_DELAY]))
    cg.add(var.set_selector_discovery(config[CONF_SELECTOR_DISCOVERY]))
    cg.add(var.set_module_state_discovery(config[CONF_MODULE_STATE_DISCOVERY]))
    cg.add(var.set_module_state_selectors(
        config[CONF_MODULE_STATE_PRIMARY_SELECTOR],
        config[CONF_MODULE_STATE_SECONDARY_SELECTOR],
    ))
    cg.add(var.set_operating_profile(config[CONF_OPERATING_PROFILE]))
    cg.add(var.set_operating_profile_cycles(config[CONF_OPERATING_PROFILE_CYCLES]))
    cg.add(var.set_query_energy_flow(config[CONF_QUERY_ENERGY_FLOW]))
    cg.add(
        var.set_force_electrical_page_discovery(
            config[CONF_FORCE_ELECTRICAL_PAGE_DISCOVERY]
        )
    )
    cg.add(var.set_force_energy_flow_discovery(config[CONF_FORCE_ENERGY_FLOW_DISCOVERY]))
    if CONF_ENERGY_FLOW_QUERY_RESULT in config:
        result_sensor = await text_sensor.new_text_sensor(
            config[CONF_ENERGY_FLOW_QUERY_RESULT]
        )
        cg.add(var.set_energy_flow_query_result_sensor(result_sensor))
    if CONF_REPEAT_INTERVAL in config:
        cg.add(var.set_repeat_interval(config[CONF_REPEAT_INTERVAL]))
