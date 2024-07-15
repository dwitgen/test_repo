#pragma once

#ifdef USE_ESP_IDF

#include "../esp_adf.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "esphome/components/speaker/speaker.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"

#include <audio_element.h>
#include <audio_pipeline.h>
#include <audio_hal.h>
#include "esp_peripherals.h"
#include "periph_adc_button.h"
#include "input_key_service.h"
#include <board.h>

#include "button/esp_adf_button.h"

#include <esp_event.h>  

namespace esphome {
namespace esp_adf {

class ESPADFSpeaker : public ESPADFPipeline, public speaker::Speaker, public Component {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }

  void setup() override;
  void loop() override;

  void start() override;
  void stop() override;

  size_t play(const uint8_t *data, size_t length) override;

  bool has_buffered_data() const override;

  // Declare methods for media/http streaming
  void play_url(const std::string &url); 
  void media_play();
  void media_pause();
  void media_stop();

  // Declare public getter for state
  speaker::State get_state() const { return this->state_; }

  // Getter and setter for volume
  int get_volume() const { return this->volume_; }
  void set_volume(int volume) { this->volume_ = volume; }

  // Declare a sensor for volume level
  sensor::Sensor *volume_sensor = nullptr;

 protected:
  void start_();
  void watch_();
 
  static void player_task(void *params);

  TaskHandle_t player_task_handle_{nullptr};
  struct {
    QueueHandle_t handle;
    uint8_t *storage;
  } buffer_queue_;
  QueueHandle_t event_queue_;
  
 private:
  int volume_ = 50;  // Default volume level
  bool is_http_stream_;
  audio_pipeline_handle_t pipeline_;
  audio_element_handle_t i2s_stream_writer_;
  audio_element_handle_t i2s_stream_writer_http_;
  audio_element_handle_t i2s_stream_writer_raw_;
  audio_element_handle_t filter_;
  audio_element_handle_t http_filter_;
  audio_element_handle_t raw_write_;
  audio_element_handle_t http_stream_reader_;
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF

