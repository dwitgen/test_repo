#include "esp_adf_button.h"

static const char *TAG = "CHECK_BOARD_BUTTONS";

Button::Button() : adc_btn_handle(nullptr), input_key_info(nullptr), speaker_ctx(nullptr) {}

void Button::initialize() {
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
    periph_service_set_callback(input_ser, input_key_service_cb, this);

    ESP_LOGW(TAG, "[ 4 ] Waiting for a button to be pressed ...");
}

void Button::setSpeakerContext(void* ctx) {
    this->speaker_ctx = ctx;
}

esp_err_t Button::input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx) {
    Button *instance = static_cast<Button*>(ctx);
    int32_t id = static_cast<int32_t>(reinterpret_cast<uintptr_t>(evt->data));

    // Read the ADC value
    int adc_value = adc1_get_raw(ADC1_CHANNEL_3);  // Replace with your ADC channel
    ESP_LOGI(TAG, "Button event callback received: id=%d, event type=%d, ADC value=%d", id, evt->type, adc_value);

    if (instance->speaker_ctx != nullptr) {
        static_cast<esphome::esp_adf::ESPADFSpeaker*>(instance->speaker_ctx)->handle_button_event(id);
    }
    return ESP_OK;
}
