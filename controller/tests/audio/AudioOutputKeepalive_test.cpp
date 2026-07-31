#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "audio/AudioOutput.h"
#include "audio/AudioOutputKeepalive.h"

namespace creatures::audio {
namespace {

class FakeAudioOutput final : public AudioOutput {
  public:
    bool open(const AudioConfig &) override { return true; }

    bool start() override {
        ++startCalls;
        return startSucceeds;
    }

    void stopAndClear() override {
        ++stopCalls;
        queued = 0;
    }

    void close() override {}

    bool write(std::span<const int16_t> samples) override {
        writeSizes.push_back(samples.size());
        allSamplesWereSilent = allSamplesWereSilent &&
                               std::all_of(samples.begin(), samples.end(), [](int16_t sample) { return sample == 0; });
        if (!writeSucceeds) {
            return false;
        }
        queued += samples.size();
        return true;
    }

    [[nodiscard]] size_t queuedFrames() const override { return queued; }
    [[nodiscard]] size_t pipelineLatencyFrames() const override { return 0; }
    [[nodiscard]] uint64_t underruns() const override { return 0; }

    size_t queued{0};
    size_t startCalls{0};
    size_t stopCalls{0};
    bool startSucceeds{true};
    bool writeSucceeds{true};
    bool allSamplesWereSilent{true};
    std::vector<size_t> writeSizes;
};

TEST(AudioOutputKeepaliveTest, PrimesTargetQueueAndStartsOutputOnce) {
    FakeAudioOutput output;
    AudioOutputKeepalive keepalive(output, 2 * FRAMES_PER_CHUNK);

    ASSERT_TRUE(keepalive.start());
    EXPECT_EQ(output.stopCalls, 1);
    EXPECT_EQ(output.startCalls, 1);
    EXPECT_EQ(output.queued, 2 * FRAMES_PER_CHUNK);
    EXPECT_EQ(output.writeSizes, (std::vector<size_t>{FRAMES_PER_CHUNK, FRAMES_PER_CHUNK}));
    EXPECT_TRUE(output.allSamplesWereSilent);

    ASSERT_TRUE(keepalive.start());
    EXPECT_EQ(output.stopCalls, 1);
    EXPECT_EQ(output.startCalls, 1);
}

TEST(AudioOutputKeepaliveTest, RefillsOnlyFramesMissingFromTarget) {
    FakeAudioOutput output;
    AudioOutputKeepalive keepalive(output, 2 * FRAMES_PER_CHUNK);
    ASSERT_TRUE(keepalive.start());

    output.writeSizes.clear();
    output.queued = FRAMES_PER_CHUNK + 123;

    ASSERT_TRUE(keepalive.refillSilence());
    EXPECT_EQ(output.queued, 2 * FRAMES_PER_CHUNK);
    EXPECT_EQ(output.writeSizes, (std::vector<size_t>{FRAMES_PER_CHUNK - 123}));
    EXPECT_EQ(output.stopCalls, 1);
    EXPECT_EQ(output.startCalls, 1);
}

TEST(AudioOutputKeepaliveTest, DoesNotWriteWhenTargetQueueIsFull) {
    FakeAudioOutput output;
    AudioOutputKeepalive keepalive(output, 2 * FRAMES_PER_CHUNK);
    ASSERT_TRUE(keepalive.start());

    output.writeSizes.clear();
    ASSERT_TRUE(keepalive.refillSilence());

    EXPECT_TRUE(output.writeSizes.empty());
    EXPECT_EQ(output.stopCalls, 1);
    EXPECT_EQ(output.startCalls, 1);
}

TEST(AudioOutputKeepaliveTest, RepeatedIdleRefillsDoNotRestartOutput) {
    FakeAudioOutput output;
    AudioOutputKeepalive keepalive(output, 2 * FRAMES_PER_CHUNK);
    ASSERT_TRUE(keepalive.start());

    for (size_t refill = 0; refill < 5; ++refill) {
        output.queued = FRAMES_PER_CHUNK;
        ASSERT_TRUE(keepalive.refillSilence());
        EXPECT_EQ(output.queued, 2 * FRAMES_PER_CHUNK);
    }

    EXPECT_EQ(output.stopCalls, 1);
    EXPECT_EQ(output.startCalls, 1);
}

TEST(AudioOutputKeepaliveTest, ClearsPrimedFramesWhenStartFails) {
    FakeAudioOutput output;
    output.startSucceeds = false;
    AudioOutputKeepalive keepalive(output, 2 * FRAMES_PER_CHUNK);

    EXPECT_FALSE(keepalive.start());
    EXPECT_EQ(output.stopCalls, 2);
    EXPECT_EQ(output.startCalls, 1);
    EXPECT_EQ(output.queued, 0);
}

} // namespace
} // namespace creatures::audio
