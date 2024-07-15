#ifndef ESP_ADF_BUTTON_H
#define ESP_ADF_BUTTON_H

#include "../esp_adf_speaker.h"
#include <audio_hal.h>

namespace esphome {
namespace esp_adf {

class ButtonHandler {
 public:
  static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx);
  static void handle_button_event(ESPADFSpeaker *instance, int32_t id, int32_t event_type);
  static void handle_mode_button(ESPADFSpeaker *instance);
  static void handle_play_button(ESPADFSpeaker *instance);
  static void handle_set_button(ESPADFSpeaker *instance);
  static void handle_rec_button(ESPADFSpeaker *instance);
  static void volume_up(ESPADFSpeaker *instance);
  static void volume_down(ESPADFSpeaker *instance);

  // Volume control methods
  static void set_volume(ESPADFSpeaker *instance, int volume);
  static int get_current_volume(ESPADFSpeaker *instance);
};

}  // namespace esp_adf
}  // namespace esphome

#endif // ESP_ADF_BUTTON_H
