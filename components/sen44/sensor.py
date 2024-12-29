from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c, sensirion_common, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    CONF_OFFSET,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_4_0,
    CONF_PM_10_0,
    CONF_TEMPERATURE,
    CONF_TEMPERATURE_COMPENSATION,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_TEMPERATURE,
    ICON_CHEMICAL_WEAPON,
    ICON_RADIATOR,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_PERCENT,
)

CODEOWNERS = ["@martgras"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

sen44_ns = cg.esphome_ns.namespace("sen44")
SEN44Component = sen44_ns.class_(
    "SEN44Component", cg.PollingComponent, sensirion_common.SensirionI2CDevice
)


CONF_AUTO_CLEANING_INTERVAL = "auto_cleaning_interval"
CONF_VOC = "voc"


# Actions
StartFanAction = sen44_ns.class_("StartFanAction", automation.Action)


def float_previously_pct(value):
    if isinstance(value, str) and "%" in value:
        raise cv.Invalid(
            f"The value '{value}' is a percentage. Suggested value: {float(value.strip('%')) / 100}"
        )
    return value


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SEN44Component),
            cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PM1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PM25,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_4_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PM10,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AUTO_CLEANING_INTERVAL): cv.update_interval,
            cv.Optional(CONF_VOC): sensor.sensor_schema(
                icon=ICON_RADIATOR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_AQI,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE_COMPENSATION): cv.Schema(
                {
                    cv.Optional(CONF_OFFSET, default=0): cv.float_,
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x69))
)

SENSOR_MAP = {
    CONF_PM_1_0: "set_pm_1_0_sensor",
    CONF_PM_2_5: "set_pm_2_5_sensor",
    CONF_PM_4_0: "set_pm_4_0_sensor",
    CONF_PM_10_0: "set_pm_10_0_sensor",
    CONF_VOC: "set_voc_sensor",
    CONF_TEMPERATURE: "set_temperature_sensor",
    CONF_HUMIDITY: "set_humidity_sensor",
}

SETTING_MAP = {
    CONF_AUTO_CLEANING_INTERVAL: "set_auto_cleaning_interval",
}


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    for key, funcName in SETTING_MAP.items():
        if key in config:
            cg.add(getattr(var, funcName)(config[key]))

    for key, funcName in SENSOR_MAP.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, funcName)(sens))

    if CONF_TEMPERATURE_COMPENSATION in config:
        cg.add(
            var.set_temperature_compensation(
                config[CONF_TEMPERATURE_COMPENSATION][CONF_OFFSET]
            )
        )


SEN44_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(SEN44Component),
    }
)


@automation.register_action(
    "sen44.start_fan_autoclean", StartFanAction, SEN44_ACTION_SCHEMA
)
async def sen54_fan_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
