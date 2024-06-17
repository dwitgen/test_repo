#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include "esp_heap_caps.h"
#include "esp_log.h"

namespace esphome {
namespace esp_adf {

inline void check_heap_memory(const char *tag) {
    ESP_LOGI(tag, "Free heap: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    ESP_LOGI(tag, "Minimum free heap: %d", heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
}

}  // namespace esp_adf
}  // namespace esphome

#endif  // MEMORY_UTILS_H
