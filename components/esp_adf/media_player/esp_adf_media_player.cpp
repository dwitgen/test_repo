#include "esp_adf_media_player.h"

namespace esphome {
namespace esp_adf_media_player {

void ESPADFMediaPlayer::setup() {
  this->speaker_ = new esphome::esp_adf::ESPADFSpeaker();
  this->speaker_->setup();
}

void ESPADFMediaPlayer::loop() {
  this->speaker_->loop();
}

MediaPlayerTraits ESPADFMediaPlayer::get_traits() {
  auto traits = MediaPlayerTraits();
  traits.set_supports_pause(true);
  traits.set_supports_seek(true);
  traits.set_supports_volume_set(true);
  return traits;
}

void ESPADFMediaPlayer::control(const MediaPlayerCall &call) {
  if (call.get_volume().has_value
