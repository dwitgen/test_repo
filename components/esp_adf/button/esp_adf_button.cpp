#include "esp_log.h"
#include "board.h"
#include "esp_peripherals.h"
#include "periph_adc_button.h"
#include "input_key_service.h"

static const char *TAG = "CHECK_BOARD_BUTTONS";

esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx) {
    ESPADFSpeaker *instance = static_cast<ESPADFSpeaker*>(ctx);
    int32_t id = static_cast<int32_t>(reinterpret_cast<uintptr_t>(evt->data));

    // Read the ADC value
    int adc_value = adc1_get_raw(ADC1_CHANNEL_3);  // Replace with your ADC channel
    ESP_LOGI(TAG, "Button event callback received: id=%d, event type=%d, ADC value=%d", id, evt->type, adc_value);

    instance->handle_button_event(id);
    return ESP_OK;
}

void handle_button_event(int32_t id) {
    ESP_LOGI(TAG, "Handle Button event received: id=%d", id);
    uint32_t current_time = millis();
    static uint32_t last_button_press[6] = {0};
    uint32_t debounce_time = 200;

    if (id == BUTTON_MODE_ID) {
        debounce_time = 500;
    }

    if (current_time - last_button_press[id] > debounce_time) {
        switch (id) {
            case BUTTON_VOLUP_ID:
                ESP_LOGI(TAG, "Volume up detected");
                //volume_up();
                break;
            case BUTTON_VOLDOWN_ID:
                ESP_LOGI(TAG, "Volume down detected");
               // volume_down();
                break;
            case BUTTON_SET_ID:
                ESP_LOGI(TAG, "Set button detected");
                //handle_set_button();
                break;
            case BUTTON_PLAY_ID:
                ESP_LOGI(TAG, "Play button detected");
                //handle_play_button();
                break;
            case BUTTON_MODE_ID:
                ESP_LOGI(TAG, "Mode button detected");
                //handle_mode_button();
                break;
            case BUTTON_REC_ID:
                ESP_LOGI(TAG, "Record button detected");
                //handle_rec_button();
                break;
            default:
                ESP_LOGW(TAG, "Unhandled button event id: %d", id);
                break;
        }
        last_button_press[id] = current_time;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "[ 1 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[ 2 ] Initialize Button peripheral with board init");
    audio_board_key_init(set);

    ESP_LOGI(TAG, "[ 3 ] Create and start input key service");
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_cfg_t input_cfg = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = set;
    input_cfg.based_cfg.task_stack = 4 * 1024;
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);

    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, input_key_service_cb, NULL);

    ESP_LOGW(TAG, "[ 4 ] Waiting for a button to be pressed ...");
}
