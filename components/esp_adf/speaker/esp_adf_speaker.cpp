#include "esp_adf_speaker.h"
#include "../button/esp_adf_button.h"

#ifdef USE_ESP_IDF

#include <driver/i2s.h>
#include <driver/gpio.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <audio_hal.h>
#include <filter_resample.h>
#include <i2s_stream.h>
#include <raw_stream.h>
#include "esp_http_client.h"
#include "http_stream.h"
#include "audio_pipeline.h"
#include "mp3_decoder.h"

#include "esp_peripherals.h"
#include "periph_adc_button.h"
#include "input_key_service.h"

#ifdef USE_ESP_ADF_BOARD
#include <board.h>
#endif

namespace esphome {
namespace esp_adf {

static const size_t BUFFER_COUNT = 50;
static const char *const TAG = "esp_adf.speaker";

#define ADC_WIDTH_BIT    ADC_WIDTH_BIT_12
#define ADC_ATTEN        ADC_ATTEN_DB_12

#ifndef ESP_EVENT_ANY_ID
#define ESP_EVENT_ANY_ID -1
#endif

void ESPADFSpeaker::setup() {
    ESP_LOGCONFIG(TAG, "Setting up ESP ADF Speaker...");

    adc1_config_width(ADC_WIDTH_BIT);
    adc1_config_channel_atten((adc1_channel_t)ADC1_CHANNEL_3, ADC_ATTEN);
    
    #ifdef USE_ESP_ADF_BOARD
    gpio_num_t pa_enable_gpio = static_cast<gpio_num_t>(get_pa_enable_gpio());
    #endif

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PA_ENABLE_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(PA_ENABLE_GPIO, 0);

    ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);

    this->buffer_queue_.storage = allocator.allocate(sizeof(StaticQueue_t) + (BUFFER_COUNT * sizeof(DataEvent)));
    if (this->buffer_queue_.storage == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate buffer queue!");
        this->mark_failed();
        return;
    }

    this->buffer_queue_.handle =
        xQueueCreateStatic(BUFFER_COUNT, sizeof(DataEvent), this->buffer_queue_.storage + sizeof(StaticQueue_t),
                           (StaticQueue_t *) (this->buffer_queue_.storage));

    this->event_queue_ = xQueueCreate(20, sizeof(TaskEvent));
    if (this->event_queue_ == nullptr) {
        ESP_LOGW(TAG, "Could not allocate event queue.");
        this->mark_failed();
        return;
    }

    uint32_t volume_sensor_key = 0;
    for (auto *sensor : App.get_sensors()) {
        if (sensor->get_name() == "generic_volume_sensor") {
            volume_sensor_key = sensor->get_object_id_hash();
            break;
        }
    }

    if (volume_sensor_key != 0) {
        this->volume_sensor = App.get_sensor_by_key(volume_sensor_key, true);
        ESP_LOGI(TAG, "Internal generic volume sensor initialized successfully: %s", this->volume_sensor->get_name().c_str());
    } else {
        ESP_LOGE(TAG, "Failed to find key for internal generic volume sensor");
    }

    if (this->volume_sensor == nullptr) {
        ESP_LOGE(TAG, "Failed to get internal generic volume sensor component");
    } else {
        ESP_LOGI(TAG, "Internal generic volume sensor initialized correctly");
    }

    ButtonHandler::set_volume(this, volume_);

    int initial_volume = ButtonHandler::get_current_volume(this);
    ButtonHandler::set_volume(this, initial_volume);

    // Initialize the peripheral set with increased queue size
    ESP_LOGI(TAG, "Initializing peripheral set...");
    esp_periph_config_t periph_cfg = {
        .task_stack = 16384,
        .task_prio = 10,
        .task_core = 0,
        .extern_stack = false
    };
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    if (!set) {
        ESP_LOGE(TAG, "Failed to initialize peripheral set");
        return;
    }

    // Initialize the audio board keys
    ESP_LOGI(TAG, "Initializing audio board keys...");
    audio_board_key_init(set);

    ESP_LOGI(TAG, "[ 3 ] Create and start input key service");
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_cfg_t input_cfg = {
        .based_cfg = {
            .task_stack = ADC_BUTTON_STACK_SIZE,
            .task_prio = ADC_BUTTON_TASK_PRIORITY,
            .task_core = ADC_BUTTON_TASK_CORE_ID,
            .task_func = nullptr,
            .extern_stack = false,
            .service_start = nullptr,
            .service_stop = nullptr,
            .service_destroy = nullptr,
            .service_ioctl = nullptr,
            .service_name = nullptr,
            .user_data = nullptr
        },
        .handle = set
    };
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);
    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, ButtonHandler::input_key_service_cb, this);

    this->initialize_audio_pipeline();
}

