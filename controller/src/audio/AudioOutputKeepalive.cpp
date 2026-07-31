#include "audio/AudioOutputKeepalive.h"

#include <algorithm>
#include <span>

namespace creatures::audio {

AudioOutputKeepalive::AudioOutputKeepalive(AudioOutput &output, size_t targetFrames)
    : output_(output), targetFrames_(targetFrames) {}

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
        const size_t framesToWrite = std::min(framesRemaining, silence_.size());
        if (!output_.write(std::span<const int16_t>(silence_.data(), framesToWrite))) {
            return false;
        }
        framesRemaining -= framesToWrite;
    }
    return true;
}

} // namespace creatures::audio
