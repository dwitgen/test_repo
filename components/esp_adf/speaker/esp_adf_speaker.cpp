#include "esp_adf_speaker.h"

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

#include "periph_adc_button.h"
#include "esp_peripherals.h"
#include "periph_button.h"

//#include "esp_heap_caps.h"


#ifdef USE_ESP_ADF_BOARD
#include <board.h>
#endif

namespace esphome {
namespace esp_adf {

static const size_t BUFFER_COUNT = 50;
static const char *const TAG = "esp_adf.speaker";

// Define ADC configuration
#define ADC_WIDTH_BIT    ADC_WIDTH_BIT_12
#define ADC_ATTEN        ADC_ATTEN_DB_12
#ifndef ESP_EVENT_ANY_ID
#define ESP_EVENT_ANY_ID -1
#endif

// Usewd for testing and can be removed if desired.
/*void check_heap_memory(const char* message) {
    size_t free_heap_size = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    ESP_LOGI("Heap Memory Check", "%s - Free heap size: %d bytes", message, free_heap_size);
}*/

void ESPADFSpeaker::set_volume(int volume) {
    ESP_LOGI(TAG, "Setting volume to %d", volume);
    
    // Ensure the volume is within the range 0-100
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    this->volume_ = volume;

    // Set volume using HAL
    
    audio_board_handle_t board_handle = audio_board_init();
    esp_err_t err = audio_hal_set_volume(board_handle->audio_hal, volume);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting volume: %s", esp_err_to_name(err));
    }

    // Update the volume sensor
    if (this->volume_sensor != nullptr) {
      this->volume_sensor->publish_state(this->volume_);
    } else {
      ESP_LOGE(TAG, "Volume sensor is not initialized");
    }
}

int ESPADFSpeaker::get_current_volume() {
  audio_board_handle_t board_handle = audio_board_init();
  if (board_handle == nullptr) {
    ESP_LOGE(TAG, "Failed to initialize audio board");
    return 0;
  }

  int current_volume = 0;
  esp_err_t read_err = audio_hal_get_volume(board_handle->audio_hal, &current_volume);
  if (read_err == ESP_OK) {
    ESP_LOGI(TAG, "Current device volume: %d", current_volume);
  } else {
    ESP_LOGE(TAG, "Error reading current volume: %s", esp_err_to_name(read_err));
  }

  return current_volume;
}
void ESPADFSpeaker::volume_up() {
    ESP_LOGI(TAG, "Volume up button pressed");
    int current_volume = this->get_current_volume();
    this->set_volume(current_volume + 10);
}

void ESPADFSpeaker::volume_down() {
    ESP_LOGI(TAG, "Volume down button pressed");
    int current_volume = this->get_current_volume();
    this->set_volume(current_volume - 10);
}

void ESPADFSpeaker::handle_mode_button() {
  //#define curren_url_ "http://streaming.tdiradio.com:8000/house.mp3"
	if (this->state_ != speaker::STATE_RUNNING && this->state_ != speaker::STATE_STARTING) {
		//this->is_http_stream_ = true;
		ESP_LOGI(TAG, "Mode button, speaker stopped");
		this->play_url("http://streaming.tdiradio.com:8000/house.mp3");
	} else {
		ESP_LOGI(TAG, "State is stopping");
		this->cleanup_audio_pipeline();
		this->stop();
			
	} 
}

void ESPADFSpeaker::button_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data) {
    uint32_t current_time = millis();
    static uint32_t last_button_press[6] = {0};  // Array to store the last press time for each button
    uint32_t debounce_time = 200;  // Default debounce time in milliseconds

    if (id == BUTTON_MODE_ID) {
        debounce_time = 500;  // Custom debounce time for mode button
    }

    if (current_time - last_button_press[id] > debounce_time) {
        switch (id) {
            case BUTTON_VOLUP_ID:
                ESP_LOGI(TAG, "Volume up detected");
                volume_up();
                break;
            case BUTTON_VOLDOWN_ID:
                ESP_LOGI(TAG, "Volume down detected");
                volume_down();
                break;
            /*case BUTTON_SET_ID:
                ESP_LOGI(TAG, "Set button detected");
                handle_set_button();
                break;
            case BUTTON_PLAY_ID:
                ESP_LOGI(TAG, "Play button detected");
                handle_play_button();
                break;*/
            case BUTTON_MODE_ID:
                ESP_LOGI(TAG, "Mode button detected");
                handle_mode_button();
                break;
            /*case BUTTON_REC_ID:
                ESP_LOGI(TAG, "Record button detected");
                handle_rec_button();
                break;*/
            default:
                ESP_LOGW(TAG, "Unhandled button event id: %d", id);
                break;
        }
        last_button_press[id] = current_time;
    }
}
void ESPADFSpeaker::initialize_audio_pipeline() {
    esp_err_t ret;
    
    // Initial memory check
    //check_heap_memory("Before initializing resample filter");
    
    // Initialize resample filter for HTTP stream
    ret = configure_resample_filter(&this->http_filter_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing resample filter: %s", esp_err_to_name(ret));
        return;
    }
    // Check memory after initializing resample filter
    //check_heap_memory("After initializing resample filter");

    // Initialize I2S stream writer for HTTP
    ret = configure_i2s_stream_writer_http(&this->i2s_stream_writer_http_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing I2S stream writer for HTTP: %s", esp_err_to_name(ret));
        return;
    }
     // Check memory after initializing I2S stream writer for HTTP
    //check_heap_memory("After initializing I2S stream writer for HTTP");
    
    // Initialize I2S stream writer for raw (if needed)
    ret = configure_i2s_stream_writer_raw(&this->i2s_stream_writer_raw_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing I2S stream writer for raw: %s", esp_err_to_name(ret));
        return;
    }

    // Check memory after initializing I2S stream writer for raw
    //check_heap_memory("After initializing I2S stream writer for raw");

    ESP_LOGI(TAG, "Audio pipeline and elements initialized successfully");
}

