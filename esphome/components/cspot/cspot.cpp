#include "cspot.h"

namespace esphome {
namespace cspot {

static const char *const TAG = "cspot";

ESPHomeAudioSink::ESPHomeAudioSink(speaker::Speaker *speaker, audio_dac::AudioDac *audio_dac)
    : speaker_(speaker), audio_dac_(audio_dac) {
  this->softwareVolumeControl = audio_dac == nullptr;
}

void ESPHomeAudioSink::feedPCMFrames(std::vector<uint8_t> &data) { this->feedPCMFrames(data.data(), data.size()); }

void ESPHomeAudioSink::feedPCMFrames(const uint8_t *data, size_t length) {
  if (data == nullptr || length == 0 || this->speaker_ == nullptr) {
    return;
  }

  std::lock_guard<std::mutex> lock(this->mutex_);

  while (this->queue_.size() + length > CSPOT_MAX_BUFFER_SIZE && !this->queue_.empty()) {
    this->queue_.pop_front();
  }

  this->queue_.insert(this->queue_.end(), data, data + length);
}

void ESPHomeAudioSink::volumeChanged(uint16_t volume) {
  const float normalized = clamp(static_cast<float>(volume) / 0xFFFF, 0.0f, 1.0f);

  if (this->audio_dac_ != nullptr) {
    this->audio_dac_->set_volume(normalized);
  }

  if (this->speaker_ != nullptr) {
    this->speaker_->set_volume(normalized);
  }
}

void ESPHomeAudioSink::loop() {
  if (this->speaker_ == nullptr) {
    return;
  }

  std::vector<uint8_t> out;
  {
    std::lock_guard<std::mutex> lock(this->mutex_);
    const size_t to_write = std::min<size_t>(this->queue_.size(), CSPOT_WRITE_CHUNK_SIZE);

    if (to_write == 0) {
      return;
    }

    out.reserve(to_write);
    for (size_t i = 0; i < to_write; i++) {
      out.push_back(this->queue_.front());
      this->queue_.pop_front();
    }
  }

  if (!this->started_) {
    this->speaker_->start();
    this->started_ = true;
  }

  const size_t written = this->speaker_->play(out.data(), out.size());
  if (written < out.size()) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    this->queue_.insert(this->queue_.begin(), out.begin() + written, out.end());
  }
}

void ESPHomeAudioSink::stop() {
  if (this->speaker_ != nullptr && this->started_) {
    this->speaker_->stop();
  }
  this->started_ = false;
  std::lock_guard<std::mutex> lock(this->mutex_);
  this->queue_.clear();
}

size_t ESPHomeAudioSink::buffered() const {
  std::lock_guard<std::mutex> lock(this->mutex_);
  return this->queue_.size();
}

void CSpotComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up cspot...");

  if (this->speaker_sink_ == nullptr) {
    ESP_LOGE(TAG, "A speaker sink is required for cspot playback.");
    this->mark_failed();
    return;
  }

  this->speaker_sink_->set_audio_stream_info(audio::AudioStreamInfo(44100, 2, 16));
  this->sink_impl_ = std::make_unique<ESPHomeAudioSink>(this->speaker_sink_, this->audio_dac_sink_);
}

void CSpotComponent::loop() {
  if (this->sink_impl_ != nullptr) {
    this->sink_impl_->loop();
  }
}

void CSpotComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "cspot:");
  LOG_COMPONENT("  ", "", this);

  ESP_LOGCONFIG(TAG, "  Sink type: %s", this->speaker_sink_ != nullptr ? "speaker" : "audio_dac");
  ESP_LOGCONFIG(TAG, "  Buffer: %u bytes", static_cast<unsigned int>(this->sink_impl_ != nullptr ? this->sink_impl_->buffered() : 0));
}

}  // namespace cspot
}  // namespace esphome
