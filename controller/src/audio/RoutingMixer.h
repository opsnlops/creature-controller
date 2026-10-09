#pragma once
#include "audio/audio-config.h"
#include <array>
#include <span>
#include <vector>

namespace creatures::audio {
using DecodedAudioFrame = std::array<int16_t, FRAMES_PER_CHUNK>;

// One mixer per output device; all buffers and filter states are allocated before playout.
class RoutingMixer {
  public:
    explicit RoutingMixer(const AudioEngineConfig &config);
    void resetFilters();
    void process(std::span<const DecodedAudioFrame> inputs, std::span<const float> gains,
                 std::span<int16_t> interleavedOutput);

  private:
    struct GainRamp {
        float current;
        float target;
        float step{0};
        size_t remaining{0};
        void setTarget(float gain);
        float next();
    };
    struct Biquad {
        double b0{0}, b1{0}, b2{0}, a1{0}, a2{0}, z1{0}, z2{0};
        float process(float value);
    };
    struct Crossover {
        std::array<Biquad, 2> stages;
        Crossover(float frequency, bool highPass);
        float process(float value);
    };
    struct Route {
        size_t input;
        size_t output;
        float gain;
    };
    struct Send {
        size_t input;
        float gain;
    };
    size_t channels_;
    float ceiling_;
    std::vector<GainRamp> gains_;
    std::vector<Route> routes_;
    std::vector<Send> bassSends_;
    std::optional<size_t> bassOutput_;
    float bassGain_{1};
    std::optional<Crossover> lowPass_;
    std::vector<std::optional<Crossover>> highPass_;
    std::vector<float> inputSamples_;
    std::vector<float> outputSamples_;
};
[[nodiscard]] float gainFromDecibels(float gainDb);
} // namespace creatures::audio
