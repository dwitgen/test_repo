#ifndef ESP_ADF_MEDIA_PLAYER_H
#define ESP_ADF_MEDIA_PLAYER_H

#include "esphome.h"
#include "esp_adf_speaker.h"

namespace esphome {
namespace esp_adf_media_player {

class ESPADFMediaPlayer : public Component, public MediaPlayer {
 public:
  void setup() override;
  void loop() override;

  MediaPlayerTraits get_traits() override;
  void control(const MediaPlayerCall &call) override;

 private:
  esphome::esp_adf_speaker::ESPADFSpeaker *speaker_;
};

}  // namespace esp_adf_media_player
}  // namespace esphome

#endif  // ESP_ADF_MEDIA_PLAYER_H
