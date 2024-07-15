#include "esp_adf_button.h"
#include "../speaker/esp_adf_speaker.h"

namespace esphome {
namespace esp_adf {

esp_err_t ButtonHandler::input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx) {
    ESPADFSpeaker *instance = static_cast<ESPADFSpeaker*>(ctx);
    int32_t id = static_cast<int32_t>(reinterpret_cast<uintptr_t>(evt->data));

    // Read the ADC value
    int adc_value = adc1_get_raw(ADC1_CHANNEL_3);  // Replace with your ADC channel
    ESP_LOGI("ButtonHandler", "Button event callback received: id=%d, event type=%d, ADC value=%d", id, evt->type, adc_value);

    handle_button_event(instance, id, evt->type);
    return ESP_OK;
}

void ButtonHandler::handle_button_event(ESPADFSpeaker *instance, int32_t id, int32_t event_type) {
    ESP_LOGI("ButtonHandler", "Handle Button event received: id=%d", id);
    if (event_type != 1 && event_type != 3) { // Only process the event if the event_type is 1 click action or 3 long press action
        ESP_LOGI("ButtonHandler", "Ignoring event with type: %d", event_type);
        return;
    }
    uint32_t current_time = esp_timer_get_time() / 1000;  // Use esp_timer_get_time() to get the current time in milliseconds
    static uint32_t last_button_press[7] = {0};
    uint32_t debounce_time = 200;

    if (id == BUTTON_MODE_ID) {
        debounce_time = 500;
    }

    if (current_time - last_button_press[id] > debounce_time) {
        switch (id) {
            case 0:
                ESP_LOGI("ButtonHandler", "Unknown Button detected");
                break;
            case 1:
                ESP_LOGI("ButtonHandler", "Record button detected");
                handle_rec_button(instance);
                break;
            case 2:
                ESP_LOGI("ButtonHandler", "Set button detected");
                handle_set_button(instance);
                break;
            case 3:
                ESP_LOGI("ButtonHandler", "Play button detected");
                handle_play_button(instance);
                break;
            case 4:
                ESP_LOGI("ButtonHandler", "Mode button detected");
                handle_mode_button(instance);
                break;
            case 5:
                ESP_LOGI("ButtonHandler", "Volume down detected");
                volume_down(instance);
                break;
            case 6:
                ESP_LOGI("ButtonHandler", "Volume up detected");
                volume_up(instance);
                break;
            default:
                ESP_LOGW("ButtonHandler", "Unhandled button event id: %d", id);
                break;
        }
        last_button_press[id] = current_time;
    }
}

void ButtonHandler::handle_mode_button(ESPADFSpeaker *instance) {
    if (instance->state_ != speaker::STATE_RUNNING && instance->state_ != speaker::STATE_STARTING) {
        ESP_LOGI("ButtonHandler", "Mode button, speaker stopped");
        instance->play_url("http://streaming.tdiradio.com:8000/house.mp3");
    } else {
        ESP_LOGI("ButtonHandler", "State is stopping");
        instance->cleanup_audio_pipeline();
        instance->stop();
    }
}

void ButtonHandler::handle_play_button(ESPADFSpeaker *instance) {
    ESP_LOGI("ButtonHandler", "Play button action");
    // Add code to play
}

void ButtonHandler::handle_set_button(ESPADFSpeaker *instance) {
    ESP_LOGI("ButtonHandler", "Set button action");
    // Add code to handle set action
}

void ButtonHandler::handle_rec_button(ESPADFSpeaker *instance) {
    ESP_LOGI("ButtonHandler", "Record button action");
    // Add code to start recording
}

void ButtonHandler::volume_up(ESPADFSpeaker *instance) {
    ESP_LOGI("ButtonHandler", "Volume up button pressed");
    int current_volume = instance->get_current_volume();
    instance->set_volume(current_volume + 10);
}

void ButtonHandler::volume_down(ESPADFSpeaker *instance) {
    ESP_LOGI("ButtonHandler", "Volume down button pressed");
    int current_volume = instance->get_current_volume();
    instance->set_volume(current_volume - 10);
}

}  // namespace esp_adf
}  // namespace esphome
