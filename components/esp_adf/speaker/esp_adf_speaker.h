#ifndef ESP_ADF_SPEAKER_H
#define ESP_ADF_SPEAKER_H

#include "../esp_adf.h"

#ifdef USE_ESP_IDF

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

namespace esphome {
namespace esp_adf {

class ButtonHandler;

class ESPADFSpeaker : public ESPADFPipeline, public speaker::Speaker, public Component {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }

  void setup() override;
  void loop() override;

  void start() override;
  void stop() override;

  size_t play(const uint8_t *data, size_t length) override;

  bool has_buffered_data() const override;

  // Declare a sensor for volume level
  sensor::Sensor *volume_sensor = nullptr;

  // Method to initialize pipeline and cleanup
  void initialize_audio_pipeline();
  void cleanup_audio_pipeline();

  // Declare methods for media/http streaming
  void play_url(const std::string &url); 
  void media_play();
  void media_pause();
  void media_stop();

  friend class ButtonHandler;

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

#endif // ESP_ADF_SPEAKER_H
