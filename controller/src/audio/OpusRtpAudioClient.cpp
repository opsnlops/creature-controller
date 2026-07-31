#include "audio/OpusRtpAudioClient.h"

#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <limits>
#include <mutex>
#include <optional>
#include <sys/select.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <utility>

#include "audio/AlsaMixerControl.h"
#include "audio/AudioOutputKeepalive.h"
#include "audio/RtcpPacket.h"
#include "audio/RtpPacket.h"
#include "util/thread_name.h"

namespace creatures::audio {
namespace {

using Clock = std::chrono::steady_clock;

struct ReceivedRtpPacket {
    RtpPacket packet;
    Clock::time_point arrival;
};

float decibelsToLinear(float decibels) {
    return std::pow(10.0f, std::clamp(decibels, MIN_GAIN_DB, MAX_GAIN_DB) / 20.0f);
}

bool timestampPrecedes(uint32_t lhs, uint32_t rhs) { return static_cast<int32_t>(lhs - rhs) < 0; }

Clock::duration framesToClockDuration(size_t frames) {
    return std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(static_cast<double>(frames) / SAMPLE_RATE));
}

} // namespace

class OpusRtpAudioClient::RtpJitterBuffer {
  public:
    enum class PushResult {
        Accepted,
        Duplicate,
        Overrun,
        NewSynchronizationSource,
    };

    struct InitialPlayout {
        uint32_t timestamp;
        Clock::time_point startTime;
        uint64_t generation;
        uint32_t synchronizationSource;
        Clock::time_point arrival;
    };

    PushResult push(ReceivedRtpPacket packet) {
        std::lock_guard<std::mutex> lock(mutex_);

        bool synchronizationSourceChanged = false;
        if (!synchronizationSource_.has_value() || *synchronizationSource_ != packet.packet.synchronizationSource) {
            frames_.clear();
            insertionOrder_.clear();
            synchronizationSource_ = packet.packet.synchronizationSource;
            ++generation_;
            synchronizationSourceChanged = true;
        }
        lastArrival_ = packet.arrival;

        if (frames_.contains(packet.packet.timestamp)) {
            return PushResult::Duplicate;
        }

        bool overrun = false;
        while (frames_.size() >= RTP_JITTER_BUFFER_FRAMES) {
            discardOldestLocked();
            overrun = true;
        }

        const uint32_t timestamp = packet.packet.timestamp;
        frames_.emplace(timestamp, std::move(packet));
        insertionOrder_.push_back(timestamp);

        if (synchronizationSourceChanged) {
            return PushResult::NewSynchronizationSource;
        }
        return overrun ? PushResult::Overrun : PushResult::Accepted;
    }

    std::optional<ReceivedRtpPacket> take(uint32_t timestamp) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto frame = frames_.find(timestamp);
        if (frame == frames_.end()) {
            return std::nullopt;
        }

        ReceivedRtpPacket packet = std::move(frame->second);
        frames_.erase(frame);
        return packet;
    }

    std::optional<ReceivedRtpPacket> peek(uint32_t timestamp) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto frame = frames_.find(timestamp);
        if (frame == frames_.end()) {
            return std::nullopt;
        }
        return frame->second;
    }

    bool contains(uint32_t timestamp) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return frames_.contains(timestamp);
    }

    std::optional<InitialPlayout> initialPlayout() {
        std::lock_guard<std::mutex> lock(mutex_);
        discardConsumedOrderEntriesLocked();
        if (insertionOrder_.empty()) {
            return std::nullopt;
        }

        const uint32_t timestamp = insertionOrder_.front();
        const auto frame = frames_.find(timestamp);
        if (frame == frames_.end()) {
            return std::nullopt;
        }

        return InitialPlayout{
            .timestamp = timestamp,
            .startTime = frame->second.arrival + std::chrono::milliseconds(FRAME_MS),
            .generation = generation_,
            .synchronizationSource = *synchronizationSource_,
            .arrival = frame->second.arrival,
        };
    }

    void discardBefore(uint32_t timestamp) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto frame = frames_.begin(); frame != frames_.end();) {
            if (timestampPrecedes(frame->first, timestamp)) {
                frame = frames_.erase(frame);
            } else {
                ++frame;
            }
        }
        discardConsumedOrderEntriesLocked();
    }

    [[nodiscard]] uint64_t generation() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return generation_;
    }

    [[nodiscard]] std::optional<uint32_t> synchronizationSource() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return synchronizationSource_;
    }

    [[nodiscard]] size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return frames_.size();
    }

    [[nodiscard]] bool isIdleFor(std::chrono::milliseconds duration) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return lastArrival_.has_value() && Clock::now() - *lastArrival_ >= duration;
    }

  private:
    void discardOldestLocked() {
        discardConsumedOrderEntriesLocked();
        if (insertionOrder_.empty()) {
            frames_.clear();
            return;
        }
        frames_.erase(insertionOrder_.front());
        insertionOrder_.pop_front();
    }

    void discardConsumedOrderEntriesLocked() {
        while (!insertionOrder_.empty() && !frames_.contains(insertionOrder_.front())) {
            insertionOrder_.pop_front();
        }
    }

    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, ReceivedRtpPacket> frames_;
    std::deque<uint32_t> insertionOrder_;
    std::optional<uint32_t> synchronizationSource_;
    std::optional<Clock::time_point> lastArrival_;
    uint64_t generation_{0};
};

void OpusRtpAudioClient::GainRamp::setTarget(float gain) {
    if (gain == target) {
        return;
    }

    target = gain;
    samplesRemaining = static_cast<size_t>(SAMPLE_RATE) * GAIN_RAMP_MS / 1000;
    if (samplesRemaining == 0) {
        current = target;
        step = 0.0f;
        return;
    }
    step = (target - current) / static_cast<float>(samplesRemaining);
}

float OpusRtpAudioClient::GainRamp::next() {
    if (samplesRemaining > 0) {
        current += step;
        --samplesRemaining;
        if (samplesRemaining == 0) {
            current = target;
        }
    }
    return current;
}

OpusRtpAudioClient::OpusRtpAudioClient(std::shared_ptr<creatures::Logger> log, std::string dialogGroup,
                                       std::string bgmGroup, uint16_t port, uint8_t dialogIndex,
                                       std::string interfaceIp, AudioConfig audioConfig)
    : log_(std::move(log)), dialogGroup_(std::move(dialogGroup)), bgmGroup_(std::move(bgmGroup)),
      interfaceIp_(std::move(interfaceIp)), port_(port), dialogIndex_(dialogIndex),
      audioConfig_(std::move(audioConfig)), dialogBuffer_(std::make_unique<RtpJitterBuffer>()),
      bgmBuffer_(std::make_unique<RtpJitterBuffer>()), dialogGainLinear_(decibelsToLinear(audioConfig_.dialogGainDb)),
      bgmGainLinear_(decibelsToLinear(audioConfig_.bgmGainDb)),
      limiterCeilingLinear_(decibelsToLinear(audioConfig_.limiterCeilingDb)) {
    log_->debug("Created RTP audio client: dialog={}, BGM={}, port={}, channel={}", dialogGroup_, bgmGroup_, port_,
                dialogIndex_);
}

OpusRtpAudioClient::~OpusRtpAudioClient() { shutdown(); }

std::optional<std::chrono::milliseconds> OpusRtpAudioClient::getLastPacketAge() const {
    const int64_t lastArrival = lastPacketArrivalNanoseconds_.load();
    if (lastArrival == 0) {
        return std::nullopt;
    }
    const int64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::nanoseconds(std::max<int64_t>(0, now - lastArrival)));
}

std::optional<std::chrono::milliseconds> OpusRtpAudioClient::getLastRtcpReportAge() const {
    const auto synchronizationSource = bgmBuffer_->synchronizationSource();
    if (!synchronizationSource.has_value()) {
        return std::nullopt;
    }
    const auto report = bgmRtcpReports_.find(*synchronizationSource);
    if (!report.has_value()) {
        return std::nullopt;
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::max(Clock::duration::zero(), Clock::now() - report->receivedAt));
}

uint64_t OpusRtpAudioClient::getRtcpReportsReceived() const {
    return dialogRtcpStats_.reportsReceived.load() + bgmRtcpStats_.reportsReceived.load();
}

uint64_t OpusRtpAudioClient::getRtcpInvalidReports() const {
    return dialogRtcpStats_.invalidReports.load() + bgmRtcpStats_.invalidReports.load();
}

const char *OpusRtpAudioClient::getTimingModeName() const {
    switch (timingMode_.load()) {
    case TimingMode::Waiting:
        return "waiting";
    case TimingMode::Rtcp:
        return "rtcp";
    case TimingMode::ArrivalFallback:
        return "arrival_fallback";
    }
    return "unknown";
}

bool OpusRtpAudioClient::isReceiving() const {
    const auto packetAge = getLastPacketAge();
    return running_.load() && packetAge.has_value() && *packetAge < std::chrono::seconds(1);
}

void OpusRtpAudioClient::start() {
    if (mainThread_.joinable()) {
        log_->warn("RTP audio client is already started");
        return;
    }
    stop_requested.store(false);
    mainThread_ = std::thread(&OpusRtpAudioClient::run, this);
}

void OpusRtpAudioClient::shutdown() {
    stop_requested.store(true);
    if (mainThread_.joinable() && mainThread_.get_id() != std::this_thread::get_id()) {
        mainThread_.join();
    }
}

void OpusRtpAudioClient::setDialogGainDb(float gainDb) {
    const float clampedGain = std::clamp(gainDb, MIN_GAIN_DB, MAX_GAIN_DB);
    dialogGainLinear_.store(decibelsToLinear(clampedGain));
    log_->info("Dialog gain set to {:.1f} dB", clampedGain);
}

void OpusRtpAudioClient::setBgmGainDb(float gainDb) {
    const float clampedGain = std::clamp(gainDb, MIN_GAIN_DB, MAX_GAIN_DB);
    bgmGainLinear_.store(decibelsToLinear(clampedGain));
    log_->info("BGM gain set to {:.1f} dB", clampedGain);
}

