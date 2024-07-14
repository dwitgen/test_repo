import os
import shutil
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import esp32
from esphome.const import CONF_ID, CONF_BOARD

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["esp32"]

CONF_ESP_ADF_ID = "esp_adf_id"
CONF_ESP_ADF = "esp_adf"

esp_adf_ns = cg.esphome_ns.namespace("esp_adf")
ESPADF = esp_adf_ns.class_("ESPADF", cg.Component)
ESPADFPipeline = esp_adf_ns.class_("ESPADFPipeline", cg.Parented.template(ESPADF))
ButtonHandler = esp_adf_ns.class_("ButtonHandler")

SUPPORTED_BOARDS = {
    "esp32s3box": "CONFIG_ESP32_S3_BOX_BOARD",
    "esp32s3boxlite": "CONFIG_ESP32_S3_BOX_LITE_BOARD",
    "esp32s3box3": "CONFIG_ESP32_S3_BOX_3_BOARD",
    "esp32s3korvo1": "CONFIG_ESP32_S3_KORVO1_BOARD",
    "esp32korvo1": "CONFIG_ESP32_KORVO1_BOARD"
}

def _default_board(config):
    config = config.copy()
    if board := config.get(CONF_BOARD) is None:
        board = esp32.get_board()
        if board in SUPPORTED_BOARDS:
            config[CONF_BOARD] = board
    return config

def final_validate_usable_board(platform: str):
    def _validate(adf_config):
        board = adf_config.get(CONF_BOARD)
        if board not in SUPPORTED_BOARDS:
            raise cv.Invalid(f"Board {board} is not supported by esp-adf {platform}")
        return adf_config

    return cv.Schema(
        {cv.Required(CONF_ESP_ADF_ID): fv.id_declaration_match_schema(_validate)},
        extra=cv.ALLOW_EXTRA,
    )

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESPADF),
            cv.Optional(CONF_BOARD): cv.string_strict,
        }
    ),
    _default_board,
    cv.only_with_esp_idf,
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add_define("USE_ESP_ADF")

    cg.add_platformio_option("build_unflags", "-Wl,--end-group")

    esp32.add_idf_component(
        name="esp-adf",
        repo="https://github.com/espressif/esp-adf",
        path="components",
        ref="v2.5",
        components=["*"],
        submodules=["components/esp-sr", "components/esp-adf-libs"],
    )

    esp32.add_idf_component(
        name="esp-dsp",
        repo="https://github.com/espressif/esp-dsp",
        ref="v1.2.0",
    )

    cg.add_platformio_option(
        "board_build.embed_txtfiles", "components/dueros_service/duer_profile"
    )

    if board := config.get(CONF_BOARD):
        cg.add_define("USE_ESP_ADF_BOARD")

        esp32.add_idf_sdkconfig_option(SUPPORTED_BOARDS[board], True)

        esp32.add_extra_script(
            "pre:apply_adf_patches.py",
            os.path.join(os.path.dirname(__file__), "apply_adf_patches.py.script"),
        )

        esp32.add_extra_script(
            "post:ensure_media_player.py",
            os.path.join(os.path.dirname(__file__), "ensure_media_player.py"),
        )

def ensure_media_player(source_dir, destination_dir):
    # Ensure the destination directory exists
    os.makedirs(destination_dir, exist_ok=True)

    # If the source directory exists, move it to the destination
    if os.path.exists(source_dir):
        if os.path.exists(destination_dir):
            shutil.rmtree(destination_dir)
        shutil.copytree(source_dir, destination_dir)
    else:
        print(f"Source directory {source_dir} does not exist. Skipping.")

# Ensure the button component is included
cg.add_platformio_option("build_src_filter", "+<components/esp_adf/button/*>")

# Hook the ensure_media_player function into the post-build process
source_dir = os.path.join(os.getcwd(), 'components', 'esp_adf', 'media_player')
destination_dir = os.path.join(os.getcwd(), 'src', 'esphome', 'components', 'esp_adf', 'media_player')
ensure_media_player(source_dir, destination_dir)
