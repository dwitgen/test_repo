#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/components/api/custom_api_device.h"

namespace esphome {
namespace media_player {

class MediaPlayer : public EntityBase, public api::CustomAPIDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // Methods to control the media player
  void set_media_url(const std::string &media_url);
  void play_url(const std::string &url);
  void play();
  void stop();
  void pause();
  void volume_up();
  void volume_down();
  void set_volume(int volume);
  void set_mute(bool mute);
  void publish_state();

 protected:
  std::string media_url_;
  float volume_{1.0f};
  bool muted_{false};
};

}  // namespace media_player
}  // namespace esphome
