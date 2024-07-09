#include "esp_adf_button.h"
#include "esp_adf_speaker.h"

namespace esphome {
namespace esp_adf {

esp_err_t ButtonHandler::input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx) {
    ESPADFSpeaker *instance = static_cast<ESPADFSpeaker*>(ctx);
    int32_t id = static_cast<int32_t>(reinterpret_cast<uintptr_t>(evt->data));

    // Read the ADC value
    int adc_value = adc1_get_raw(ADC1_CHANNEL_3);  // Replace with your ADC channel
    ESP_LOGI("ButtonHandler", "Button event callback received: id=%d, event type=%d, ADC value=%d", id, evt->type, adc_value);

    instance->handle_button_event(id, evt->type);
    return ESP_OK;
}

void ButtonHandler::handle_button_event(int32_t id, int32_t event_type) {
    ESP_LOGI("ButtonHandler", "Handle Button event received: id=%d", id);
    if (event_type != 1 && event_type != 3) { // Only process the event if the event_type is 1 click action or 3 long press action
        ESP_LOGI("ButtonHandler", "Ignoring event with type: %d", event_type);
        return;
    }
    uint32_t current_time = millis();
    static uint32_t last_button_press[7] = {0};
    uint32_t debounce_time = 200;

    if (id == BUTTON_MODE_ID) {
        debounce_time = 500;
    }

    if (current_time - last_button_press[id] > debounce_time) {
        switch (id) {
            case 0:
                ESP_LOGI("ButtonHandler", "Unknown Button detected");
                //volume_down();
                break;
            case 1:
                ESP_LOGI("ButtonHandler", "Record button detected");
                handle_rec_button();
                break;
            case 2:
                ESP_LOGI("ButtonHandler", "Set button detected");
                handle_set_button();
                break;
            case 3:
                ESP_LOGI("ButtonHandler", "Play button detected");
                handle_play_button();
                break;
            case 4:
                ESP_LOGI("ButtonHandler", "Mode button detected");
                handle_mode_button();
                break;
            case 5:
                ESP_LOGI("ButtonHandler", "Volume down detected");
                instance->volume_down();
                break;
            case 6:
                ESP_LOGI("ButtonHandler", "Volume up detected");
                instance->volume_up();
                break;
            default:
                ESP_LOGW("ButtonHandler", "Unhandled button event id: %d", id);
                break;
        }
        last_button_press[id] = current_time;
    }
}

void ButtonHandler::handle_mode_button() {
    // Implementation of mode button handling
}

void ButtonHandler::handle_play_button() {
    // Implementation of play button handling
}

void ButtonHandler::handle_set_button() {
    // Implementation of set button handling
}

void ButtonHandler::handle_rec_button() {
    // Implementation of record button handling
}

}  // namespace esp_adf
}  // namespace esphome

