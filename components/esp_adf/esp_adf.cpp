#include "esp_adf.h"
#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF

#ifdef USE_ESP_ADF_BOARD
#include <board.h>
#include <periph_adc_button.h>
#endif

#include "esphome/core/log.h"

namespace esphome {
namespace esp_adf {

static const char *const TAG = "esp_adf";

void ESPADF::setup() {
#ifdef USE_ESP_ADF_BOARD
  ESP_LOGI(TAG, "Start codec chip");
  audio_board_handle_t board_handle = audio_board_init();
  audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

   // Initialize the peripheral set
  esp_periph_set_handle_t set = esp_periph_set_init(NULL);

  // Initialize the audio board keys
  esp_err_t ret = audio_board_key_init(set);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize audio board keys");
    return;
  }

  // Register the button event handler
  ESP_LOGI(TAG, "Registering button event handler...");
  esp_event_handler_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, button_event_handler, this);

#endif
}
void button_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data) {
  ESP_LOGI(TAG, "Button event received: base=%s, id=%d", base, id);
  // Handle the event here
}


float ESPADF::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
