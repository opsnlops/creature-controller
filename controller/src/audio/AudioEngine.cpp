#include "audio/AudioEngine.h"
#include "audio/AlsaMixerControl.h"
#include "util/thread_name.h"
#include <algorithm>
#include <cmath>
#include <fmt/format.h>
#include <stdexcept>

namespace creatures::audio {
namespace {
using Clock = std::chrono::steady_clock;
Clock::duration framesToClockDuration(size_t frames) {
    return std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(static_cast<double>(frames) / SAMPLE_RATE));
}
} // namespace

AudioEngine::InputStream::InputStream(InputConfig config, size_t bufferCapacity)
    : channel(config.channel), name(config.channel == 17 ? "BGM" : fmt::format("Bird {}", config.channel)),
      group(fmt::format("{}{}", DIALOG_GROUP_BASE, config.channel)), buffer(bufferCapacity),
      gain(gainFromDecibels(config.gainDb)) {}

AudioEngine::AudioEngine(std::shared_ptr<Logger> log, AudioEngineConfig config, std::unique_ptr<AudioOutput> output,
                         PacketReceiverFactory receivers)
    : log_(std::move(log)), config_(std::move(config)), audioConfig_(config_.output), interfaceIp_(config_.interfaceIp),
      port_(config_.port), audioOutput_(std::move(output)), receiverFactory_(std::move(receivers)), mixer_(config_),
      decoded_(config_.inputs.size()), gains_(config_.inputs.size()), decodedGenerations_(config_.inputs.size()) {
    if (!receiverFactory_)
        receiverFactory_ = [this] { return createUdpPacketReceiver(log_); };
    if (!log_)
        throw std::invalid_argument("Audio engine requires a logger");
    for (const auto &input : config_.inputs) {
        if (input.channel == 17)
            masterIndex_ = streams_.size();
        streams_.push_back(std::make_unique<InputStream>(input, RTP_JITTER_BUFFER_FRAMES +
                                                                    audioConfig_.commonPlayoutDelayMs / FRAME_MS));
    }
}
AudioEngine::~AudioEngine() { shutdown(); }

bool AudioEngine::start() {
    std::lock_guard lock(lifecycleMutex_);
    if (state_.load() != State::Stopped)
        return false;
    for (auto &stream : streams_) {
        stream->buffer.clear();
        stream->reports.clear();
        stream->decoderGeneration = 0;
        stream->qualifiedGeneration = 0;
    }
    mixer_ = RoutingMixer(config_);
    activeReport_.reset();
    lastPacketArrivalNanoseconds_.store(0);
    timingMode_.store(TimingMode::Waiting);
    state_.store(State::Starting);
    stop_requested.store(false);
    try {
        if (!audioOutput_)
            audioOutput_ = createAudioOutput(log_);
        if (!audioOutput_ || !audioOutput_->open(audioConfig_) || !initializeDecoders() || !openSockets()) {
            releaseResources();
            state_.store(State::Failed);
            return false;
        }
        const size_t idleFrames =
            rtcpIdleQueueFrames(audioConfig_.commonPlayoutDelayMs, audioConfig_.audioDeviceCompensationMs,
                                audioOutput_->pipelineLatencyFrames());
        if (audioOutput_->pipelineLatencyFrames() >= idleFrames) {
            log_->error("Output pipeline latency exceeds playout headroom; increase commonPlayoutDelayMs on all "
                        "synchronized nodes");
            releaseResources();
            state_.store(State::Failed);
            return false;
        }
        keepalive_ = std::make_unique<AudioOutputKeepalive>(*audioOutput_, idleFrames, audioConfig_.channels.size());
        if (!keepalive_->start()) {
            releaseResources();
            state_.store(State::Failed);
            return false;
        }
        (void)AlsaMixerControl::applyConfiguredVolume(log_, audioConfig_);
        state_.store(State::Running);
        for (auto &stream : streams_) {
            stream->receiver = std::thread(&AudioEngine::receiveStream, this, std::ref(*stream));
            if (stream->rtcpReceiverSource)
                stream->rtcpReceiver = std::thread(&AudioEngine::receiveRtcpStream, this, std::ref(*stream));
        }
        playoutThread_ = std::thread([this] {
            try {
                audioPlayoutThread();
            } catch (const std::exception &error) {
                log_->error("Audio playout failed: {}", error.what());
                state_.store(State::Failed);
                stop_requested.store(true);
            }
        });
        mainThread_ = std::thread(&AudioEngine::run, this);
        return true;
    } catch (const std::exception &error) {
        log_->error("Audio startup failed: {}", error.what());
        stop_requested.store(true);
        joinWorkerThreads();
        releaseResources();
        state_.store(State::Failed);
        return false;
    }
}
void AudioEngine::shutdown() {
    std::lock_guard lock(lifecycleMutex_);
    const bool failed = state_.load() == State::Failed;
    if (!failed)
        state_.store(State::Stopping);
    stop_requested.store(true);
    if (mainThread_.joinable())
        mainThread_.join();
    if (!failed && state_.load() != State::Failed)
        state_.store(State::Stopped);
}
void AudioEngine::setInputGainDb(uint8_t channel, float gainDb) {
    if (!std::isfinite(gainDb))
        throw std::invalid_argument("Audio gain must be finite");
    for (auto &stream : streams_) {
        if (stream->channel == channel) {
            stream->gain.store(gainFromDecibels(std::clamp(gainDb, MIN_GAIN_DB, MAX_GAIN_DB)));
            return;
        }
    }
    throw std::invalid_argument("Audio input channel is not subscribed");
}

