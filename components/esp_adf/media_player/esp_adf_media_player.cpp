#include "esp_adf_media_player.h"

namespace esphome {
namespace esp_adf_media_player {

void ESPADFMediaPlayer::setup() {
  // Initialize the media player and get a reference to the speaker component
  this->speaker_ = new esphome::esp_adf_speaker::ESPADFSpeaker();
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
  if (call.get_volume().has_value()) {
    float volume = *call.get_volume();
    this->speaker_->set_volume(volume);
  }

  if (call.get_media_url().has_value()) {
    std::string url = *call.get_media_url();
    this->speaker_->play(url);
  }

  if (call.get_command().has_value()) {
    MediaPlayerCallCommand cmd = *call.get_command();
    if (cmd == MediaPlayerCallCommand::MEDIA_PLAYER_CALL_COMMAND_PLAY) {
      this->speaker_->play("");
    } else if (cmd == MediaPlayerCallCommand::MEDIA_PLAYER_CALL_COMMAND_PAUSE) {
      this->speaker_->pause();
    } else if (cmd == MediaPlayerCallCommand::MEDIA_PLAYER_CALL_COMMAND_STOP) {
      this->speaker_->stop();
    }
  }
}

}  // namespace esp_adf_media_player
}  // namespace esphome
