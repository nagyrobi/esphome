#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/speaker/speaker.h"

#include "AudioSink.h"

namespace esphome {
namespace cspot {

static constexpr size_t CSPOT_MAX_BUFFER_SIZE = 64 * 1024;
static constexpr size_t CSPOT_WRITE_CHUNK_SIZE = 4 * 1024;

class ESPHomeAudioSink : public ::AudioSink {
 public:
  explicit ESPHomeAudioSink(speaker::Speaker *speaker, audio_dac::AudioDac *audio_dac = nullptr);

  void feedPCMFrames(std::vector<uint8_t> &data);
  void feedPCMFrames(const uint8_t *data, size_t length);
  void volumeChanged(uint16_t volume);

  void loop();
  void stop();
  size_t buffered() const;

 protected:
  speaker::Speaker *speaker_;
  audio_dac::AudioDac *audio_dac_;

  mutable std::mutex mutex_;
  std::deque<uint8_t> queue_;
  bool started_{false};
};

class CSpotComponent : public Component {
 public:
  void set_sink(speaker::Speaker *sink) { this->speaker_sink_ = sink; }
  void set_sink(audio_dac::AudioDac *sink) { this->audio_dac_sink_ = sink; }

  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  speaker::Speaker *speaker_sink_{nullptr};
  audio_dac::AudioDac *audio_dac_sink_{nullptr};

  std::unique_ptr<ESPHomeAudioSink> sink_impl_;
};

}  // namespace cspot
}  // namespace esphome
