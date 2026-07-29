#pragma once

#include <memory>

#include "audio/audio-config.h"
#include "logging/Logger.h"

namespace creatures::audio {

class AlsaMixerControl {
  public:
    [[nodiscard]] static bool applyConfiguredVolume(const std::shared_ptr<creatures::Logger> &log,
                                                    const AudioConfig &config);
};

} // namespace creatures::audio
