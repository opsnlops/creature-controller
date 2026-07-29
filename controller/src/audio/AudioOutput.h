#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <span>

#include "audio/audio-config.h"
#include "logging/Logger.h"

namespace creatures::audio {

class AudioOutput {
  public:
    virtual ~AudioOutput() = default;

    virtual bool open(const AudioConfig &config) = 0;
    virtual bool start() = 0;
    virtual void stopAndClear() = 0;
    virtual void close() = 0;

    virtual bool write(std::span<const int16_t> samples) = 0;

    // Best available estimate of frames waiting between the application and
    // the speaker. Native backends include the hardware pipeline where the
    // platform exposes it.
    [[nodiscard]] virtual size_t queuedFrames() const = 0;
    [[nodiscard]] virtual size_t pipelineLatencyFrames() const = 0;
    [[nodiscard]] virtual uint64_t underruns() const = 0;
};

std::unique_ptr<AudioOutput> createAudioOutput(std::shared_ptr<creatures::Logger> log);
void listAudioOutputDevices(std::ostream &output);

} // namespace creatures::audio
