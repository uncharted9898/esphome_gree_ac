import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import uart

DEPENDENCIES = ["uart", "sinclair_ac"]

CONF_UART_ID = "uart_id"
CONF_CLIMATE_ID = "climate_id"
CONF_RESTORE_CONTROL = "restore_control"
CONF_FRAME_SPACING = "frame_spacing"
CONF_START_DELAY = "start_delay"

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
        cv.Optional(CONF_FRAME_SPACING, default="450ms"): cv.positive_time_period_milliseconds,
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
