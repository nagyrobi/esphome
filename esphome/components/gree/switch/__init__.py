import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_ID,
    CONF_ICON,
    CONF_MODEL,
    CONF_RESTORE_MODE,
    ENTITY_CATEGORY_CONFIG,
)
from .. import (
    gree_ns,
    MODELS
)

CONF_SWITCH_LIGHT = "light"
CONF_SWITCH_TURBO = "turbo"
CONF_SWITCH_HEALTH = "health"
CONF_SWITCH_XFAN = "xfan"

CONF_GREE_ID = "gree_climate_id"

ICON_DISPLAY_HIGHLIGHT = "mdi:fit-to-screen"
ICON_TURBO = "mdi:car-turbocharger"
ICON_IONIZE_ATOM = "mdi:atom"
ICON_GREEN_OUTLINE = "mdi:leaf-circle-outline"

GreeLightSwitch = gree_ns.class_(
    "GreeLightSwitch", switch.Switch, cg.Component
)
GreeTurboSwitch = gree_ns.class_(
    "GreeTurboSwitch", switch.Switch, cg.Component
)
GreeHealthSwitch = gree_ns.class_(
    "GreeHealthSwitch", switch.Switch, cg.Component
)
GreeXfanSwitch = gree_ns.class_(
    "GreeXfanSwitch", switch.Switch, cg.Component
)

CONF_SWITCH_LIGHT = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(GreeLightSwitch),
        cv.Required(CONF_GREE_ID): cv.use_id(GreeClimate), # ?? only accept id of a gree climate here
        cv.Required(CONF_MODEL): cv.enum(MODELS), # accept only GREE_YAN || GREE_YAA ||  GREE_YAC || GREE_YAC1FB9 here
        cv.Optional(CONF_ICON, default=ICON_DISPLAY_HIGHLIGHT): cv.icon,
        cv.Optional(
            CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_CONFIG
        ): cv.entity_category,
        cv.Optional(
            CONF_RESTORE_MODE, default=RESTORE_DEFAULT_ON
        ): cv.restore_mode,
    }
).extend(cv.COMPONENT_SCHEMA)

CONF_SWITCH_TURBO = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(GreeTurboSwitch),
        cv.Required(CONF_GREE_ID): cv.use_id(GreeClimate), # ?? only accept id of a gree climate here
        cv.Required(CONF_MODEL): cv.enum(MODELS), # accept only GREE_YAN || GREE_YAA ||  GREE_YAC || GREE_YAC1FB9 here
        cv.Optional(CONF_ICON, default=ICON_TURBO): cv.icon,
        cv.Optional(
            CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_CONFIG
        ): cv.entity_category,
        cv.Optional(
            CONF_RESTORE_MODE, default=RESTORE_DEFAULT_OFF
        ): cv.restore_mode,
    }
).extend(cv.COMPONENT_SCHEMA)

CONF_SWITCH_HEALTH = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(GreeHealthSwitch),
        cv.Required(CONF_GREE_ID): cv.use_id(GreeClimate), # ?? only accept id of a gree climate here
        cv.Required(CONF_MODEL): cv.enum(MODELS), # accept only GREE_YAN || GREE_YAA ||  GREE_YAC || GREE_YAC1FB9 here
        cv.Optional(CONF_ICON, default=ICON_IONIZE_ATOM): cv.icon,
        cv.Optional(
            CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_CONFIG
        ): cv.entity_category,
        cv.Optional(
            CONF_RESTORE_MODE, default=RESTORE_DEFAULT_OFF
        ): cv.restore_mode,
    }
).extend(cv.COMPONENT_SCHEMA)

CONF_SWITCH_XFAN = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(GreeXfanSwitch),
        cv.Required(CONF_GREE_ID): cv.use_id(GreeClimate), # ?? only accept id of a gree climate here
        cv.Required(CONF_MODEL): cv.enum(MODELS), # accept only GREE_YAN || GREE_YAA ||  GREE_YAC || GREE_YAC1FB9
        cv.Optional(CONF_ICON, default=ICON_GREEN_OUTLINE): cv.icon,
        cv.Optional(
            CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_CONFIG
        ): cv.entity_category,
        cv.Optional(
            CONF_RESTORE_MODE, default=RESTORE_DEFAULT_OFF
        ): cv.restore_mode,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config): # ??
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)
