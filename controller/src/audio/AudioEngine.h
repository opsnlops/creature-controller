#pragma once
#include "audio/AudioOutput.h"
#include "audio/AudioOutputKeepalive.h"
#include "audio/PacketReceiver.h"
#include "audio/RoutingMixer.h"
#include "audio/RtcpTiming.h"
#include "audio/RtpJitterBuffer.h"
#include "logging/Logger.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <opus.h>
#include <string>
#include <thread>
#include <vector>

namespace creatures::audio {
class AudioEngine final {
  public:
    enum class State { Stopped, Starting, Running, Stopping, Failed };
    AudioEngine(std::shared_ptr<Logger> log, AudioEngineConfig config, std::unique_ptr<AudioOutput> output = nullptr,
                PacketReceiverFactory receivers = {});
    ~AudioEngine();
    AudioEngine(const AudioEngine &) = delete;
    AudioEngine &operator=(const AudioEngine &) = delete;
    [[nodiscard]] bool start();
    void shutdown();
    void setInputGainDb(uint8_t channel, float gainDb);
    [[nodiscard]] State state() const { return state_.load(); }
    [[nodiscard]] bool isReceiving() const;
    [[nodiscard]] uint64_t getPacketsReceived() const { return totalPackets_.load(); }
    [[nodiscard]] float getBufferLevel() const { return bufferLevel_.load(); }
    [[nodiscard]] size_t getOutputQueuedFrames() const { return outputQueuedFrames_.load(); }
    [[nodiscard]] size_t getBufferedFrames(uint8_t channel) const;
    [[nodiscard]] std::optional<std::chrono::milliseconds> getLastPacketAge() const;
    [[nodiscard]] std::optional<std::chrono::milliseconds> getLastRtcpReportAge() const;
    [[nodiscard]] uint64_t getRtcpReportsReceived() const;
    [[nodiscard]] uint64_t getRtcpInvalidReports() const;
    [[nodiscard]] uint64_t getRtcpFallbacks() const { return rtcpFallbacks_.load(); }
    [[nodiscard]] int64_t getLastStartLatenessMicroseconds() const { return lastStartLatenessMicroseconds_.load(); }
    [[nodiscard]] const char *getTimingModeName() const;
    [[nodiscard]] std::string getStats() const;

  private:
    struct StreamStats {
        std::atomic<uint64_t> packetsReceived{0}, invalidPackets{0}, duplicatePackets{0}, bufferOverruns{0},
            decodedFrames{0}, fecFrames{0}, concealedFrames{0}, decodeErrors{0};
    };
    struct RtcpStats {
        std::atomic<uint64_t> reportsReceived{0}, invalidReports{0}, cacheEvictions{0}, sourceMismatches{0},
            staleReports{0};
    };
    struct InputStream {
        explicit InputStream(InputConfig config, size_t bufferCapacity);
        uint8_t channel;
        std::string name;
        std::string group;
        RtpJitterBuffer buffer;
        RtcpReportCache reports{RTCP_REPORT_CACHE_ENTRIES};
        StreamStats stats;
        RtcpStats rtcpStats;
        OpusDecoder *decoder{nullptr};
        uint64_t decoderGeneration{0};
        uint64_t qualifiedGeneration{0};
        std::atomic<float> gain;
        std::unique_ptr<PacketReceiver> receiverSource, rtcpReceiverSource;
        std::thread receiver, rtcpReceiver;
    };
    enum class TimingMode : uint8_t { Waiting, Rtcp, ArrivalFallback };
    void run();
    void receiveStream(InputStream &stream);
    void receiveRtcpStream(InputStream &stream);
    void audioPlayoutThread();
    void mixTimestamp(uint32_t timestamp, uint64_t masterGeneration, std::span<int16_t> mixed);
    bool decodeTimestamp(InputStream &stream, uint32_t timestamp, uint64_t generation, DecodedAudioFrame &samples);
    bool initializeDecoders();
    bool openSockets();
    void joinWorkerThreads();
    void releaseResources();
    std::shared_ptr<Logger> log_;
    const AudioEngineConfig config_;
    const AudioConfig &audioConfig_;
    const std::string &interfaceIp_;
    const uint16_t port_;
    std::vector<std::unique_ptr<InputStream>> streams_;
    size_t masterIndex_{0};
    std::unique_ptr<AudioOutput> audioOutput_;
    std::unique_ptr<AudioOutputKeepalive> keepalive_;
    PacketReceiverFactory receiverFactory_;
    RoutingMixer mixer_;
    std::vector<DecodedAudioFrame> decoded_;
    std::vector<float> gains_;
    std::vector<uint64_t> decodedGenerations_;
    std::optional<RtcpSenderReport> activeReport_;
    std::thread mainThread_, playoutThread_;
    std::mutex lifecycleMutex_;
    std::atomic<bool> stop_requested{false};
    std::atomic<State> state_{State::Stopped};
    std::atomic<int64_t> lastPacketArrivalNanoseconds_{0};
    std::atomic<uint64_t> totalPackets_{0};
    std::atomic<float> bufferLevel_{0};
    std::atomic<size_t> outputQueuedFrames_{0};
    std::atomic<uint64_t> mixedFrames_{0}, playoutDeadlineMisses_{0}, rtcpFallbacks_{0}, rtcpLateFramesDropped_{0};
    std::atomic<int64_t> lastStartLatenessMicroseconds_{0};
    std::atomic<TimingMode> timingMode_{TimingMode::Waiting};
};
} // namespace creatures::audio