void ESPADFSpeaker::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP ADF Speaker...");

  #ifdef USE_ESP_ADF_BOARD
  // Use the PA enable pin from board configuration
  gpio_num_t pa_enable_gpio = static_cast<gpio_num_t>(get_pa_enable_gpio());
  int but_channel = INPUT_BUTOP_ID;
  #endif

	/*// Initialize peripherals and buttons
	esp_periph_set_handle_t set = esp_periph_set_init(NULL);
	ESP_ERROR_CHECK(esp_periph_start(set, adc_btn_handle));
	esp_event_handler_register(PERIPH_ID_ADC_BTN, ESP_EVENT_ANY_ID, button_event_handler, NULL);
	ESP_ERROR_CHECK(esp_periph_start(set));*/

	// Initialize peripherals
    esp_periph_set_handle_t set = esp_periph_set_init(NULL);

    // Initialize buttons
    esp_periph_handle_t adc_btn_handle;
    ESP_ERROR_CHECK(audio_board_key_init(set, &adc_btn_handle)); // Ensure this function initializes adc_btn_handle correctly

    // Register the button event handler
    ESP_ERROR_CHECK(esp_event_handler_register(PERIPH_ID_ADC_BTN, ESP_EVENT_ANY_ID, &ESPADFSpeaker::button_event_handler, this));

    // Start the peripheral set
    ESP_ERROR_CHECK(esp_periph_start(set, adc_btn_handle));
	
  gpio_config_t io_conf;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = (1ULL << PA_ENABLE_GPIO);
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&io_conf);
  gpio_set_level(PA_ENABLE_GPIO, 0);  // Ensure PA is off initially

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

 // Find the key for the generic volume sensor
  uint32_t volume_sensor_key = 0;
  for (auto *sensor : App.get_sensors()) {
    if (sensor->get_name() == "generic_volume_sensor") {
      volume_sensor_key = sensor->get_object_id_hash();
      break;
    }
  }

  // Use the key to get the sensor
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
  // Set initial volume
  this->set_volume(volume_); // Set initial volume to 50%

   // Read and set initial volume
  int initial_volume = this->get_current_volume();
  this->set_volume(initial_volume);
  
  // Configure ADC for buttons
  adc1_config_width(ADC_WIDTH_BIT);
  adc1_config_channel_atten((adc1_channel_t)but_channel, ADC_ATTEN);

  // Initialize audio pipeline
  this->initialize_audio_pipeline();

}


/*void ESPADFSpeaker::handle_mode_button() {
    // Switch to HTTP stream mode and play the test stream
    this->is_http_stream_ = true;
    this->play_url("http://streaming.tdiradio.com:8000/house.mp3");
}*/