void AudioEngine::run() {
    setThreadName("audio-monitor");
    while (!stop_requested.load()) {
        const size_t queued = audioOutput_->queuedFrames();
        outputQueuedFrames_.store(queued);
        bufferLevel_.store(
            std::clamp(static_cast<float>(queued) / (TARGET_PLAYOUT_FRAMES * FRAMES_PER_CHUNK), 0.0f, 1.0f));
        uint64_t packets = 0;
        for (const auto &stream : streams_)
            packets += stream->stats.packetsReceived.load();
        totalPackets_.store(packets);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    joinWorkerThreads();
    releaseResources();
}
bool AudioEngine::initializeDecoders() {
    for (auto &stream : streams_) {
        int error = OPUS_OK;
        stream->decoder = opus_decoder_create(SAMPLE_RATE, OUTPUT_CH, &error);
        if (!stream->decoder || error != OPUS_OK) {
            log_->error("Unable to create {} decoder: {}", stream->name, opus_strerror(error));
            return false;
        }
    }
    return true;
}
bool AudioEngine::openSockets() {
    for (auto &stream : streams_) {
        stream->receiverSource = receiverFactory_();
        if (!stream->receiverSource || !stream->receiverSource->open(stream->group, interfaceIp_, port_))
            return false;
        if (port_ != 65535) {
            stream->rtcpReceiverSource = receiverFactory_();
            if (stream->rtcpReceiverSource &&
                !stream->rtcpReceiverSource->open(stream->group, interfaceIp_, static_cast<uint16_t>(port_ + 1)))
                stream->rtcpReceiverSource.reset();
        }
        if (!stream->rtcpReceiverSource) {
            if (config_.requireRtcp && stream->channel == 17)
                return false;
            log_->warn("{} RTCP unavailable", stream->name);
        }
    }
    return true;
}
void AudioEngine::joinWorkerThreads() {
    for (auto &stream : streams_) {
        if (stream->receiver.joinable())
            stream->receiver.join();
        if (stream->rtcpReceiver.joinable())
            stream->rtcpReceiver.join();
    }
    if (playoutThread_.joinable())
        playoutThread_.join();
}
void AudioEngine::releaseResources() {
    keepalive_.reset();
    if (audioOutput_) {
        audioOutput_->stopAndClear();
        audioOutput_->close();
    }
    for (auto &stream : streams_) {
        stream->receiverSource.reset();
        stream->rtcpReceiverSource.reset();
        if (stream->decoder) {
            opus_decoder_destroy(stream->decoder);
            stream->decoder = nullptr;
        }
    }
    bufferLevel_.store(0);
    outputQueuedFrames_.store(0);
}
void AudioEngine::receiveStream(InputStream &stream) {
    setThreadName(fmt::format("rtp-rx-{}", stream.channel));
    try {
        std::vector<uint8_t> packet(MAX_RTP_PACKET_SIZE + 1);
        while (!stop_requested.load()) {
            const auto result = stream.receiverSource->receive(packet, MAX_RTP_PACKET_SIZE);
            if (result == PacketReceiver::Result::Timeout)
                continue;
            if (result == PacketReceiver::Result::Error)
                throw std::runtime_error("RTP socket receive failed");
            auto parsed = parseOpusRtpPacket(packet);
            if (!parsed) {
                stream.stats.invalidPackets.fetch_add(1);
                continue;
            }
            const auto arrival = Clock::now();
            stream.stats.packetsReceived.fetch_add(1);
            lastPacketArrivalNanoseconds_.store(
                std::chrono::duration_cast<std::chrono::nanoseconds>(arrival.time_since_epoch()).count());
            const auto pushResult = stream.buffer.push({std::move(*parsed), arrival});
            if (pushResult == RtpJitterBuffer::PushResult::Duplicate)
                stream.stats.duplicatePackets.fetch_add(1);
            if (pushResult == RtpJitterBuffer::PushResult::Overrun)
                stream.stats.bufferOverruns.fetch_add(1);
        }
    } catch (const std::exception &error) {
        log_->error("{} receiver failed: {}", stream.name, error.what());
        state_.store(State::Failed);
        stop_requested.store(true);
    }
}
void AudioEngine::receiveRtcpStream(InputStream &stream) {
    setThreadName(fmt::format("rtcp-rx-{}", stream.channel));
    try {
        std::vector<uint8_t> packet(MAX_RTCP_PACKET_SIZE + 1);
        while (!stop_requested.load()) {
            const auto result = stream.rtcpReceiverSource->receive(packet, MAX_RTCP_PACKET_SIZE);
            if (result == PacketReceiver::Result::Timeout)
                continue;
            if (result == PacketReceiver::Result::Error)
                throw std::runtime_error("RTCP socket receive failed");
            auto report = parseRtcpSenderReport(packet);
            if (!report) {
                stream.rtcpStats.invalidReports.fetch_add(1);
                continue;
            }
            stream.rtcpStats.reportsReceived.fetch_add(1);
            if (stream.reports.store(std::move(*report), Clock::now()))
                stream.rtcpStats.cacheEvictions.fetch_add(1);
        }
    } catch (const std::exception &error) {
        log_->error("{} RTCP receiver failed: {}", stream.name, error.what());
        state_.store(State::Failed);
        stop_requested.store(true);
    }
}

bool AudioEngine::decodeTimestamp(InputStream &stream, uint32_t timestamp, uint64_t generation,
                                  DecodedAudioFrame &samples) {
    if (stream.buffer.generation() != generation) {
        samples.fill(0);
        return true;
    }
    if (generation != stream.decoderGeneration) {
        opus_decoder_ctl(stream.decoder, OPUS_RESET_STATE);
        stream.decoderGeneration = generation;
    }
    if (stream.buffer.isIdleFor(std::chrono::milliseconds(STREAM_IDLE_TIMEOUT_MS)) || generation == 0) {
        samples.fill(0);
        return true;
    }
    int decoded = 0;
    if (auto packet = stream.buffer.take(timestamp, generation)) {
        decoded =
            opus_decode(stream.decoder, packet->packet.payload.data(),
                        static_cast<opus_int32>(packet->packet.payload.size()), samples.data(), FRAMES_PER_CHUNK, 0);
        if (decoded >= 0)
            stream.stats.decodedFrames.fetch_add(1);
    } else if (auto next = stream.buffer.peek(timestamp + FRAMES_PER_CHUNK, generation)) {
        decoded =
            opus_decode(stream.decoder, next->packet.payload.data(),
                        static_cast<opus_int32>(next->packet.payload.size()), samples.data(), FRAMES_PER_CHUNK, 1);
        if (decoded >= 0)
            stream.stats.fecFrames.fetch_add(1);
    } else {
        decoded = opus_decode(stream.decoder, nullptr, 0, samples.data(), FRAMES_PER_CHUNK, 0);
        if (decoded >= 0)
            stream.stats.concealedFrames.fetch_add(1);
    }
    if (decoded < 0) {
        samples.fill(0);
        stream.stats.decodeErrors.fetch_add(1);
        return false;
    }
    std::fill(samples.begin() + decoded, samples.end(), 0);
    return true;
}
void AudioEngine::mixTimestamp(uint32_t timestamp, uint64_t masterGeneration, std::span<int16_t> mixed) {
    for (size_t index = 0; index < streams_.size(); ++index) {
        auto &stream = *streams_[index];
        gains_[index] = stream.gain.load();
        const auto generation = index == masterIndex_ ? masterGeneration : stream.buffer.generation();
        decodedGenerations_[index] = generation;
        if (index != masterIndex_ && activeReport_ && stream.qualifiedGeneration != generation) {
            const auto source = stream.buffer.synchronizationSource();
            const auto report = source ? stream.reports.find(*source) : std::nullopt;
            if (!report || Clock::now() - report->receivedAt > std::chrono::milliseconds(RTCP_MAX_REPORT_AGE_MS) ||
                !rtcpClockMappingsCompatible(*activeReport_, report->report,
                                             std::chrono::microseconds(RTCP_CLOCK_COMPATIBILITY_US))) {
                decoded_[index].fill(0);
                continue;
            }
            stream.qualifiedGeneration = generation;
        }
        decodeTimestamp(stream, timestamp, generation, decoded_[index]);
    }
    mixer_.process(decoded_, gains_, mixed);
}

std::optional<std::chrono::milliseconds> AudioEngine::getLastPacketAge() const {
    const int64_t last = lastPacketArrivalNanoseconds_.load();
    if (!last)
        return std::nullopt;
    const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::nanoseconds(std::max<int64_t>(0, now - last)));
}
std::optional<std::chrono::milliseconds> AudioEngine::getLastRtcpReportAge() const {
    const auto &master = *streams_[masterIndex_];
    const auto source = master.buffer.synchronizationSource();
    const auto report = source ? master.reports.find(*source) : std::nullopt;
    if (!report)
        return std::nullopt;
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - report->receivedAt);
}
size_t AudioEngine::getBufferedFrames(uint8_t channel) const {
    for (const auto &stream : streams_)
        if (stream->channel == channel)
            return stream->buffer.size();
    return 0;
}
uint64_t AudioEngine::getRtcpReportsReceived() const {
    uint64_t total = 0;
    for (const auto &stream : streams_)
        total += stream->rtcpStats.reportsReceived.load();
    return total;
}
uint64_t AudioEngine::getRtcpInvalidReports() const {
    uint64_t total = 0;
    for (const auto &stream : streams_)
        total += stream->rtcpStats.invalidReports.load();
    return total;
}
const char *AudioEngine::getTimingModeName() const {
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
bool AudioEngine::isReceiving() const {
    const auto age = getLastPacketAge();
    return state_.load() == State::Running && age && *age < std::chrono::seconds(1);
}
std::string AudioEngine::getStats() const {
    std::string stats = fmt::format(
        "packets={}, output queue={:.1f} ms, receiving={}, timing={}, RTCP reports={}/{} valid/invalid, fallbacks={}, "
        "start lateness={:+d} us, late starts dropped={}, deadline misses={}",
        getPacketsReceived(), static_cast<double>(getOutputQueuedFrames()) * 1000 / SAMPLE_RATE, isReceiving(),
        getTimingModeName(), getRtcpReportsReceived(), getRtcpInvalidReports(), getRtcpFallbacks(),
        getLastStartLatenessMicroseconds(), rtcpLateFramesDropped_.load(), playoutDeadlineMisses_.load());
    for (const auto &stream : streams_)
        stats +=
            fmt::format("; {}: queued={}, decoded={}, fec={}, plc={}, errors={}, invalid={}, duplicate={}, overruns={}",
                        stream->name, stream->buffer.size(), stream->stats.decodedFrames.load(),
                        stream->stats.fecFrames.load(), stream->stats.concealedFrames.load(),
                        stream->stats.decodeErrors.load(), stream->stats.invalidPackets.load(),
                        stream->stats.duplicatePackets.load(), stream->stats.bufferOverruns.load());
    return stats;
}

void AudioEngine::audioPlayoutThread() {
    setThreadName("audio-playout");
    auto &master = *streams_[masterIndex_];
    auto &keepalive = *keepalive_;
    const size_t targetQueue = std::max<size_t>(TARGET_PLAYOUT_FRAMES * FRAMES_PER_CHUNK,
                                                audioOutput_->pipelineLatencyFrames() + FRAMES_PER_CHUNK);
    const auto idleTimeout = std::chrono::milliseconds(STREAM_IDLE_TIMEOUT_MS);
    std::vector<int16_t> mixed(FRAMES_PER_CHUNK * audioConfig_.channels.size());
    uint64_t fallbackGeneration = 0;
    uint64_t waitingGeneration = 0;
    const auto fail = [](const char *message) { throw std::runtime_error(message); };
    while (!stop_requested.load()) {
        timingMode_.store(TimingMode::Waiting);
        if (!keepalive.refillSilence())
            fail("Unable to maintain idle audio output");
        const auto initial = master.buffer.initialPlayout();
        if (!initial) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        const uint32_t timestamp = initial->timestamp;
        const auto report = master.reports.find(initial->synchronizationSource);
        const bool fresh =
            report && Clock::now() - report->receivedAt <= std::chrono::milliseconds(RTCP_MAX_REPORT_AGE_MS);
        std::optional<RtcpPlayoutPlanner> planner;
        std::optional<RtcpPlayoutPlan> plan;
        int64_t mappingArrivalDelta = 0;
        if (fresh && !report->report.canonicalName.empty() && fallbackGeneration != initial->generation) {
            planner.emplace(RtcpClockPair::capture(), std::chrono::milliseconds(audioConfig_.commonPlayoutDelayMs),
                            std::chrono::milliseconds(audioConfig_.audioDeviceCompensationMs));
            plan = planner->plan(report->report, timestamp, audioOutput_->queuedFrames());
            if (plan) {
                mappingArrivalDelta = std::chrono::duration_cast<std::chrono::microseconds>(
                                          plan->presentationDeadline - initial->arrival -
                                          std::chrono::milliseconds(audioConfig_.commonPlayoutDelayMs))
                                          .count();
                if (!rtcpPresentationDeadlinePlausible(*plan, initial->arrival,
                                                       std::chrono::milliseconds(audioConfig_.commonPlayoutDelayMs),
                                                       std::chrono::milliseconds(RTCP_MAX_DEADLINE_DISTANCE_MS)))
                    plan.reset();
            }
        }
        if (!plan && fallbackGeneration != initial->generation &&
            Clock::now() - initial->arrival < std::chrono::milliseconds(RTCP_STARTUP_WAIT_MS)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (!plan && config_.requireRtcp) {
            if (waitingGeneration != initial->generation) {
                waitingGeneration = initial->generation;
                log_->warn("BGM SSRC {} waiting for usable RTCP; output remains silent (report={}, fresh={}, "
                           "mapping/arrival delta={:+d} us)",
                           initial->synchronizationSource, report.has_value(), fresh, mappingArrivalDelta);
            }
            master.buffer.discardBefore(timestamp + FRAMES_PER_CHUNK, initial->generation);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        bool missed = false;
        if (plan) {
            while (!stop_requested.load() && master.buffer.generation() == initial->generation) {
                if (!keepalive.refillSilence())
                    fail("Unable to maintain output before RTCP playout");
                plan = planner->plan(report->report, timestamp, audioOutput_->queuedFrames());
                if (!plan)
                    fail("Previously valid RTCP mapping became invalid");
                const auto enqueueState =
                    classifyRtcpEnqueue(*plan, Clock::now(), std::chrono::microseconds(RTCP_START_LATE_TOLERANCE_US));
                if (enqueueState == RtcpEnqueueState::Missed) {
                    missed = true;
                    break;
                }
                if (enqueueState == RtcpEnqueueState::Ready)
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (missed) {
                rtcpLateFramesDropped_.fetch_add(1);
                master.buffer.discardBefore(timestamp + FRAMES_PER_CHUNK, initial->generation);
                continue;
            }
        } else {
            if (fallbackGeneration != initial->generation) {
                fallbackGeneration = initial->generation;
                rtcpFallbacks_.fetch_add(1);
                log_->warn("BGM SSRC {} uses arrival-time fallback (mapping/arrival delta={:+d} us)",
                           initial->synchronizationSource, mappingArrivalDelta);
            }
            while (!stop_requested.load() && master.buffer.generation() == initial->generation &&
                   Clock::now() < initial->startTime) {
                if (!keepalive.refillSilence())
                    fail("Unable to maintain output before fallback playout");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        if (stop_requested.load())
            break;
        if (master.buffer.generation() != initial->generation)
            continue;
        timingMode_.store(plan ? TimingMode::Rtcp : TimingMode::ArrivalFallback);
        activeReport_ = plan ? std::optional<RtcpSenderReport>(report->report) : std::nullopt;
        for (auto &stream : streams_)
            stream->qualifiedGeneration = 0;
        mixer_.resetFilters();
        mixTimestamp(timestamp, initial->generation, mixed);
        const size_t queuedBeforeWrite = audioOutput_->queuedFrames();
        const auto writeTime = Clock::now();
        if (!audioOutput_->write(mixed))
            fail("Unable to queue initial audio frame");
        if (plan) {
            const auto predicted = writeTime + framesToClockDuration(queuedBeforeWrite) +
                                   std::chrono::milliseconds(audioConfig_.audioDeviceCompensationMs);
            lastStartLatenessMicroseconds_.store(
                std::chrono::duration_cast<std::chrono::microseconds>(predicted - plan->presentationDeadline).count());
            log_->info("RTCP playout: BGM SSRC {}, CNAME '{}', RTP timestamp {}, delay={} ms, compensation={:+d} ms, "
                       "mapping/arrival delta={:+d} us, start lateness={:+d} us",
                       initial->synchronizationSource, report->report.canonicalName, timestamp,
                       audioConfig_.commonPlayoutDelayMs, audioConfig_.audioDeviceCompensationMs, mappingArrivalDelta,
                       lastStartLatenessMicroseconds_.load());
        } else {
            lastStartLatenessMicroseconds_.store(0);
        }
        uint32_t nextTimestamp = timestamp + FRAMES_PER_CHUNK;
        for (size_t index = 0; index < streams_.size(); ++index)
            streams_[index]->buffer.discardBefore(nextTimestamp, decodedGenerations_[index]);
        mixedFrames_.fetch_add(1);
        while (!stop_requested.load() && master.buffer.generation() == initial->generation &&
               !master.buffer.isIdleFor(idleTimeout)) {
            const size_t queued = audioOutput_->queuedFrames();
            if (queued >= targetQueue || (!master.buffer.contains(nextTimestamp) && queued >= FRAMES_PER_CHUNK)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            if (!master.buffer.contains(nextTimestamp))
                std::this_thread::sleep_for(std::chrono::milliseconds(PACKET_WAIT_MS));
            if (stop_requested.load() || master.buffer.generation() != initial->generation)
                break;
            if (audioOutput_->queuedFrames() == 0)
                playoutDeadlineMisses_.fetch_add(1);
            mixTimestamp(nextTimestamp, initial->generation, mixed);
            if (!audioOutput_->write(mixed))
                fail("Unable to queue audio frame");
            nextTimestamp += FRAMES_PER_CHUNK;
            for (size_t index = 0; index < streams_.size(); ++index)
                streams_[index]->buffer.discardBefore(nextTimestamp, decodedGenerations_[index]);
            mixedFrames_.fetch_add(1);
        }
        log_->debug("Audio returned to idle keepalive");
    }
}

} // namespace creatures::audio
