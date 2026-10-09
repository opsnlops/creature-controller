#include "audio/RoutingMixer.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace creatures::audio {
float gainFromDecibels(float gainDb) { return std::pow(10.0f, gainDb / 20.0f); }

void RoutingMixer::GainRamp::setTarget(float gain) {
    if (gain == target)
        return;
    target = gain;
    remaining = SAMPLE_RATE * GAIN_RAMP_MS / 1000;
    step = (target - current) / static_cast<float>(remaining);
}
float RoutingMixer::GainRamp::next() {
    if (remaining) {
        current += step;
        if (--remaining == 0)
            current = target;
    }
    return current;
}

RoutingMixer::Crossover::Crossover(float frequency, bool highPass) {
    const double omega = 2 * std::numbers::pi * frequency / SAMPLE_RATE;
    const double cosine = std::cos(omega);
    const double alpha = std::sin(omega) / std::sqrt(2.0);
    const double a0 = 1 + alpha;
    Biquad filter;
    filter.b0 = (highPass ? 1 + cosine : 1 - cosine) / (2 * a0);
    filter.b1 = (highPass ? -(1 + cosine) : 1 - cosine) / a0;
    filter.b2 = filter.b0;
    filter.a1 = -2 * cosine / a0;
    filter.a2 = (1 - alpha) / a0;
    stages = {filter, filter};
}
float RoutingMixer::Biquad::process(float value) {
    const double output = b0 * value + z1;
    z1 = b1 * value - a1 * output + z2;
    z2 = b2 * value - a2 * output;
    // Prevent denormal state during long periods of silence.
    if (std::abs(z1) < 1e-20)
        z1 = 0;
    if (std::abs(z2) < 1e-20)
        z2 = 0;
    return static_cast<float>(output);
}
float RoutingMixer::Crossover::process(float value) {
    for (auto &stage : stages)
        value = stage.process(value);
    return value;
}

RoutingMixer::RoutingMixer(const AudioEngineConfig &config)
    : channels_(config.output.channels.size()), ceiling_(gainFromDecibels(config.output.limiterCeilingDb)),
      highPass_(channels_), inputSamples_(config.inputs.size()), outputSamples_(channels_) {
    validateAudioEngineConfig(config);
    for (const auto &input : config.inputs) {
        const float gain = gainFromDecibels(input.gainDb);
        gains_.push_back({gain, gain});
    }
    for (const auto &route : config.routes)
        routes_.push_back({route.input, route.output, gainFromDecibels(route.gainDb)});
    if (config.bass) {
        const auto &bass = *config.bass;
        bassOutput_ = bass.output;
        bassGain_ = gainFromDecibels(bass.gainDb) * (bass.invertPolarity ? -1.0f : 1.0f);
        lowPass_.emplace(bass.crossoverHz, false);
        for (const auto &send : bass.sends)
            bassSends_.push_back({send.input, gainFromDecibels(send.gainDb)});
        for (size_t output : bass.highPassOutputs)
            highPass_[output].emplace(bass.crossoverHz, true);
    }
}

void RoutingMixer::resetFilters() {
    const auto reset = [](Crossover &filter) {
        for (auto &stage : filter.stages) {
            stage.z1 = 0;
            stage.z2 = 0;
        }
    };
    if (lowPass_)
        reset(*lowPass_);
    for (auto &filter : highPass_)
        if (filter)
            reset(*filter);
}

void RoutingMixer::process(std::span<const DecodedAudioFrame> inputs, std::span<const float> gains,
                           std::span<int16_t> interleavedOutput) {
    if (inputs.size() != gains_.size() || gains.size() != gains_.size() ||
        interleavedOutput.size() != FRAMES_PER_CHUNK * channels_)
        throw std::invalid_argument("Audio mixer frame dimensions do not match its configuration");
    for (size_t input = 0; input < inputs.size(); ++input)
        gains_[input].setTarget(gains[input]);
    for (size_t sample = 0; sample < FRAMES_PER_CHUNK; ++sample) {
        std::fill(outputSamples_.begin(), outputSamples_.end(), 0);
        for (size_t input = 0; input < inputs.size(); ++input)
            inputSamples_[input] = static_cast<float>(inputs[input][sample]) / 32768.0f * gains_[input].next();
        for (const auto &route : routes_)
            outputSamples_[route.output] += inputSamples_[route.input] * route.gain;
        if (bassOutput_) {
            float bass = 0;
            for (const auto &send : bassSends_)
                bass += inputSamples_[send.input] * send.gain;
            outputSamples_[*bassOutput_] = lowPass_->process(bass) * bassGain_;
        }
        for (size_t output = 0; output < channels_; ++output) {
            float value = outputSamples_[output];
            if (highPass_[output])
                value = highPass_[output]->process(value);
            value = std::clamp(value, -ceiling_, ceiling_);
            interleavedOutput[sample * channels_ + output] = static_cast<int16_t>(std::lround(value * 32767.0f));
        }
    }
}
} // namespace creatures::audio
