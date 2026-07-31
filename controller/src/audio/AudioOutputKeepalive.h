#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "audio/AudioOutput.h"
#include "audio/audio-config.h"

namespace creatures::audio {

class AudioOutputKeepalive {
  public:
    AudioOutputKeepalive(AudioOutput &output, size_t targetFrames);

    bool start();
    bool refillSilence();

  private:
    AudioOutput &output_;
    const size_t targetFrames_;
    std::array<int16_t, FRAMES_PER_CHUNK> silence_{};
    bool started_{false};
};

} // namespace creatures::audio
