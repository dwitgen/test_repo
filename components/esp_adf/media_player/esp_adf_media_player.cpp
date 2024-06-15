#include "esp_adf_media_player.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esp_adf_media_player {

static const char *TAG = "esp_adf_media_player";

void ESPADFMediaPlayer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP ADF Media Player...");
  // Initialization code here
}

void ESPADFMediaPlayer::loop() {
  // Code to run in the main loop
}

media_player::MediaPlayerTraits ESPADFMediaPlayer::get_traits() {
  auto traits = media_player::MediaPlayerTraits();
  traits.set_supports_pause(true);
  traits.set_supports_stop(true);
  traits.set_supports_seek(true);
  traits.set_supports_volume_mute(true);
  return traits;
}

void ESPADFMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  if (call.get_source().has_value()) {
    this->play_media(call.get_source().value(), "");
  }
  if (call.get_command() == media_player::MEDIA_PLAYER_COMMAND_PLAY) {
    this->play();
  } else if (call.get_command() == media_player::MEDIA_PLAYER_COMMAND_PAUSE) {
    this->pause();
  } else if (call.get_command() == media_player::MEDIA_PLAYER_COMMAND_STOP) {
    this->stop();
  } else if (call.get_command() == media_player::MEDIA_PLAYER_COMMAND_MUTE) {
    this->mute(call.get_mute());
  } else if (call.get_command() == media_player::MEDIA_PLAYER_COMMAND_UNMUTE) {
    this->unmute();
  } else if (call.get_command() == media_player::MEDIA_PLAYER_COMMAND_SEEK) {
    this->seek(call.get_seek_position());
  } else if (call.get_command() == media_player::MEDIA_PLAYER_COMMAND_SET_VOLUME) {
    this->set_volume(call.get_volume_level());
  }
}

}  // namespace esp_adf_media_player
}  // namespace esphome
