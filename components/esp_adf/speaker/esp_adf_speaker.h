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

  void start() override;
  void stop() override;

  size_t play(const uint8_t *data, size_t length) override;

  bool has_buffered_data() const override;

  // Declare methods for volume control
  void set_volume(int volume);
  void volume_up();
  void volume_down();
  // Declare a method to get the current volume from the device
  int get_current_volume();

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
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
