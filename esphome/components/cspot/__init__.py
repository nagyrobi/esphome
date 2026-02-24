import esphome.codegen as cg
from esphome.components import audio_dac, speaker
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network"]

CONF_SINK = "sink"

cspot_ns = cg.esphome_ns.namespace("cspot")
CSpotComponent = cspot_ns.class_("CSpotComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CSpotComponent),
        cv.Required(CONF_SINK): cv.Any(
            cv.use_id(speaker.Speaker),
            cv.use_id(audio_dac.AudioDac),
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    sink = await cg.get_variable(config[CONF_SINK])
    cg.add(var.set_sink(sink))

    cg.add_library("cspot", None, "https://github.com/philippe44/cspot.git")
