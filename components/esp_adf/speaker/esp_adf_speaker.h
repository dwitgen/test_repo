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

namespace esphome {
namespace esp_adf {

class ESPADFSpeaker : public ESPADFPipeline, public speaker::Speaker, public Component {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }

  void setup() override;
  void loop() override;

  void play_url(const std::string &url);
  void play_raw(const uint8_t *data, size_t length);
  void pause();
  void stop();
  void set_volume(int volume);
  int get_current_volume();
  void volume_up();
  void volume_down();
  size_t play(const uint8_t *data, size_t length); 

 private:
  static void player_task(void *params);
  void start();
  void start_();
  void stop_();
  void watch_();
  bool has_buffered_data() const;

  int volume_ = 50;  // Default volume level
  QueueHandle_t buffer_queue_;
  QueueHandle_t event_queue_;
  TaskHandle_t player_task_handle_ = nullptr;
  sensor::Sensor *volume_sensor = nullptr;

  // Add these member variables for the audio pipeline and elements
  audio_pipeline_handle_t pipeline_;
  audio_element_handle_t i2s_stream_writer_;
  audio_element_handle_t http_stream_reader_;
  audio_element_handle_t raw_stream_writer_;
  audio_element_handle_t filter_;
  audio_element_handle_t raw_write_;

  bool is_playing_url_ = false;
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
