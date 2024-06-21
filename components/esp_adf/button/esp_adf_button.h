#ifndef ESP_ADF_BUTTON_H
#define ESP_ADF_BUTTON_H

#include "board.h"
#include "esp_peripherals.h"
#include "periph_adc_button.h"
#include "input_key_service.h"
#include "esp_log.h"

class Button {
public:
    Button();
    void initialize();
    void readButton();
    void setSpeakerContext(void* ctx);

private:
    esp_periph_handle_t adc_btn_handle;
    input_key_service_info_t *input_key_info;
    void* speaker_ctx;
    static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx);
};

#endif // ESP_ADF_BUTTON_H
