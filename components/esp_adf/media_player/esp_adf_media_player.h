#ifndef ESP_ADF_MEDIA_PLAYER_H
#define ESP_ADF_MEDIA_PLAYER_H

#include "esphome.h"

namespace esphome {
namespace esp_adf_media_player {

class ESPADFMediaPlayer : public Component, public media_player::MediaPlayer {
 public:
  void setup() override;
  void loop() override;
  media_player::MediaPlayerTraits get_traits() override;
  void control(const media_player::MediaPlayerCall &call) override;
};

}  // namespace esp_adf_media_player
}  // namespace esphome

#endif  // ESP_ADF_MEDIA_PLAYER_H