void ESPADFSpeaker::play_url(const std::string &url) {

		 if (this->state_ == speaker::STATE_RUNNING || this->state_ == speaker::STATE_STARTING ) {
        ESP_LOGI(TAG, "Audio stream is already running, ignoring play request");
        return;
    }
    ESP_LOGI(TAG, "Attempting to play URL: %s", url.c_str());
    // Ensure the pipeline is stopped if already running
    // Cleanup the previous pipeline
    this->cleanup_audio_pipeline();

    // Change HTTP Ring Buffer
    // First, undefine the previous definition if it exists
    #ifdef HTTP_STREAM_RINGBUFFER_SIZE
    #undef HTTP_STREAM_RINGBUFFER_SIZE
    #endif
    
    // Redefine it to the new value
    #define HTTP_STREAM_RINGBUFFER_SIZE (12 * 1024)
     // Configure HTTP stream
    http_stream_cfg_t http_cfg = {
        .type = AUDIO_STREAM_READER,
        .out_rb_size = HTTP_STREAM_RINGBUFFER_SIZE,
        .task_stack = HTTP_STREAM_TASK_STACK,
        .task_core = HTTP_STREAM_TASK_CORE,
        .task_prio = HTTP_STREAM_TASK_PRIO,
        .stack_in_ext = false,
        .event_handle = NULL, /* [](http_stream_event_msg_t *msg) -> int {
            ESP_LOGI(TAG, "HTTP event received: event_id=%d, buffer_len=%d", msg->event_id, msg->buffer_len);
            if (msg->event_id == HTTP_STREAM_ON_RESPONSE) {
                audio_element_info_t music_info = {0};
                audio_element_getinfo((audio_element_handle_t)msg->el, &music_info);
                ESP_LOGI(TAG, "Received music info from HTTP stream: sample_rate=%d, bits=%d, channels=%d",
                    music_info.sample_rates, music_info.bits, music_info.channels);
            }
            return ESP_OK;
        },*/
        .user_data = NULL,
        .auto_connect_next_track = false,
        .enable_playlist_parser = false,
        .multi_out_num = 0,
        .cert_pem = NULL,
        .crt_bundle_attach = NULL,
        
    };

    ESP_LOGI(TAG, "Passed Configure HTTP Stream"); // Remove when done

    this->http_stream_reader_ = http_stream_init(&http_cfg);
    if (this->http_stream_reader_ == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP stream reader");
        return;
    }
    ESP_LOGI(TAG, "HTTP stream reader init"); // Remove when done
    
    audio_element_set_uri(this->http_stream_reader_, url.c_str());
    ESP_LOGI(TAG, "HTTP set URI"); // Remove when done

    // Initialize MP3 Decoder
    ESP_LOGI(TAG, "Create MP3 decoder to decode MP3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    audio_element_handle_t mp3_decoder = mp3_decoder_init(&mp3_cfg);
    if (mp3_decoder == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MP3 decoder");
        return;
    }

    
    // Initialize a new audio pipeline for the URL stream
    audio_pipeline_cfg_t pipeline_cfg = {
        .rb_size = 8 * 1024,
    };
    this->pipeline_ = audio_pipeline_init(&pipeline_cfg);
    if (this->pipeline_ == NULL) {
        ESP_LOGE(TAG, "Failed to initialize http audio pipeline");
        return;
    }
    ESP_LOGI(TAG, "HTTP passed initilialize new audio pipeline"); // Remove when done

     
    // Register the pipeline elements
    ESP_LOGI(TAG, "Register all elements to audio pipeline"); // Remove when done

    ESP_LOGI(TAG, "Register HTTP stream reader"); // Remove when done
    if (audio_pipeline_register(this->pipeline_, this->http_stream_reader_, "http") != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register HTTP stream reader");
        audio_pipeline_deinit(this->pipeline_);
        this->pipeline_ = nullptr;
        return;
    }

    ESP_LOGI(TAG, "Register MP3 decoder"); // Remove when done
    if (audio_pipeline_register(this->pipeline_, mp3_decoder, "mp3") != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MP3 decoder");
        audio_pipeline_deinit(this->pipeline_);
        this->pipeline_ = nullptr;
        return;
    }

    ESP_LOGI(TAG, "Register resample filter"); // Remove when done
    if (audio_pipeline_register(this->pipeline_, this->http_filter_, "filter") != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register resample filter");
        audio_pipeline_deinit(this->pipeline_);
        this->pipeline_ = nullptr;
        return;
    }

    ESP_LOGI(TAG, "Register I2S stream writer"); // Remove when done
    if (audio_pipeline_register(this->pipeline_, this->i2s_stream_writer_http_, "i2s") != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register I2S stream writer");
        audio_pipeline_deinit(this->pipeline_);
        this->pipeline_ = nullptr;
        return;
    }

    
    // Link the pipeline elements
    ESP_LOGI(TAG, "Link elements in pipeline"); // Remove when done
    const char *link_tag[4] = {"http", "mp3", "filter", "i2s"};
    if (audio_pipeline_link(this->pipeline_, &link_tag[0], 4) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to link pipeline elements");
        audio_pipeline_deinit(this->pipeline_);
        this->pipeline_ = nullptr;
        return;
    }
    ESP_LOGI(TAG, "Linked pipeline elements"); // Remove when done

     // Enable the PA
    gpio_set_level(PA_ENABLE_GPIO, 1);  // Enable PA
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
		
    // Start the audio pipeline
    ESP_LOGI(TAG, "Starting new audio pipeline for URL"); 
    if (audio_pipeline_run(this->pipeline_) != ESP_OK) {
	  	  ESP_LOGE(TAG, "Failed to run audio pipeline");
        audio_pipeline_deinit(this->pipeline_);
        this->pipeline_ = nullptr;
        return;
    }
}

void ESPADFSpeaker::media_play() {
  if (this->state_ == speaker::STATE_STOPPED ) { /* || this->state_ == speaker::STATE_PAUSED) { */
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

void ESPADFSpeaker::start() { this->state_ = speaker::STATE_STARTING; }
void ESPADFSpeaker::start_() {
  if (!this->parent_->try_lock()) {
    return;  // Waiting for another i2s component to return lock
  }
  //gpio_set_level(PA_ENABLE_GPIO, 1);  // Enable PA
  xTaskCreate(ESPADFSpeaker::player_task, "speaker_task", 8192, (void *) this, 0, &this->player_task_handle_);
}

void ESPADFSpeaker::player_task(void *params) {
  ESPADFSpeaker *this_speaker = (ESPADFSpeaker *) params;

  TaskEvent event;
  event.type = TaskEventType::STARTING;
  xQueueSend(this_speaker->event_queue_, &event, portMAX_DELAY);
  
  // Configure and initialize i2s
  i2s_driver_config_t i2s_config = {
      .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 16000,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM,
      .dma_buf_count = 8,
      .dma_buf_len = 1024,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0,
      .mclk_multiple = I2S_MCLK_MULTIPLE_256,
      .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
  };

  audio_pipeline_cfg_t pipeline_cfg = {
      .rb_size = 8 * 1024,
  };
  audio_pipeline_handle_t pipeline = audio_pipeline_init(&pipeline_cfg);

  i2s_stream_cfg_t i2s_cfg = {
      .type = AUDIO_STREAM_WRITER,
      .i2s_config = i2s_config,
      .i2s_port = I2S_NUM_0,
      .use_alc = false,
      .volume = 0,
      .out_rb_size = I2S_STREAM_RINGBUFFER_SIZE,
      .task_stack = I2S_STREAM_TASK_STACK,
      .task_core = I2S_STREAM_TASK_CORE,
      .task_prio = I2S_STREAM_TASK_PRIO,
      .stack_in_ext = false,
      .multi_out_num = 0,
      .uninstall_drv = true,
      .need_expand = false,
      .expand_src_bits = I2S_BITS_PER_SAMPLE_16BIT,
  };
  this_speaker->i2s_stream_writer_ = i2s_stream_init(&i2s_cfg);
  if (this_speaker->i2s_stream_writer_ == nullptr) {
        ESP_LOGE("ESPADFSpeaker", "Failed to initialize I2S stream writer");
        event.type = TaskEventType::WARNING;
        xQueueSend(this_speaker->event_queue_, &event, portMAX_DELAY);
        return;
    }


  // Determine if HTTP stream or raw stream
    if (this_speaker->is_http_stream_) {
        // HTTP Stream Configuration
        http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
        http_cfg.type = AUDIO_STREAM_READER;
        this_speaker->http_stream_reader_ = http_stream_init(&http_cfg);
        
        if (this_speaker->http_stream_reader_ == nullptr) {
            ESP_LOGE("ESPADFSpeaker", "Failed to initialize HTTP stream reader");
            event.type = TaskEventType::WARNING;
            xQueueSend(this_speaker->event_queue_, &event, portMAX_DELAY);
            return;
        }
        

        rsp_filter_cfg_t rsp_cfg = {
            .src_rate = 44100,
            .src_ch = 2,
            .dest_rate = 16000,
            .dest_bits = 16,
            .dest_ch = 1,
            .src_bits = 16,
            .mode = RESAMPLE_DECODE_MODE,
            .max_indata_bytes = RSP_FILTER_BUFFER_BYTE,
            .out_len_bytes = RSP_FILTER_BUFFER_BYTE,
            .type = ESP_RESAMPLE_TYPE_AUTO,
            .complexity = 2,
            .down_ch_idx = 0,
            .prefer_flag = ESP_RSP_PREFER_TYPE_SPEED,
            .out_rb_size = RSP_FILTER_RINGBUFFER_SIZE,
            .task_stack = RSP_FILTER_TASK_STACK,
            .task_core = RSP_FILTER_TASK_CORE,
            .task_prio = RSP_FILTER_TASK_PRIO,
            .stack_in_ext = true,
        };
        this_speaker->filter_ = rsp_filter_init(&rsp_cfg);

        if (this_speaker->filter_ == nullptr) {
            ESP_LOGE("ESPADFSpeaker", "Failed to initialize resample filter");
            event.type = TaskEventType::WARNING;
            xQueueSend(this_speaker->event_queue_, &event, portMAX_DELAY);
            return;
        }
      
        this_speaker->pipeline_ = audio_pipeline_init(&pipeline_cfg);
        audio_pipeline_register(this_speaker->pipeline_, this_speaker->http_stream_reader_, "http");
        audio_pipeline_register(this_speaker->pipeline_, this_speaker->filter_, "filter");
        audio_pipeline_register(this_speaker->pipeline_, this_speaker->i2s_stream_writer_, "i2s");

        const char *link_tag_http[3] = {"http", "filter", "i2s"};
        audio_pipeline_link(this_speaker->pipeline_, &link_tag_http[0], 3);

        audio_pipeline_run(this_speaker->pipeline_);
    } else {
        // Raw Stream Configuration
        raw_stream_cfg_t raw_cfg = {
            .type = AUDIO_STREAM_WRITER,
            .out_rb_size = 8 * 1024,
        };
        this_speaker->raw_write_ = raw_stream_init(&raw_cfg);

        if (this_speaker->raw_write_ == nullptr) {
            ESP_LOGE("ESPADFSpeaker", "Failed to initialize raw stream writer");
            event.type = TaskEventType::WARNING;
            xQueueSend(this_speaker->event_queue_, &event, portMAX_DELAY);
            return;
        }

        this_speaker->pipeline_ = audio_pipeline_init(&pipeline_cfg);
        audio_pipeline_register(this_speaker->pipeline_, this_speaker->raw_write_, "raw");
        audio_pipeline_register(this_speaker->pipeline_, this_speaker->i2s_stream_writer_, "i2s");

        const char *link_tag_raw[2] = {"raw", "i2s"};
        audio_pipeline_link(this_speaker->pipeline_, &link_tag_raw[0], 2);

        audio_pipeline_run(this_speaker->pipeline_);
    }
  DataEvent data_event;

  event.type = TaskEventType::STARTED;
  xQueueSend(this_speaker->event_queue_, &event, 0);
  gpio_set_level(PA_ENABLE_GPIO, 1);  // Enable PA

  uint32_t last_received = millis();

  while (true) {
        if (xQueueReceive(this_speaker->buffer_queue_.handle, &data_event, 0) != pdTRUE) {
            if (millis() - last_received > 500) {
                // No audio for 500ms, stop
                break;
            } else {
                continue;
            }
        }
        if (data_event.stop) {
            // Stop signal from main thread
            while (xQueueReceive(this_speaker->buffer_queue_.handle, &data_event, 0) == pdTRUE) {
                // Flush queue
            }
            break;
        }

        size_t remaining = data_event.len;
        size_t current = 0;
        if (remaining > 0)
            last_received = millis();

        while (remaining > 0) {
            int bytes_written = raw_stream_write(this_speaker->raw_write_, (char *) data_event.data + current, remaining);
            if (bytes_written == ESP_FAIL) {
                event = {.type = TaskEventType::WARNING, .err = ESP_FAIL};
                xQueueSend(this_speaker->event_queue_, &event, 0);
                continue;
            }

            remaining -= bytes_written;
            current += bytes_written;
        }

        event.type = TaskEventType::RUNNING;
        xQueueSend(this_speaker->event_queue_, &event, 0);
    }

    audio_pipeline_stop(this_speaker->pipeline_);
    audio_pipeline_wait_for_stop(this_speaker->pipeline_);
    audio_pipeline_terminate(this_speaker->pipeline_);

    event.type = TaskEventType::STOPPING;
    xQueueSend(this_speaker->event_queue_, &event, portMAX_DELAY);

    audio_pipeline_unregister(this_speaker->pipeline_, this_speaker->i2s_stream_writer_);
    if (this_speaker->is_http_stream_) {
        audio_pipeline_unregister(this_speaker->pipeline_, this_speaker->filter_);
        audio_pipeline_unregister(this_speaker->pipeline_, this_speaker->http_stream_reader_);
    } else {
        audio_pipeline_unregister(this_speaker->pipeline_, this_speaker->raw_write_);
    }

    audio_pipeline_deinit(this_speaker->pipeline_);
    audio_element_deinit(this_speaker->i2s_stream_writer_);
    if (this_speaker->is_http_stream_) {
        audio_element_deinit(this_speaker->filter_);
        audio_element_deinit(this_speaker->http_stream_reader_);
    } else {
        audio_element_deinit(this_speaker->raw_write_);
    }

  event.type = TaskEventType::STOPPED;
  xQueueSend(this_speaker->event_queue_, &event, portMAX_DELAY);
  gpio_set_level(PA_ENABLE_GPIO, 0);  // Disable PA

  while (true) {
    delay(10);
  }
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
  //gpio_set_level(PA_ENABLE_GPIO, 0);  // Disable PA
  xQueueSendToFront(this->buffer_queue_.handle, &data, portMAX_DELAY);
}

void ESPADFSpeaker::watch_() {
  TaskEvent event;
  if (xQueueReceive(this->event_queue_, &event, 0) == pdTRUE) {
    switch (event.type) {
      case TaskEventType::STARTING:
      case TaskEventType::STOPPING:
        break;
      case TaskEventType::STARTED:
        this->state_ = speaker::STATE_RUNNING;
        break;
      case TaskEventType::RUNNING:
        this->status_clear_warning();
        break;
      case TaskEventType::STOPPED:
        this->parent_->unlock();
        this->state_ = speaker::STATE_STOPPED;
        vTaskDelete(this->player_task_handle_);
        this->player_task_handle_ = nullptr;
        break;
      case TaskEventType::WARNING:
        ESP_LOGW(TAG, "Error writing to pipeline: %s", esp_err_to_name(event.err));
        this->status_set_warning();
        break;
    }
  }
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
   // Read ADC value for button control
    int adc_value = adc1_get_raw((adc1_channel_t)INPUT_BUTOP_ID);
    if (adc_value < 0) {
        ESP_LOGE(TAG, "ADC read error");
        return;
    }

    /*//ESP_LOGD(TAG, "ADC value: %d", adc_value);
    
    static uint32_t last_mode_button_press = 0;
    static uint32_t last_vol_up_button_press = 0;
    static uint32_t last_vol_down_button_press = 0;
    uint32_t current_time = millis();

    // Determine button press based on ADC value
    if (adc_value >= VOL_UP_THRESHOLD_LOW && adc_value <= VOL_UP_THRESHOLD_HIGH) {
        if (current_time - last_vol_up_button_press > 200) {  // 200ms debounce time for volume up
            ESP_LOGI(TAG, "Volume up detected");
            this->volume_up();
            last_vol_up_button_press = current_time;
        }
    } else if (adc_value >= VOL_DOWN_THRESHOLD_LOW && adc_value <= VOL_DOWN_THRESHOLD_HIGH) {
        if (current_time - last_vol_down_button_press > 200) {  // 200ms debounce time for volume down
            ESP_LOGI(TAG, "Volume down detected");
            this->volume_down();
            last_vol_down_button_press = current_time;
        }
    } else if (adc_value >= MODE_THRESHOLD_LOW && adc_value <= MODE_THRESHOLD_HIGH) {
        if (current_time - last_mode_button_press > 500) {  // 500ms debounce time for mode button
            ESP_LOGI(TAG, "Mode button detected");
            this->handle_mode_button();
            last_mode_button_press = current_time;
        }
    }*/
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
      return index;  // Queue full
    }
    remaining -= to_send_length;
    index += to_send_length;
  }
  return index;
}

bool ESPADFSpeaker::has_buffered_data() const { return uxQueueMessagesWaiting(this->buffer_queue_.handle) > 0; }

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF

