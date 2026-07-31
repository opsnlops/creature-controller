#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <opus.h>

#include "audio/AudioOutput.h"
#include "audio/RtcpTiming.h"
#include "audio/audio-config.h"
#include "logging/Logger.h"
#include "util/StoppableThread.h"

namespace creatures::audio {

class OpusRtpAudioClient final : public StoppableThread {
  public:
    OpusRtpAudioClient(std::shared_ptr<creatures::Logger> log, std::string dialogGroup, std::string bgmGroup,
                       uint16_t port, uint8_t dialogIndex, std::string interfaceIp, AudioConfig audioConfig);
    ~OpusRtpAudioClient() override;

    void start() override;
    void shutdown() override;

    void setDialogGainDb(float gainDb);
    void setBgmGainDb(float gainDb);

    [[nodiscard]] bool isReceiving() const;
    [[nodiscard]] uint64_t getPacketsReceived() const { return totalPackets_.load(); }
    [[nodiscard]] float getBufferLevel() const { return bufferLevel_.load(); }
    [[nodiscard]] size_t getOutputQueuedFrames() const { return outputQueuedFrames_.load(); }
    [[nodiscard]] size_t getDialogBufferedFrames() const { return dialogBufferedFrames_.load(); }
    [[nodiscard]] size_t getBgmBufferedFrames() const { return bgmBufferedFrames_.load(); }
    [[nodiscard]] std::optional<std::chrono::milliseconds> getLastPacketAge() const;
    [[nodiscard]] std::optional<std::chrono::milliseconds> getLastRtcpReportAge() const;
    [[nodiscard]] uint64_t getRtcpReportsReceived() const;
    [[nodiscard]] uint64_t getRtcpInvalidReports() const;
    [[nodiscard]] uint64_t getRtcpFallbacks() const { return rtcpFallbacks_.load(); }
    [[nodiscard]] int64_t getLastStartLatenessMicroseconds() const { return lastStartLatenessMicroseconds_.load(); }
    [[nodiscard]] const char *getTimingModeName() const;

  private:
    class RtpJitterBuffer;

    struct StreamStats {
        std::atomic<uint64_t> packetsReceived{0};
        std::atomic<uint64_t> invalidPackets{0};
        std::atomic<uint64_t> duplicatePackets{0};
        std::atomic<uint64_t> bufferOverruns{0};
        std::atomic<uint64_t> decodedFrames{0};
        std::atomic<uint64_t> fecFrames{0};
        std::atomic<uint64_t> concealedFrames{0};
        std::atomic<uint64_t> decodeErrors{0};
    };

    struct RtcpStats {
        std::atomic<uint64_t> reportsReceived{0};
        std::atomic<uint64_t> invalidReports{0};
        std::atomic<uint64_t> cacheEvictions{0};
        std::atomic<uint64_t> sourceMismatches{0};
        std::atomic<uint64_t> staleReports{0};
    };

    enum class TimingMode : uint8_t {
        Waiting,
        Rtcp,
        ArrivalFallback,
    };

    struct GainRamp {
        explicit GainRamp(float initialGain) : current(initialGain), target(initialGain) {}

        void setTarget(float gain);
        float next();

        float current;
        float target;
        float step{0.0f};
        size_t samplesRemaining{0};
    };

    void run() override;
    void receiveStream(int socket, RtpJitterBuffer &buffer, StreamStats &stats, const std::string &streamName);
    void receiveRtcpStream(int socket, RtcpReportCache &reports, RtcpStats &stats, const std::string &streamName);
    void audioPlayoutThread();

    bool initializeAudioDevice();
    bool initializeDecoders();
    bool openSockets();
    void joinWorkerThreads();
    void releaseResources();

    bool decodeTimestamp(RtpJitterBuffer &buffer, OpusDecoder *decoder, uint32_t timestamp,
                         std::array<int16_t, FRAMES_PER_CHUNK> &samples, StreamStats &stats,
                         uint64_t &observedGeneration, const char *streamName);
    void mixTimestamp(uint32_t timestamp, std::array<int16_t, FRAMES_PER_CHUNK> &mixed, uint64_t &dialogGeneration,
                      uint64_t &bgmGeneration, GainRamp &dialogGain, GainRamp &bgmGain);

    bool openSocket(int &socket, const std::string &group, uint16_t port, const char *protocol) const;
    static bool receivePacket(int socket, std::vector<uint8_t> &packet, size_t maximumSize);

    std::shared_ptr<creatures::Logger> log_;
    const std::string dialogGroup_;
    const std::string bgmGroup_;
    const std::string interfaceIp_;
    const uint16_t port_;
    const uint8_t dialogIndex_;
    const AudioConfig audioConfig_;

    int dialogSocket_{-1};
    int bgmSocket_{-1};
    int dialogRtcpSocket_{-1};
    int bgmRtcpSocket_{-1};
    OpusDecoder *dialogDecoder_{nullptr};
    OpusDecoder *bgmDecoder_{nullptr};
    std::unique_ptr<AudioOutput> audioOutput_;

    std::unique_ptr<RtpJitterBuffer> dialogBuffer_;
    std::unique_ptr<RtpJitterBuffer> bgmBuffer_;
    RtcpReportCache dialogRtcpReports_{RTCP_REPORT_CACHE_ENTRIES};
    RtcpReportCache bgmRtcpReports_{RTCP_REPORT_CACHE_ENTRIES};

    std::thread mainThread_;
    std::thread dialogThread_;
    std::thread bgmThread_;
    std::thread dialogRtcpThread_;
    std::thread bgmRtcpThread_;
    std::thread playoutThread_;

    std::atomic<float> dialogGainLinear_{1.0f};
    std::atomic<float> bgmGainLinear_{1.0f};
    const float limiterCeilingLinear_;

    std::atomic<bool> running_{false};
    std::atomic<int64_t> lastPacketArrivalNanoseconds_{0};
    std::atomic<uint64_t> totalPackets_{0};
    std::atomic<float> bufferLevel_{0.0f};
    std::atomic<size_t> outputQueuedFrames_{0};
    std::atomic<size_t> dialogBufferedFrames_{0};
    std::atomic<size_t> bgmBufferedFrames_{0};
    std::atomic<uint64_t> mixedFrames_{0};
    std::atomic<uint64_t> playoutDeadlineMisses_{0};
    std::atomic<uint64_t> rtcpFallbacks_{0};
    std::atomic<uint64_t> rtcpLateFramesDropped_{0};
    std::atomic<int64_t> lastStartLatenessMicroseconds_{0};
    std::atomic<TimingMode> timingMode_{TimingMode::Waiting};

    StreamStats dialogStats_;
    StreamStats bgmStats_;
    RtcpStats dialogRtcpStats_;
    RtcpStats bgmRtcpStats_;
};

} // namespace creatures::audio
