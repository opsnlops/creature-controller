#include "audio/AudioEngine.h"
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <gtest/gtest.h>
#include <map>
#include <mutex>
#include <numbers>
using namespace creatures::audio;
namespace {
class QuietLogger : public creatures::Logger {
  public:
    void init(std::string) override {}
    void setLevel(const std::string &) override {}

  protected:
    void logTrace(const std::string &, fmt::format_args) override {}
    void logDebug(const std::string &, fmt::format_args) override {}
    void logInfo(const std::string &, fmt::format_args) override {}
    void logWarning(const std::string &, fmt::format_args) override {}
    void logError(const std::string &, fmt::format_args) override {}
    void logCritical(const std::string &, fmt::format_args) override {}
};
class TestOutput : public AudioOutput {
  public:
    bool open(const AudioConfig &config) override {
        channels = config.channels.size();
        return openSucceeds;
    }
    bool start() override {
        std::lock_guard lock(mutex);
        started = true;
        last = std::chrono::steady_clock::now();
        ++starts;
        return true;
    }
    void stopAndClear() override {
        std::lock_guard lock(mutex);
        started = false;
        queued = 0;
    }
    void close() override { closed.store(true); }
    bool write(std::span<const int16_t> samples) override {
        std::lock_guard lock(mutex);
        if (failWrite.load())
            return false;
        queued += static_cast<double>(samples.size() / channels);
        if (std::any_of(samples.begin(), samples.end(), [](int16_t value) { return value != 0; }))
            nonSilentWrites.fetch_add(1);
        for (size_t index = 0; index < samples.size(); ++index) {
            auto &maximum = channelMaximum[index % channels];
            maximum.store(std::max(maximum.load(), std::abs(static_cast<int>(samples[index]))));
        }
        ++writes;
        return true;
    }
    size_t queuedFrames() const override {
        std::lock_guard lock(mutex);
        const auto now = std::chrono::steady_clock::now();
        if (started)
            queued = std::max(0.0, queued - std::chrono::duration<double>(now - last).count() * SAMPLE_RATE);
        last = now;
        return static_cast<size_t>(queued);
    }
    size_t pipelineLatencyFrames() const override { return pipeline; }
    uint64_t underruns() const override { return 0; }
    bool openSucceeds{true};
    std::atomic<bool> failWrite{false}, closed{false};
    std::atomic<size_t> starts{0}, writes{0}, nonSilentWrites{0};
    std::array<std::atomic<int>, 8> channelMaximum{};
    size_t channels{1};
    size_t pipeline{0};
    mutable std::mutex mutex;
    mutable double queued{0};
    mutable std::chrono::steady_clock::time_point last;
    bool started{false};
};
struct PacketHub {
    std::mutex mutex;
    std::map<std::pair<std::string, uint16_t>, std::deque<std::vector<uint8_t>>> queues;
    bool openSucceeds{true};
    std::atomic<bool> failReceive{false};
};
class TestReceiver : public PacketReceiver {
  public:
    explicit TestReceiver(std::shared_ptr<PacketHub> hub) : hub_(std::move(hub)) {}
    bool open(const std::string &group, const std::string &, uint16_t port) override {
        key_ = {group, port};
        return hub_->openSucceeds;
    }
    Result receive(std::vector<uint8_t> &packet, size_t) override {
        if (hub_->failReceive.load())
            return Result::Error;
        {
            std::lock_guard lock(hub_->mutex);
            auto &queue = hub_->queues[key_];
            if (!queue.empty()) {
                packet = std::move(queue.front());
                queue.pop_front();
                return Result::Packet;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return Result::Timeout;
    }

  private:
    std::shared_ptr<PacketHub> hub_;
    std::pair<std::string, uint16_t> key_;
};
PacketReceiverFactory factory(std::shared_ptr<PacketHub> hub) {
    return [hub] { return std::make_unique<TestReceiver>(hub); };
}
void put32(std::vector<uint8_t> &bytes, uint32_t value) {
    for (int shift : {24, 16, 8, 0})
        bytes.push_back(static_cast<uint8_t>(value >> shift));
}
void seedBgm(const std::shared_ptr<PacketHub> &hub, bool withRtcp, uint32_t source = 100,
             const std::string &group = BGM_GROUP, const std::string &cname = "workshop") {
    std::vector<uint8_t> report{0x80, 200, 0, 6};
    put32(report, source);
    const auto epoch = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch);
    const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(epoch - seconds).count();
    put32(report, static_cast<uint32_t>(seconds.count() + 2208988800LL));
    put32(report, static_cast<uint32_t>(static_cast<uint64_t>(nanos) * (uint64_t{1} << 32) / 1000000000));
    put32(report, 0);
    put32(report, 0);
    put32(report, 0);
    std::vector<uint8_t> sdes{0x81, 202, 0, 0};
    put32(sdes, source);
    sdes.push_back(1);
    sdes.push_back(static_cast<uint8_t>(cname.size()));
    sdes.insert(sdes.end(), cname.begin(), cname.end());
    sdes.push_back(0);
    while (sdes.size() % 4)
        sdes.push_back(0);
    sdes[3] = static_cast<uint8_t>(sdes.size() / 4 - 1);
    report.insert(report.end(), sdes.begin(), sdes.end());
    int error;
    OpusEncoder *encoder = opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_AUDIO, &error);
    if (!encoder || error != OPUS_OK)
        throw std::runtime_error("Test encoder initialization failed");
    std::lock_guard lock(hub->mutex);
    if (withRtcp)
        hub->queues[{group, RTCP_PORT}].push_back(report);
    for (uint16_t i = 0; i < 16; ++i) {
        DecodedAudioFrame samples;
        for (size_t j = 0; j < samples.size(); ++j)
            samples[j] = static_cast<int16_t>(
                5000 * std::sin(2 * std::numbers::pi * 440 * (i * FRAMES_PER_CHUNK + j) / SAMPLE_RATE));
        std::array<uint8_t, 1024> payload{};
        const int size = opus_encode(encoder, samples.data(), FRAMES_PER_CHUNK, payload.data(), payload.size());
        if (size <= 0) {
            opus_encoder_destroy(encoder);
            throw std::runtime_error("Test Opus encode failed");
        }
        std::vector<uint8_t> packet{0x80, RTP_OPUS_PAYLOAD_TYPE, static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
        put32(packet, i * FRAMES_PER_CHUNK);
        put32(packet, source);
        packet.insert(packet.end(), payload.begin(), payload.begin() + size);
        hub->queues[{group, RTP_PORT}].push_back(std::move(packet));
    }
    opus_encoder_destroy(encoder);
}
bool waitUntil(const std::function<bool()> &predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!predicate() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return predicate();
}
AudioEngineConfig engineConfig(bool requireRtcp = true) {
    auto config = creatureAudioConfig(1, "127.0.0.1", {});
    config.requireRtcp = requireRtcp;
    config.output.commonPlayoutDelayMs = 40;
    return config;
}
} // namespace
TEST(AudioEngineTest, StartupFailureIsSynchronousAndResourcesClose) {
    auto output = std::make_unique<TestOutput>();
    auto *device = output.get();
    device->openSucceeds = false;
    AudioEngine engine(std::make_shared<QuietLogger>(), engineConfig(), std::move(output),
                       factory(std::make_shared<PacketHub>()));
    EXPECT_FALSE(engine.start());
    EXPECT_EQ(engine.state(), AudioEngine::State::Failed);
    EXPECT_TRUE(device->closed.load());
    engine.shutdown();
    engine.shutdown();
}
TEST(AudioEngineTest, SocketFailureIsSynchronous) {
    auto hub = std::make_shared<PacketHub>();
    hub->openSucceeds = false;
    AudioEngine engine(std::make_shared<QuietLogger>(), engineConfig(), std::make_unique<TestOutput>(), factory(hub));
    EXPECT_FALSE(engine.start());
    EXPECT_EQ(engine.state(), AudioEngine::State::Failed);
}
TEST(AudioEngineTest, RequiredRtcpKeepsUnsynchronizedBgmSilent) {
    auto hub = std::make_shared<PacketHub>();
    seedBgm(hub, false);
    auto output = std::make_unique<TestOutput>();
    auto *device = output.get();
    AudioEngine engine(std::make_shared<QuietLogger>(), engineConfig(), std::move(output), factory(hub));
    ASSERT_TRUE(engine.start());
    ASSERT_TRUE(waitUntil([&] { return engine.getPacketsReceived() >= 16; }));
    EXPECT_EQ(device->nonSilentWrites.load(), 0);
    EXPECT_EQ(engine.getRtcpFallbacks(), 0);
    EXPECT_STREQ(engine.getTimingModeName(), "waiting");
    engine.shutdown();
    const auto writes = device->writes.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(device->writes.load(), writes);
    EXPECT_TRUE(device->closed.load());
}
TEST(AudioEngineTest, MissingBirdDoesNotPreventRtcpBgmPlayout) {
    auto hub = std::make_shared<PacketHub>();
    seedBgm(hub, true);
    auto output = std::make_unique<TestOutput>();
    auto *device = output.get();
    AudioEngine engine(std::make_shared<QuietLogger>(), engineConfig(), std::move(output), factory(hub));
    ASSERT_TRUE(engine.start());
    ASSERT_TRUE(waitUntil([&] { return device->nonSilentWrites.load() > 0; }));
    EXPECT_STREQ(engine.getTimingModeName(), "rtcp");
    EXPECT_EQ(engine.getRtcpFallbacks(), 0);
    EXPECT_EQ(device->starts.load(), 1);
    engine.shutdown();
    EXPECT_EQ(engine.state(), AudioEngine::State::Stopped);
}
TEST(AudioEngineTest, ControllerConfigurationRetainsArrivalFallback) {
    auto hub = std::make_shared<PacketHub>();
    seedBgm(hub, false);
    auto output = std::make_unique<TestOutput>();
    auto *device = output.get();
    AudioEngine engine(std::make_shared<QuietLogger>(), engineConfig(false), std::move(output), factory(hub));
    ASSERT_TRUE(engine.start());
    ASSERT_TRUE(waitUntil([&] { return device->nonSilentWrites.load() > 0; }));
    EXPECT_STREQ(engine.getTimingModeName(), "arrival_fallback");
    EXPECT_EQ(engine.getRtcpFallbacks(), 1);
    engine.shutdown();
}
TEST(AudioEngineTest, DeviceFailureIsReportedAndWorkersJoin) {
    auto hub = std::make_shared<PacketHub>();
    auto output = std::make_unique<TestOutput>();
    auto *device = output.get();
    AudioEngine engine(std::make_shared<QuietLogger>(), engineConfig(), std::move(output), factory(hub));
    ASSERT_TRUE(engine.start());
    device->failWrite.store(true);
    ASSERT_TRUE(waitUntil([&] { return engine.state() == AudioEngine::State::Failed; }));
    engine.shutdown();
    EXPECT_TRUE(device->closed.load());
}
TEST(AudioEngineTest, ReceiverFailureDoesNotTerminateProcess) {
    auto hub = std::make_shared<PacketHub>();
    AudioEngine engine(std::make_shared<QuietLogger>(), engineConfig(), std::make_unique<TestOutput>(), factory(hub));
    ASSERT_TRUE(engine.start());
    hub->failReceive.store(true);
    ASSERT_TRUE(waitUntil([&] { return engine.state() == AudioEngine::State::Failed; }));
    engine.shutdown();
}

TEST(AudioEngineTest, CleanRestartDoesNotReuseOldPacketsOrTimingReports) {
    auto hub = std::make_shared<PacketHub>();
    seedBgm(hub, true);
    auto output = std::make_unique<TestOutput>();
    auto *device = output.get();
    AudioEngine engine(std::make_shared<QuietLogger>(), engineConfig(), std::move(output), factory(hub));
    ASSERT_TRUE(engine.start());
    ASSERT_TRUE(waitUntil([&] { return device->nonSilentWrites.load() > 0; }));
    engine.shutdown();
    const auto played = device->nonSilentWrites.load();
    ASSERT_TRUE(engine.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    EXPECT_EQ(device->nonSilentWrites.load(), played);
    EXPECT_STREQ(engine.getTimingModeName(), "waiting");
    EXPECT_FALSE(engine.getLastRtcpReportAge());
    engine.shutdown();
}

TEST(AudioEngineTest, ExcessiveDeviceLatencyFailsWithNoRunningWorkers) {
    auto output = std::make_unique<TestOutput>();
    auto *device = output.get();
    device->pipeline = SAMPLE_RATE / 10;
    AudioEngine engine(std::make_shared<QuietLogger>(), engineConfig(), std::move(output),
                       factory(std::make_shared<PacketHub>()));
    EXPECT_FALSE(engine.start());
    EXPECT_EQ(engine.state(), AudioEngine::State::Failed);
    EXPECT_TRUE(device->closed.load());
    EXPECT_EQ(device->starts.load(), 0);
}

TEST(AudioEngineTest, UnqualifiedBirdRemainsSilentInMultichannelOutputWhileBgmPlays) {
    for (bool incompatibleReport : {false, true}) {
        auto hub = std::make_shared<PacketHub>();
        seedBgm(hub, incompatibleReport, 200, "239.19.63.1", "other-clock");
        seedBgm(hub, true);
        auto output = std::make_unique<TestOutput>();
        auto *device = output.get();
        auto config = engineConfig();
        config.output.channels = {SpeakerChannel::FrontLeft, SpeakerChannel::FrontRight, SpeakerChannel::RearLeft,
                                  SpeakerChannel::RearRight, SpeakerChannel::Center,     SpeakerChannel::Lfe,
                                  SpeakerChannel::SideLeft,  SpeakerChannel::SideRight};
        config.routes = {{0, 0, 0}, {1, 1, 0}};
        AudioEngine engine(std::make_shared<QuietLogger>(), config, std::move(output), factory(hub));
        ASSERT_TRUE(engine.start());
        ASSERT_TRUE(waitUntil([&] { return device->channelMaximum[1].load() > 0; }));
        EXPECT_EQ(device->channelMaximum[0].load(), 0);
        for (size_t channel = 2; channel < 8; ++channel)
            EXPECT_EQ(device->channelMaximum[channel].load(), 0);
        EXPECT_STREQ(engine.getTimingModeName(), "rtcp");
        EXPECT_EQ(engine.getRtcpFallbacks(), 0);
        engine.shutdown();
    }
}
