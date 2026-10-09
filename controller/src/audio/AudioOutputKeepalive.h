#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "audio/AudioOutput.h"
#include "audio/audio-config.h"

namespace creatures::audio {

class AudioOutputKeepalive {
  public:
    AudioOutputKeepalive(AudioOutput &output, size_t targetFrames, size_t channels = 1);

    bool start();
    bool refillSilence();

  private:
    AudioOutput &output_;
    const size_t targetFrames_;
    const size_t channels_;
    std::vector<int16_t> silence_;
    bool started_{false};
};

} // namespace creatures::audio
