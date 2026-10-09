#include "audio/AudioOutputKeepalive.h"

#include <algorithm>
#include <span>
#include <stdexcept>

namespace creatures::audio {

AudioOutputKeepalive::AudioOutputKeepalive(AudioOutput &output, size_t targetFrames, size_t channels)
    : output_(output), targetFrames_(targetFrames), channels_(channels), silence_(FRAMES_PER_CHUNK * channels) {
    if (channels == 0 || channels > 8)
        throw std::invalid_argument("Invalid keepalive channel count");
}

bool AudioOutputKeepalive::start() {
    if (started_) {
        return true;
    }

    output_.stopAndClear();
    if (!refillSilence() || !output_.start()) {
        output_.stopAndClear();
        return false;
    }

    started_ = true;
    return true;
}

bool AudioOutputKeepalive::refillSilence() {
    const size_t queuedFrames = output_.queuedFrames();
    if (queuedFrames >= targetFrames_) {
        return true;
    }

    size_t framesRemaining = targetFrames_ - queuedFrames;
    while (framesRemaining > 0) {
        const size_t framesToWrite = std::min<size_t>(framesRemaining, FRAMES_PER_CHUNK);
        if (!output_.write(std::span<const int16_t>(silence_.data(), framesToWrite * channels_))) {
            return false;
        }
        framesRemaining -= framesToWrite;
    }
    return true;
}

} // namespace creatures::audio
