#include "audio/RoutingMixer.h"
#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
using namespace creatures::audio;
namespace {
AudioEngineConfig stereo() {
    auto config = creatureAudioConfig(1, "127.0.0.1", {});
    config.output.channels = {SpeakerChannel::FrontLeft, SpeakerChannel::FrontRight};
    config.output.limiterCeilingDb = 0;
    config.inputs.insert(config.inputs.begin() + 1, {2, 0});
    config.routes = {{0, 0, 0}, {1, 1, 0}, {2, 0, 0}, {2, 1, 0}};
    return config;
}
} // namespace
TEST(RoutingMixerTest, IsolatesBirdsAndFansOutBgm) {
    auto config = stereo();
    RoutingMixer mixer(config);
    std::vector<DecodedAudioFrame> frames(3);
    frames[0].fill(1000);
    frames[1].fill(2000);
    frames[2].fill(500);
    std::vector<int16_t> output(FRAMES_PER_CHUNK * 2);
    mixer.process(frames, std::array<float, 3>{1, 1, 1}, output);
    for (size_t i = 0; i < FRAMES_PER_CHUNK; ++i) {
        EXPECT_EQ(output[2 * i], 1500);
        EXPECT_EQ(output[2 * i + 1], 2500);
    }
}
TEST(RoutingMixerTest, MonoPreservesOriginalMixWithinPcmQuantization) {
    AudioConfig output;
    output.dialogGainDb = -3;
    output.bgmGainDb = 6;
    auto config = creatureAudioConfig(3, "127.0.0.1", output);
    RoutingMixer mixer(config);
    std::array<DecodedAudioFrame, 2> frames{};
    for (size_t i = 0; i < FRAMES_PER_CHUNK; ++i) {
        frames[0][i] = static_cast<int16_t>(static_cast<int>(i) * 131 - 31000);
        frames[1][i] = static_cast<int16_t>(31000 - static_cast<int>(i) * 131);
    }
    const std::array<float, 2> gains{gainFromDecibels(-3), gainFromDecibels(6)};
    DecodedAudioFrame mixed{};
    mixer.process(frames, gains, mixed);
    const float ceiling = gainFromDecibels(output.limiterCeilingDb);
    for (size_t i = 0; i < mixed.size(); ++i) {
        const float expected =
            std::clamp(frames[0][i] / 32768.0f * gains[0] + frames[1][i] / 32768.0f * gains[1], -ceiling, ceiling);
        EXPECT_NEAR(mixed[i], static_cast<int16_t>(std::lround(expected * 32767.0f)), 1);
    }
}
TEST(RoutingMixerTest, GainChangesRampOnceBeforeFanout) {
    auto config = stereo();
    config.routes = {{0, 0, 0}, {0, 1, 0}};
    RoutingMixer mixer(config);
    std::vector<DecodedAudioFrame> frames(3);
    frames[0].fill(10000);
    std::vector<int16_t> output(FRAMES_PER_CHUNK * 2);
    mixer.process(frames, std::array<float, 3>{0, 1, 1}, output);
    EXPECT_GT(output[0], 9000);
    EXPECT_EQ(output[2 * 95], 0);
    for (size_t i = 0; i < FRAMES_PER_CHUNK; ++i)
        EXPECT_EQ(output[2 * i], output[2 * i + 1]);
}
TEST(RoutingMixerTest, UnusedOutputsRemainSilent) {
    auto config = stereo();
    config.output.channels.push_back(SpeakerChannel::Center);
    RoutingMixer mixer(config);
    std::vector<DecodedAudioFrame> frames(3);
    for (auto &frame : frames)
        frame.fill(10000);
    std::vector<int16_t> output(FRAMES_PER_CHUNK * 3);
    mixer.process(frames, std::array<float, 3>{1, 1, 1}, output);
    for (size_t i = 0; i < FRAMES_PER_CHUNK; ++i)
        EXPECT_EQ(output[3 * i + 2], 0);
}
TEST(RoutingMixerTest, BassSendIsCountedOnceRegardlessOfBgmFanout) {
    auto one = stereo();
    one.output.channels.push_back(SpeakerChannel::Lfe);
    one.bass = BassConfig{2, 80, 0, false, {{2, 0}}, {}};
    auto two = one;
    two.routes.pop_back();
    RoutingMixer first(one), second(two);
    std::vector<DecodedAudioFrame> frames(3);
    frames[2].fill(1000);
    std::vector<int16_t> a(FRAMES_PER_CHUNK * 3), b(a.size());
    first.process(frames, std::array<float, 3>{1, 1, 1}, a);
    second.process(frames, std::array<float, 3>{1, 1, 1}, b);
    for (size_t i = 0; i < FRAMES_PER_CHUNK; ++i)
        EXPECT_EQ(a[3 * i + 2], b[3 * i + 2]);
}
TEST(RoutingMixerTest, LinkwitzRileyHasHalfAmplitudeAtCrossoverAndRejectsHighFrequencies) {
    auto measure = [](float frequency) {
        auto config = stereo();
        config.output.channels.push_back(SpeakerChannel::Lfe);
        config.bass = BassConfig{2, 80, 0, false, {{2, 0}}, {0}};
        config.routes = {{2, 0, 0}};
        RoutingMixer mixer(config);
        std::vector<DecodedAudioFrame> frames(3);
        std::vector<int16_t> output(FRAMES_PER_CHUNK * 3);
        double bassEnergy = 0, mainEnergy = 0, inputEnergy = 0;
        for (size_t block = 0; block < 200; ++block) {
            for (size_t i = 0; i < FRAMES_PER_CHUNK; ++i)
                frames[2][i] = static_cast<int16_t>(
                    10000 * std::sin(2 * std::numbers::pi * frequency * (block * FRAMES_PER_CHUNK + i) / SAMPLE_RATE));
            mixer.process(frames, std::array<float, 3>{1, 1, 1}, output);
            if (block < 100)
                continue;
            for (size_t i = 0; i < FRAMES_PER_CHUNK; ++i) {
                bassEnergy += static_cast<double>(output[3 * i + 2]) * output[3 * i + 2];
                mainEnergy += static_cast<double>(output[3 * i]) * output[3 * i];
                inputEnergy += static_cast<double>(frames[2][i]) * frames[2][i];
            }
        }
        return std::pair{std::sqrt(bassEnergy / inputEnergy), std::sqrt(mainEnergy / inputEnergy)};
    };
    const auto crossover = measure(80);
    EXPECT_NEAR(crossover.first, 0.5, 0.005);
    EXPECT_NEAR(crossover.second, 0.5, 0.005);
    const auto high = measure(1000);
    EXPECT_LT(high.first, 0.001);
    EXPECT_GT(high.second, 0.99);
    const auto low = measure(20);
    EXPECT_GT(low.first, 0.99);
    EXPECT_LT(low.second, 0.01);
}
TEST(RoutingMixerTest, RejectsMismatchedBuffers) {
    auto config = stereo();
    RoutingMixer mixer(config);
    EXPECT_THROW(mixer.process({}, {}, {}), std::invalid_argument);
}

TEST(RoutingMixerTest, NewSessionResetClearsCrossoverHistory) {
    auto config = stereo();
    config.output.channels.push_back(SpeakerChannel::Lfe);
    config.bass = BassConfig{2, 80, 0, false, {{2, 0}}, {0, 1}};
    RoutingMixer mixer(config);
    std::vector<DecodedAudioFrame> frames(3);
    for (auto &frame : frames)
        frame.fill(1000);
    std::vector<int16_t> output(FRAMES_PER_CHUNK * 3);
    mixer.process(frames, std::array<float, 3>{1, 1, 1}, output);
    mixer.resetFilters();
    for (auto &frame : frames)
        frame.fill(0);
    mixer.process(frames, std::array<float, 3>{1, 1, 1}, output);
    EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](int16_t value) { return value == 0; }));
}