void ESPADFSpeaker::initialize_audio_pipeline() {
    esp_err_t ret;

    ret = configure_resample_filter(&this->http_filter_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing resample filter: %s", esp_err_to_name(ret));
        return;
    }

    ret = configure_i2s_stream_writer_http(&this->i2s_stream_writer_http_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing I2S stream writer for HTTP: %s", esp_err_to_name(ret));
        return;
    }

    ret = configure_i2s_stream_writer_raw(&this->i2s_stream_writer_raw_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing I2S stream writer for raw: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Audio pipeline and elements initialized successfully");
}

void ESPADFSpeaker::cleanup_audio_pipeline() {
    if (this->pipeline_ != nullptr) {
        ESP_LOGI(TAG, "Stopping current audio pipeline");
        audio_pipeline_stop(this->pipeline_);
        audio_pipeline_wait_for_stop(this->pipeline_);
        audio_pipeline_terminate(this->pipeline_);
        audio_pipeline_unregister(this->pipeline_, this->i2s_stream_writer_http_);
        audio_pipeline_unregister(this->pipeline_, this->http_filter_);
        audio_pipeline_unregister(this->pipeline_, this->http_stream_reader_);
        audio_pipeline_deinit(this->pipeline_);
        this->pipeline_ = nullptr;
    }
}

void ESPADFSpeaker::start() {
    this->state_ = speaker::STATE_STARTING;
}

void ESPADFSpeaker::stop() {
    if (this->state_ == speaker::STATE_STOPPED)
        return;
    if (this->state_ == speaker::STATE_STARTING) {
        this->cleanup_audio_pipeline();
        this->state_ = speaker::STATE_STOPPED;
        return;
    }
    this->state_ = speaker::STATE_STOPPING;
    DataEvent data;
    data.stop = true;
    xQueueSendToFront(this->buffer_queue_.handle, &data, portMAX_DELAY);
}

void ESPADFSpeaker::loop() {
    this->watch_();
    switch (this->state_) {
        case speaker::STATE_STARTING:
            this->start_();
            break;
        case speaker::STATE_RUNNING:
        case speaker::STATE_STOPPING:
        case speaker::STATE_STOPPED:
            break;
    }
}

size_t ESPADFSpeaker::play(const uint8_t *data, size_t length) {
    if (this->is_failed()) {
        ESP_LOGE(TAG, "Failed to play audio, speaker is in failed state.");
        return 0;
    }
    if (this->state_ != speaker::STATE_RUNNING && this->state_ != speaker::STATE_STARTING) {
        this->start();
    }
    size_t remaining = length;
    size_t index = 0;
    while (remaining > 0) {
        DataEvent event;
        event.stop = false;
        size_t to_send_length = std::min(remaining, BUFFER_SIZE);
        event.len = to_send_length;
        memcpy(event.data, data + index, to_send_length);
        if (xQueueSend(this->buffer_queue_.handle, &event, 0) != pdTRUE) {
            return index;
        }
        remaining -= to_send_length;
        index += to_send_length;
    }
    return index;
}

bool ESPADFSpeaker::has_buffered_data() const {
    return uxQueueMessagesWaiting(this->buffer_queue_.handle) > 0;
}

void ESPADFSpeaker::play_url(const std::string &url) {
    if (this->state_ == speaker::STATE_RUNNING || this->state_ == speaker::STATE_STARTING) {
        ESP_LOGI(TAG, "Audio stream is already running, ignoring play request");
        return;
    }
    ESP_LOGI(TAG, "Attempting to play URL: %s", url.c_str());

    this->cleanup_audio_pipeline();

    #ifdef HTTP_STREAM_RINGBUFFER_SIZE
    #undef HTTP_STREAM_RINGBUFFER_SIZE
    #endif
    #define HTTP_STREAM_RINGBUFFER_SIZE (12 * 1024)

    http_stream_cfg_t http_cfg = {
        .type = AUDIO_STREAM_READER,
        .out_rb_size = HTTP_STREAM_RINGBUFFER_SIZE,
        .task_stack = HTTP_STREAM_TASK_STACK,
        .task_core = HTTP_STREAM_TASK_CORE,
        .task_prio = HTTP_STREAM_TASK_PRIO,
        .stack_in_ext = false,
        .event_handle = NULL,
        .user_data = NULL,
        .auto_connect_next_track = false,
        .enable_playlist_parser = false,
        .multi_out_num = 0,
        .cert_pem = NULL,
        .crt_bundle_attach = NULL,
    };

    ESP_LOGI(TAG, "Passed Configure HTTP Stream");

    this->http_stream_reader_ = http_stream_init(&http_cfg);
    if (this->http_stream_reader_ == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP stream reader");
        return;
    }
    ESP_LOGI(TAG, "HTTP stream reader init");
    
    audio_element_set_uri(this->http_stream_reader_, url.c_str());
    ESP_LOGI(TAG, "HTTP set URI");

    ESP_LOGI(TAG, "Create MP3 decoder to decode MP3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    audio_element_handle_t mp3_decoder = mp3_decoder_init(&mp3_cfg);
    if (mp3_decoder == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MP3 decoder");
        return;
    }

    audio_pipeline_cfg_t pipeline_cfg = {
        .rb_size = 8 * 1024,
    };
    this->pipeline_ = audio_pipeline_init(&pipeline_cfg);
    if (this->pipeline_ == NULL) {
        ESP_LOGE(TAG, "Failed to initialize http audio pipeline");
        return;
    }
    ESP_LOGI(TAG, "HTTP passed initilialize new audio pipeline");

    ESP_LOGI(TAG, "Register all elements to audio pipeline");

    ESP_LOGI(TAG, "Register HTTP stream reader");
    if (audio_pipeline_register(this->pipeline_, this->http_stream_reader_, "http") != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register HTTP stream reader");
        audio_pipeline_deinit(this->pipeline_);
        this->pipeline_ = nullptr;
        return;
    }

    ESP_LOGI(TAG, "Register MP3 decoder");
    if (audio_pipeline_register(this->pipeline_, mp3_decoder, "mp3") != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MP3 decoder");
        audio_pipeline_deinit(this->pipeline_);
        this->pipeline_ = nullptr;
        return;
    }

    ESP_LOGI(TAG, "Register resample filter");
    if (audio_pipeline_register(this->pipeline_, this->http_filter_, "filter") != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register resample filter");
        audio_pipeline_deinit(this->pipeline_);
        this->pipeline_ = nullptr;
        return;
    }

    ESP_LOGI(TAG, "Register I2S stream writer");
    if (audio_pipeline_register(this->pipeline_, this->i2s_stream_writer_http_, "i2s") != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register I2S stream writer");
        audio_pipeline_deinit(this->pipeline_);
        this->pipeline_ = nullptr;
        return;
    }

    ESP_LOGI(TAG, "Link elements in pipeline");
    const char *link_tag[4] = {"http", "mp3", "filter", "i2s"};
    if (audio_pipeline_link(this->pipeline_, &link_tag[0], 4) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to link pipeline elements");
        audio_pipeline_deinit(this->pipeline_);
        this->pipeline_ = nullptr;
        return;
    }
    ESP_LOGI(TAG, "Linked pipeline elements");

    gpio_set_level(PA_ENABLE_GPIO, 1);
    ESP_LOGI(TAG, "PA enabled");

    if (this->state_ != speaker::STATE_RUNNING && this->state_ != speaker::STATE_STARTING) {
        ESP_LOGI(TAG, "State is Not Running");
        TaskEvent event;
        event.type = TaskEventType::STARTED;
        this->start();
    }
    if (this->state_ == speaker::STATE_RUNNING) {
        ESP_LOGI(TAG, "State is now Running after button");
    }
    if (this->state_ == speaker::STATE_STARTING) {
        ESP_LOGI(TAG, "State is starting after button");
    }

    ESP_LOGI(TAG, "Starting new audio pipeline for URL");
    if (audio_pipeline_run(this->pipeline_) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to run audio pipeline");
        audio_pipeline_deinit(this->pipeline_);
        this->pipeline_ = nullptr;
        return;
    }
}

void ESPADFSpeaker::media_play() {
    if (this->state_ == speaker::STATE_STOPPED) {
        audio_pipeline_resume(this->pipeline_);
        this->state_ = speaker::STATE_RUNNING;
    }
}

void ESPADFSpeaker::media_pause() {
    if (this->state_ == speaker::STATE_RUNNING) {
        audio_pipeline_pause(this->pipeline_);
        this->state_ = speaker::STATE_STOPPED;
    }
}

void ESPADFSpeaker::media_stop() {
    if (this->state_ != speaker::STATE_STOPPED) {
        this->cleanup_audio_pipeline();
        this->state_ = speaker::STATE_STOPPED;
    }
}

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
