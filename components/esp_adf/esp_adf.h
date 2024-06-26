#pragma once

#ifdef USE_ESP_IDF

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <periph_adc_button.h>
#include <esp_event.h>

namespace esphome {
namespace esp_adf {

static const size_t BUFFER_SIZE = 1024;

enum class TaskEventType : uint8_t {
  STARTING = 0,
  STARTED,
  RUNNING,
  STOPPING,
  STOPPED,
  WARNING = 255,
};

struct TaskEvent {
  TaskEventType type;
  esp_err_t err;
};

struct CommandEvent {
  bool stop;
};

struct DataEvent {
  bool stop;
  size_t len;
  uint8_t data[BUFFER_SIZE];
};

class ESPADF;

class ESPADFPipeline : public Parented<ESPADF> {};

class ESPADF : public Component {
 public:
  void setup() override;

  float get_setup_priority() const override;

  void lock() { this->lock_.lock(); }
  bool try_lock() { return this->lock_.try_lock(); }
  void unlock() { this->lock_.unlock(); }

 protected:
  Mutex lock_;
  static void button_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
  void handle_button_event(int32_t id);
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