void OpusRtpAudioClient::run() {
    setThreadName("opus-rtp-main");

    if (!initializeAudioDevice() || !initializeDecoders() || !openSockets()) {
        releaseResources();
        return;
    }

    [[maybe_unused]] const bool volumeApplied = AlsaMixerControl::applyConfiguredVolume(log_, audioConfig_);

    running_.store(true);
    dialogThread_ = std::thread(&OpusRtpAudioClient::receiveStream, this, dialogSocket_, std::ref(*dialogBuffer_),
                                std::ref(dialogStats_), "Dialog");
    bgmThread_ = std::thread(&OpusRtpAudioClient::receiveStream, this, bgmSocket_, std::ref(*bgmBuffer_),
                             std::ref(bgmStats_), "BGM");
    if (dialogRtcpSocket_ >= 0) {
        dialogRtcpThread_ = std::thread(&OpusRtpAudioClient::receiveRtcpStream, this, dialogRtcpSocket_,
                                        std::ref(dialogRtcpReports_), std::ref(dialogRtcpStats_), "Dialog");
    }
    if (bgmRtcpSocket_ >= 0) {
        bgmRtcpThread_ = std::thread(&OpusRtpAudioClient::receiveRtcpStream, this, bgmRtcpSocket_,
                                     std::ref(bgmRtcpReports_), std::ref(bgmRtcpStats_), "BGM");
    }
    playoutThread_ = std::thread(&OpusRtpAudioClient::audioPlayoutThread, this);

    log_->info("RTP audio client running with {:.1f} dB dialog gain, {:.1f} dB BGM gain, {} ms common playout "
               "delay, and {:+d} ms device compensation",
               audioConfig_.dialogGainDb, audioConfig_.bgmGainDb, audioConfig_.commonPlayoutDelayMs,
               audioConfig_.audioDeviceCompensationMs);

    while (!stop_requested.load()) {
        constexpr float targetFrames = static_cast<float>(TARGET_PLAYOUT_FRAMES * FRAMES_PER_CHUNK);
        const size_t outputQueuedFrames = audioOutput_->queuedFrames();
        outputQueuedFrames_.store(outputQueuedFrames);
        dialogBufferedFrames_.store(dialogBuffer_->size());
        bgmBufferedFrames_.store(bgmBuffer_->size());
        bufferLevel_.store(std::clamp(static_cast<float>(outputQueuedFrames) / targetFrames, 0.0f, 1.0f));
        totalPackets_.store(dialogStats_.packetsReceived.load() + bgmStats_.packetsReceived.load());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    joinWorkerThreads();
    running_.store(false);
    releaseResources();
    log_->info("RTP audio client stopped");
}

bool OpusRtpAudioClient::initializeAudioDevice() {
    audioOutput_ = createAudioOutput(log_);
    return audioOutput_ != nullptr && audioOutput_->open(audioConfig_);
}

bool OpusRtpAudioClient::initializeDecoders() {
    int error = OPUS_OK;
    dialogDecoder_ = opus_decoder_create(SAMPLE_RATE, OUTPUT_CH, &error);
    if (dialogDecoder_ == nullptr || error != OPUS_OK) {
        log_->error("Unable to create dialog Opus decoder: {}", opus_strerror(error));
        return false;
    }

    bgmDecoder_ = opus_decoder_create(SAMPLE_RATE, OUTPUT_CH, &error);
    if (bgmDecoder_ == nullptr || error != OPUS_OK) {
        log_->error("Unable to create BGM Opus decoder: {}", opus_strerror(error));
        return false;
    }
    return true;
}

bool OpusRtpAudioClient::openSockets() {
    if (!openSocket(dialogSocket_, dialogGroup_, port_, "RTP")) {
        return false;
    }
    if (!openSocket(bgmSocket_, bgmGroup_, port_, "RTP")) {
        return false;
    }

    if (port_ == std::numeric_limits<uint16_t>::max()) {
        log_->warn("RTCP reception disabled because RTP port {} has no adjacent UDP port", port_);
        return true;
    }

    const uint16_t rtcpPort = static_cast<uint16_t>(port_ + 1U);
    if (!openSocket(dialogRtcpSocket_, dialogGroup_, rtcpPort, "RTCP")) {
        log_->warn("Dialog RTCP timing is unavailable; audio will use arrival-time fallback");
    }
    if (!openSocket(bgmRtcpSocket_, bgmGroup_, rtcpPort, "RTCP")) {
        log_->warn("BGM RTCP timing is unavailable; audio will use arrival-time fallback");
    }
    return true;
}

void OpusRtpAudioClient::joinWorkerThreads() {
    if (dialogThread_.joinable()) {
        dialogThread_.join();
    }
    if (bgmThread_.joinable()) {
        bgmThread_.join();
    }
    if (dialogRtcpThread_.joinable()) {
        dialogRtcpThread_.join();
    }
    if (bgmRtcpThread_.joinable()) {
        bgmRtcpThread_.join();
    }
    if (playoutThread_.joinable()) {
        playoutThread_.join();
    }
}

void OpusRtpAudioClient::releaseResources() {
    bufferLevel_.store(0.0f);
    outputQueuedFrames_.store(0);
    dialogBufferedFrames_.store(0);
    bgmBufferedFrames_.store(0);

    if (audioOutput_ != nullptr) {
        audioOutput_->stopAndClear();
        audioOutput_->close();
        audioOutput_.reset();
    }
    if (dialogSocket_ >= 0) {
        close(dialogSocket_);
        dialogSocket_ = -1;
    }
    if (bgmSocket_ >= 0) {
        close(bgmSocket_);
        bgmSocket_ = -1;
    }
    if (dialogRtcpSocket_ >= 0) {
        close(dialogRtcpSocket_);
        dialogRtcpSocket_ = -1;
    }
    if (bgmRtcpSocket_ >= 0) {
        close(bgmRtcpSocket_);
        bgmRtcpSocket_ = -1;
    }
    if (dialogDecoder_ != nullptr) {
        opus_decoder_destroy(dialogDecoder_);
        dialogDecoder_ = nullptr;
    }
    if (bgmDecoder_ != nullptr) {
        opus_decoder_destroy(bgmDecoder_);
        bgmDecoder_ = nullptr;
    }
}

void OpusRtpAudioClient::receiveStream(int socket, RtpJitterBuffer &buffer, StreamStats &stats,
                                       const std::string &streamName) {
    setThreadName(streamName == "Dialog" ? "opus-dialog-rx" : "opus-bgm-rx");
    std::vector<uint8_t> packet(MAX_RTP_PACKET_SIZE);

    while (!stop_requested.load()) {
        if (!receivePacket(socket, packet, MAX_RTP_PACKET_SIZE)) {
            continue;
        }

        auto parsed = parseOpusRtpPacket(packet);
        if (!parsed.has_value()) {
            stats.invalidPackets.fetch_add(1);
            continue;
        }
        stats.packetsReceived.fetch_add(1);
        lastPacketArrivalNanoseconds_.store(
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());

        const uint32_t synchronizationSource = parsed->synchronizationSource;
        switch (buffer.push(ReceivedRtpPacket{std::move(*parsed), Clock::now()})) {
        case RtpJitterBuffer::PushResult::Accepted:
            break;
        case RtpJitterBuffer::PushResult::Duplicate:
            stats.duplicatePackets.fetch_add(1);
            break;
        case RtpJitterBuffer::PushResult::Overrun:
            stats.bufferOverruns.fetch_add(1);
            break;
        case RtpJitterBuffer::PushResult::NewSynchronizationSource:
            log_->info("{} RTP stream started with SSRC {}", streamName, synchronizationSource);
            break;
        }
    }

    log_->info("{} receiver stopped: packets={}, invalid={}, duplicates={}, overruns={}", streamName,
               stats.packetsReceived.load(), stats.invalidPackets.load(), stats.duplicatePackets.load(),
               stats.bufferOverruns.load());
}

void OpusRtpAudioClient::receiveRtcpStream(int socket, RtcpReportCache &reports, RtcpStats &stats,
                                           const std::string &streamName) {
    setThreadName(streamName == "Dialog" ? "rtcp-dialog-rx" : "rtcp-bgm-rx");
    std::vector<uint8_t> packet(MAX_RTCP_PACKET_SIZE);

    while (!stop_requested.load()) {
        if (!receivePacket(socket, packet, MAX_RTCP_PACKET_SIZE)) {
            continue;
        }

        auto report = parseRtcpSenderReport(packet);
        if (!report.has_value()) {
            stats.invalidReports.fetch_add(1);
            continue;
        }

        log_->debug("{} RTCP sender report received: SSRC {}, CNAME '{}', RTP timestamp {}, NTP {}.{}", streamName,
                    report->synchronizationSource, report->canonicalName, report->rtpTimestamp,
                    static_cast<uint32_t>(report->ntpTimestamp >> 32U), static_cast<uint32_t>(report->ntpTimestamp));
        stats.reportsReceived.fetch_add(1);
        if (reports.store(std::move(*report), Clock::now())) {
            stats.cacheEvictions.fetch_add(1);
        }
    }

    log_->info("{} RTCP receiver stopped: reports={}, invalid={}, cache_evictions={}, source_mismatches={}, stale={}",
               streamName, stats.reportsReceived.load(), stats.invalidReports.load(), stats.cacheEvictions.load(),
               stats.sourceMismatches.load(), stats.staleReports.load());
}

bool OpusRtpAudioClient::decodeTimestamp(RtpJitterBuffer &buffer, OpusDecoder *decoder, uint32_t timestamp,
                                         std::array<int16_t, FRAMES_PER_CHUNK> &samples, StreamStats &stats,
                                         uint64_t &observedGeneration, const char *streamName) {
    const uint64_t generation = buffer.generation();
    if (generation != observedGeneration) {
        opus_decoder_ctl(decoder, OPUS_RESET_STATE);
        observedGeneration = generation;
        log_->debug("{} decoder reset for RTP generation {}", streamName, generation);
    }

    int decodedSamples = 0;
    if (auto packet = buffer.take(timestamp); packet.has_value()) {
        decodedSamples =
            opus_decode(decoder, packet->packet.payload.data(), static_cast<opus_int32>(packet->packet.payload.size()),
                        samples.data(), FRAMES_PER_CHUNK, 0);
        if (decodedSamples >= 0) {
            stats.decodedFrames.fetch_add(1);
        }
    } else if (auto nextPacket = buffer.peek(timestamp + FRAMES_PER_CHUNK); nextPacket.has_value()) {
        decodedSamples = opus_decode(decoder, nextPacket->packet.payload.data(),
                                     static_cast<opus_int32>(nextPacket->packet.payload.size()), samples.data(),
                                     FRAMES_PER_CHUNK, 1);
        if (decodedSamples >= 0) {
            stats.fecFrames.fetch_add(1);
        }
    } else {
        decodedSamples = opus_decode(decoder, nullptr, 0, samples.data(), FRAMES_PER_CHUNK, 0);
        if (decodedSamples >= 0) {
            stats.concealedFrames.fetch_add(1);
        }
    }

    if (decodedSamples < 0) {
        samples.fill(0);
        stats.decodeErrors.fetch_add(1);
        log_->warn("{} Opus decode failed at RTP timestamp {}: {}", streamName, timestamp,
                   opus_strerror(decodedSamples));
        return false;
    }
    if (decodedSamples < FRAMES_PER_CHUNK) {
        std::fill(samples.begin() + decodedSamples, samples.end(), 0);
    }
    return true;
}

void OpusRtpAudioClient::mixTimestamp(uint32_t timestamp, std::array<int16_t, FRAMES_PER_CHUNK> &mixed,
                                      uint64_t &dialogGeneration, uint64_t &bgmGeneration, GainRamp &dialogGain,
                                      GainRamp &bgmGain) {
    std::array<int16_t, FRAMES_PER_CHUNK> dialog{};
    std::array<int16_t, FRAMES_PER_CHUNK> bgm{};
    decodeTimestamp(*dialogBuffer_, dialogDecoder_, timestamp, dialog, dialogStats_, dialogGeneration, "Dialog");
    decodeTimestamp(*bgmBuffer_, bgmDecoder_, timestamp, bgm, bgmStats_, bgmGeneration, "BGM");

    dialogGain.setTarget(dialogGainLinear_.load());
    bgmGain.setTarget(bgmGainLinear_.load());

    constexpr float int16Scale = 32768.0f;
    for (size_t sampleIndex = 0; sampleIndex < FRAMES_PER_CHUNK; ++sampleIndex) {
        const float dialogSample = static_cast<float>(dialog[sampleIndex]) / int16Scale;
        const float bgmSample = static_cast<float>(bgm[sampleIndex]) / int16Scale;
        const float sample = std::clamp(dialogSample * dialogGain.next() + bgmSample * bgmGain.next(),
                                        -limiterCeilingLinear_, limiterCeilingLinear_);
        mixed[sampleIndex] = static_cast<int16_t>(std::lround(sample * 32767.0f));
    }
}

void OpusRtpAudioClient::audioPlayoutThread() {
    setThreadName("opus-playout");

    constexpr size_t targetQueueFrames = TARGET_PLAYOUT_FRAMES * FRAMES_PER_CHUNK;
    constexpr auto idleTimeout = std::chrono::milliseconds(STREAM_IDLE_TIMEOUT_MS);
    const size_t idleQueueFrames =
        rtcpIdleQueueFrames(audioConfig_.commonPlayoutDelayMs, audioConfig_.audioDeviceCompensationMs);
    AudioOutputKeepalive outputKeepalive(*audioOutput_, idleQueueFrames);

    GainRamp dialogGain(dialogGainLinear_.load());
    GainRamp bgmGain(bgmGainLinear_.load());
    uint64_t dialogGeneration = 0;
    uint64_t bgmDecoderGeneration = 0;
    uint64_t activeBgmGeneration = 0;
    uint64_t arrivalFallbackGeneration = 0;
    uint64_t lastSummaryFrame = 0;

    if (!outputKeepalive.start()) {
        log_->error("Unable to start the audio output keepalive");
        stop_requested.store(true);
        return;
    }
    const double idleQueueMilliseconds = static_cast<double>(idleQueueFrames) * 1000.0 / SAMPLE_RATE;
    const double enqueueHeadroomMilliseconds = static_cast<double>(audioConfig_.commonPlayoutDelayMs) -
                                               idleQueueMilliseconds -
                                               std::max<int>(0, audioConfig_.audioDeviceCompensationMs);
    log_->info("Audio output keepalive started with {:.1f} ms queued ({:.1f} ms RTCP enqueue headroom)",
               static_cast<double>(audioOutput_->queuedFrames()) * 1000.0 / SAMPLE_RATE, enqueueHeadroomMilliseconds);

    while (!stop_requested.load()) {
        timingMode_.store(TimingMode::Waiting);
        const auto initial = bgmBuffer_->initialPlayout();
        if (!initial.has_value()) {
            if (!outputKeepalive.refillSilence()) {
                log_->error("Unable to maintain idle audio output");
                stop_requested.store(true);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        uint32_t nextTimestamp = initial->timestamp;
        const auto now = Clock::now();
        const auto dialogSynchronizationSource = dialogBuffer_->synchronizationSource();
        const auto bgmReport = bgmRtcpReports_.find(initial->synchronizationSource);
        const auto dialogReport = dialogSynchronizationSource.has_value()
                                      ? dialogRtcpReports_.find(*dialogSynchronizationSource)
                                      : std::optional<TimedRtcpSenderReport>{};

        const auto reportIsFresh = [now](const std::optional<TimedRtcpSenderReport> &report) {
            return report.has_value() && now - report->receivedAt <= std::chrono::milliseconds(RTCP_MAX_REPORT_AGE_MS);
        };

        const bool freshReports = reportIsFresh(bgmReport) && reportIsFresh(dialogReport);
        const bool compatibleReports =
            freshReports && rtcpClockMappingsCompatible(bgmReport->report, dialogReport->report,
                                                        std::chrono::microseconds(RTCP_CLOCK_COMPATIBILITY_US));

        std::optional<RtcpPlayoutPlanner> rtcpPlanner;
        std::optional<RtcpPlayoutPlan> initialRtcpPlan;
        bool rtcpDeadlineIsPlausible = false;
        int64_t mappingArrivalDeltaMicroseconds = 0;
        if (compatibleReports) {
            rtcpPlanner.emplace(RtcpClockPair::capture(), std::chrono::milliseconds(audioConfig_.commonPlayoutDelayMs),
                                std::chrono::milliseconds(audioConfig_.audioDeviceCompensationMs));
            initialRtcpPlan = rtcpPlanner->plan(bgmReport->report, nextTimestamp, audioOutput_->queuedFrames());
            if (initialRtcpPlan.has_value()) {
                const auto expectedPresentation =
                    initial->arrival + std::chrono::milliseconds(audioConfig_.commonPlayoutDelayMs);
                mappingArrivalDeltaMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(
                                                      initialRtcpPlan->presentationDeadline - expectedPresentation)
                                                      .count();
                rtcpDeadlineIsPlausible = rtcpPresentationDeadlinePlausible(
                    *initialRtcpPlan, initial->arrival, std::chrono::milliseconds(audioConfig_.commonPlayoutDelayMs),
                    std::chrono::milliseconds(RTCP_MAX_DEADLINE_DISTANCE_MS));
            }
        }

        const bool generationLockedToFallback = initial->generation == arrivalFallbackGeneration;
        const bool useRtcp =
            !generationLockedToFallback && compatibleReports && initialRtcpPlan.has_value() && rtcpDeadlineIsPlausible;
        if (!generationLockedToFallback && !useRtcp &&
            now - initial->arrival < std::chrono::milliseconds(RTCP_STARTUP_WAIT_MS)) {
            if (!outputKeepalive.refillSilence()) {
                log_->error("Unable to maintain audio output while waiting for RTCP");
                stop_requested.store(true);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        std::array<int16_t, FRAMES_PER_CHUNK> initialMixed{};
        bool initialDecoded = false;
        std::optional<RtcpPlayoutPlan> finalRtcpPlan;
        if (useRtcp) {
            mixTimestamp(nextTimestamp, initialMixed, dialogGeneration, bgmDecoderGeneration, dialogGain, bgmGain);
            initialDecoded = true;

            bool retryWithNextTimestamp = false;
            while (!stop_requested.load() && bgmBuffer_->generation() == initial->generation) {
                if (!outputKeepalive.refillSilence()) {
                    log_->error("Unable to maintain audio output before RTCP playout");
                    stop_requested.store(true);
                    break;
                }

                finalRtcpPlan = rtcpPlanner->plan(bgmReport->report, nextTimestamp, audioOutput_->queuedFrames());
                if (!finalRtcpPlan.has_value()) {
                    break;
                }

                const auto enqueueNow = Clock::now();
                const auto enqueueState = classifyRtcpEnqueue(*finalRtcpPlan, enqueueNow,
                                                              std::chrono::microseconds(RTCP_START_LATE_TOLERANCE_US));
                if (enqueueState == RtcpEnqueueState::Missed) {
                    const auto enqueueLateness = std::chrono::duration_cast<std::chrono::microseconds>(
                        enqueueNow - finalRtcpPlan->enqueueDeadline);
                    rtcpLateFramesDropped_.fetch_add(1);
                    nextTimestamp += FRAMES_PER_CHUNK;
                    dialogBuffer_->discardBefore(nextTimestamp);
                    bgmBuffer_->discardBefore(nextTimestamp);
                    log_->debug("Skipped late RTCP startup frame; next RTP timestamp {}, enqueue deadline missed by "
                                "{} us",
                                nextTimestamp, enqueueLateness.count());
                    retryWithNextTimestamp = true;
                    break;
                }
                if (enqueueState == RtcpEnqueueState::Ready) {
                    break;
                }

                const auto remaining = finalRtcpPlan->enqueueDeadline - enqueueNow;
                std::this_thread::sleep_for(
                    std::min(remaining, std::chrono::duration_cast<Clock::duration>(std::chrono::milliseconds(1))));
            }

            if (stop_requested.load()) {
                break;
            }
            if (retryWithNextTimestamp || bgmBuffer_->generation() != initial->generation) {
                continue;
            }
            if (!finalRtcpPlan.has_value()) {
                log_->warn("RTCP timing calculation failed for BGM generation {}; using arrival-time fallback",
                           initial->generation);
            } else {
                timingMode_.store(TimingMode::Rtcp);
                log_->info("Audio timing mode RTCP selected for BGM SSRC {} and dialog SSRC {} (CNAME '{}', "
                           "mapping NTP {}.{} to RTP {}, delay={} ms, compensation={:+d} ms, "
                           "mapping/arrival delta={:+d} us)",
                           initial->synchronizationSource, *dialogSynchronizationSource,
                           bgmReport->report.canonicalName,
                           static_cast<uint32_t>(bgmReport->report.ntpTimestamp >> 32U),
                           static_cast<uint32_t>(bgmReport->report.ntpTimestamp), bgmReport->report.rtpTimestamp,
                           audioConfig_.commonPlayoutDelayMs, audioConfig_.audioDeviceCompensationMs,
                           mappingArrivalDeltaMicroseconds);
            }
        }

        if (!finalRtcpPlan.has_value()) {
            timingMode_.store(TimingMode::ArrivalFallback);
            if (!generationLockedToFallback) {
                arrivalFallbackGeneration = initial->generation;
                rtcpFallbacks_.fetch_add(1);

                if (!bgmReport.has_value() && !bgmRtcpReports_.empty()) {
                    bgmRtcpStats_.sourceMismatches.fetch_add(1);
                }
                if (!dialogReport.has_value() && !dialogRtcpReports_.empty()) {
                    dialogRtcpStats_.sourceMismatches.fetch_add(1);
                }
                if (bgmReport.has_value() && !reportIsFresh(bgmReport)) {
                    bgmRtcpStats_.staleReports.fetch_add(1);
                }
                if (dialogReport.has_value() && !reportIsFresh(dialogReport)) {
                    dialogRtcpStats_.staleReports.fetch_add(1);
                }

                log_->warn("RTCP timing unavailable for BGM generation {}; using arrival-time fallback "
                           "(BGM report={}, dialog report={}, fresh={}, compatible={}, plan={}, plausible={}, "
                           "mapping/arrival delta={:+d} us, output queue={:.1f} ms, BGM cache={}, dialog cache={})",
                           initial->generation, bgmReport.has_value() ? "yes" : "no",
                           dialogReport.has_value() ? "yes" : "no", freshReports ? "yes" : "no",
                           compatibleReports ? "yes" : "no", initialRtcpPlan.has_value() ? "yes" : "no",
                           rtcpDeadlineIsPlausible ? "yes" : "no", mappingArrivalDeltaMicroseconds,
                           static_cast<double>(audioOutput_->queuedFrames()) * 1000.0 / SAMPLE_RATE,
                           bgmRtcpReports_.size(), dialogRtcpReports_.size());
            }
            while (!stop_requested.load() && bgmBuffer_->generation() == initial->generation &&
                   Clock::now() < initial->startTime) {
                if (!outputKeepalive.refillSilence()) {
                    log_->error("Unable to maintain audio output before fallback playout");
                    stop_requested.store(true);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (stop_requested.load()) {
                break;
            }
            if (bgmBuffer_->generation() != initial->generation) {
                continue;
            }
            if (!initialDecoded) {
                mixTimestamp(nextTimestamp, initialMixed, dialogGeneration, bgmDecoderGeneration, dialogGain, bgmGain);
            }
        }

        activeBgmGeneration = initial->generation;
        dialogBuffer_->discardBefore(nextTimestamp);
        bgmBuffer_->discardBefore(nextTimestamp);

        const size_t queuedBeforeInitialWrite = audioOutput_->queuedFrames();
        const auto initialWriteTime = Clock::now();
        if (!audioOutput_->write(initialMixed)) {
            log_->error("Unable to queue initial audio frame");
            stop_requested.store(true);
            break;
        }
        if (finalRtcpPlan.has_value()) {
            const auto predictedPresentation = initialWriteTime + framesToClockDuration(queuedBeforeInitialWrite) +
                                               std::chrono::milliseconds(audioConfig_.audioDeviceCompensationMs);
            const int64_t startLateness = std::chrono::duration_cast<std::chrono::microseconds>(
                                              predictedPresentation - finalRtcpPlan->presentationDeadline)
                                              .count();
            lastStartLatenessMicroseconds_.store(startLateness);
            log_->info("RTCP playout started at RTP timestamp {} from report RTP timestamp {} (start lateness {:+d} "
                       "us, {} frames queued before write)",
                       initial->timestamp, bgmReport->report.rtpTimestamp, startLateness, queuedBeforeInitialWrite);
        } else {
            lastStartLatenessMicroseconds_.store(0);
            log_->info("Arrival-time playout started at RTP timestamp {} with {:.1f} ms queued", initial->timestamp,
                       static_cast<double>(audioOutput_->queuedFrames()) * 1000.0 / SAMPLE_RATE);
        }

        nextTimestamp += FRAMES_PER_CHUNK;
        mixedFrames_.fetch_add(1);

        while (!stop_requested.load() && bgmBuffer_->generation() == activeBgmGeneration &&
               !bgmBuffer_->isIdleFor(idleTimeout)) {
            const size_t queuedFrames = audioOutput_->queuedFrames();
            if (queuedFrames >= targetQueueFrames) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (!bgmBuffer_->contains(nextTimestamp) && queuedFrames >= FRAMES_PER_CHUNK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (!bgmBuffer_->contains(nextTimestamp)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(PACKET_WAIT_MS));
            }

            if (audioOutput_->queuedFrames() == 0) {
                playoutDeadlineMisses_.fetch_add(1);
            }

            std::array<int16_t, FRAMES_PER_CHUNK> mixed{};
            mixTimestamp(nextTimestamp, mixed, dialogGeneration, bgmDecoderGeneration, dialogGain, bgmGain);
            if (!audioOutput_->write(mixed)) {
                log_->error("Unable to queue audio frame");
                stop_requested.store(true);
                break;
            }

            nextTimestamp += FRAMES_PER_CHUNK;
            dialogBuffer_->discardBefore(nextTimestamp);
            bgmBuffer_->discardBefore(nextTimestamp);
            const uint64_t frameCount = mixedFrames_.fetch_add(1) + 1;

            if (frameCount % MIX_STATS_FRAME_INTERVAL == 0) {
                log_->debug("Audio playout: frames={}, queued={:.1f} ms, timing={}, deadline_misses={}, "
                            "RTCP_late_drops={}, underruns={}, dialog fec/plc/errors={}/{}/{}, BGM "
                            "fec/plc/errors={}/{}/{}",
                            frameCount, static_cast<double>(audioOutput_->queuedFrames()) * 1000.0 / SAMPLE_RATE,
                            getTimingModeName(), playoutDeadlineMisses_.load(), rtcpLateFramesDropped_.load(),
                            audioOutput_->underruns(), dialogStats_.fecFrames.load(),
                            dialogStats_.concealedFrames.load(), dialogStats_.decodeErrors.load(),
                            bgmStats_.fecFrames.load(), bgmStats_.concealedFrames.load(),
                            bgmStats_.decodeErrors.load());
            }

            if (frameCount - lastSummaryFrame >= MIX_SUMMARY_FRAME_INTERVAL) {
                lastSummaryFrame = frameCount;
                log_->info("Audio clock stable: {} frames played, {} deadline misses, {} output underruns", frameCount,
                           playoutDeadlineMisses_.load(), audioOutput_->underruns());
            }
        }

        log_->debug("Audio playout returned to idle keepalive");
    }
}

bool OpusRtpAudioClient::openSocket(int &socket, const std::string &group, uint16_t port, const char *protocol) const {
    socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket < 0) {
        log_->error("Unable to create {} socket for {}: {}", protocol, group, std::strerror(errno));
        return false;
    }

    int reuseAddress = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress)) < 0) {
        log_->warn("Unable to enable SO_REUSEADDR for {}: {}", group, std::strerror(errno));
    }

    int receiveBufferSize = 256 * 1024;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &receiveBufferSize, sizeof(receiveBufferSize)) < 0) {
        log_->warn("Unable to enlarge the {} receive buffer for {}: {}", protocol, group, std::strerror(errno));
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(group.c_str());
    address.sin_port = htons(port);
    if (bind(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        log_->error("Unable to bind {} socket to {}:{}: {}", protocol, group, port, std::strerror(errno));
        close(socket);
        socket = -1;
        return false;
    }

    ip_mreq membership{};
    membership.imr_multiaddr.s_addr = inet_addr(group.c_str());
    membership.imr_interface.s_addr = inet_addr(interfaceIp_.c_str());
    if (setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &membership, sizeof(membership)) < 0) {
        log_->error("Unable to join {} multicast group {} on {}: {}", protocol, group, interfaceIp_,
                    std::strerror(errno));
        close(socket);
        socket = -1;
        return false;
    }

    const int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0 || fcntl(socket, F_SETFL, flags | O_NONBLOCK) < 0) {
        log_->error("Unable to make {} socket non-blocking for {}: {}", protocol, group, std::strerror(errno));
        close(socket);
        socket = -1;
        return false;
    }

    log_->info("Joined {} multicast group {} on {}:{}", protocol, group, interfaceIp_, port);
    return true;
}

bool OpusRtpAudioClient::receivePacket(int socket, std::vector<uint8_t> &packet, size_t maximumSize) {
    fd_set readSockets;
    FD_ZERO(&readSockets);
    FD_SET(socket, &readSockets);

    timeval timeout{0, 1000};
    const int selectResult = select(socket + 1, &readSockets, nullptr, nullptr, &timeout);
    if (selectResult <= 0 || !FD_ISSET(socket, &readSockets)) {
        return false;
    }

    packet.resize(maximumSize + 1U);
    const ssize_t bytesReceived = recv(socket, packet.data(), packet.size(), 0);
    if (bytesReceived <= 0) {
        return false;
    }
    if (static_cast<size_t>(bytesReceived) > maximumSize) {
        packet.clear();
        return false;
    }
    packet.resize(static_cast<size_t>(bytesReceived));
    return true;
}

} // namespace creatures::audio
