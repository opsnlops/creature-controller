//
// AudioSubsystem.cpp
//

#include <chrono>
#include <thread>

#include <fmt/format.h>

#include "util/thread_name.h"

#include "AudioSubsystem.h"

using namespace creatures::audio;

AudioSubsystem::AudioSubsystem(std::shared_ptr<creatures::Logger> log) : log_(log) {
    log_->debug("AudioSubsystem created");
}

bool AudioSubsystem::initialize(uint8_t creatureChannel, const std::string &ifaceIp, const AudioConfig &audioConfig,
                                uint16_t port) {
    if (creatureChannel < 1 || creatureChannel > 16) {
        log_->error("Invalid creature channel: {} (must be 1-16)", creatureChannel);
        return false;
    }

    log_->info("Initializing audio subsystem: creature channel {}, interface "
               "{}, port {}",
               creatureChannel, ifaceIp, port);

    // Construct multicast addresses
    std::string dialogGroup = fmt::format("{}{}", DIALOG_GROUP_BASE, creatureChannel);
    std::string bgmGroup = BGM_GROUP;

    log_->debug("Dialog multicast group: {}", dialogGroup);
    log_->debug("BGM multicast group: {}", bgmGroup);

    rtpClient_ = std::make_shared<OpusRtpAudioClient>(log_,
                                                      dialogGroup, // Dialog channel for this creature
                                                      bgmGroup,    // BGM channel (always channel 17)
                                                      port, creatureChannel, ifaceIp, audioConfig);

    if (!rtpClient_) {
        log_->error("Failed to create RTP audio client");
        return false;
    }

    log_->info("Audio subsystem initialized successfully");
    return true;
}

void AudioSubsystem::setDialogGainDb(float gainDb) {
    if (rtpClient_) {
        rtpClient_->setDialogGainDb(gainDb);
    }
}

void AudioSubsystem::setBgmGainDb(float gainDb) {
    if (rtpClient_) {
        rtpClient_->setBgmGainDb(gainDb);
    }
}

void AudioSubsystem::run() {
    if (!rtpClient_) {
        log_->error("Audio subsystem not initialized - cannot start");
        return;
    }

    setThreadName("AudioSubsystem");

    log_->info("Starting RTP audio client");
    rtpClient_->start();

    stopMon_.store(false);
    monThread_ = std::thread(&AudioSubsystem::monitoringLoop, this);

    running_.store(true);
    log_->info("Audio subsystem running");
}

void AudioSubsystem::shutdown() {
    log_->info("Shutting down audio subsystem");

    running_.store(false);
    stopMon_.store(true);

    if (monThread_.joinable()) {
        log_->debug("Waiting for monitoring thread to complete");
        monThread_.join();
    }

    if (rtpClient_) {
        log_->debug("Shutting down RTP client");
        rtpClient_->shutdown();
    }

    log_->info("Audio subsystem shutdown complete");
}

bool AudioSubsystem::isReceiving() const { return rtpClient_ && rtpClient_->isReceiving(); }

std::string AudioSubsystem::getStats() const {
    if (!rtpClient_) {
        return "audio disabled";
    }

    const double outputQueueMilliseconds =
        static_cast<double>(rtpClient_->getOutputQueuedFrames()) * 1000.0 / SAMPLE_RATE;
    const double targetQueueMilliseconds =
        static_cast<double>(TARGET_PLAYOUT_FRAMES * FRAMES_PER_CHUNK) * 1000.0 / SAMPLE_RATE;
    const auto packetAge = rtpClient_->getLastPacketAge();
    const std::string packetAgeText = packetAge.has_value() ? fmt::format("{} ms ago", packetAge->count()) : "never";
    const auto reportAge = rtpClient_->getLastRtcpReportAge();
    const std::string reportAgeText = reportAge.has_value() ? fmt::format("{} ms", reportAge->count()) : "none";

    return fmt::format(
        "packets received={}, output queue={:.1f}/{:.1f} ms, RTP queued dialog={} BGM={}, last packet={}, "
        "receiving={}, timing={}, RTCP reports={}/{} valid/invalid, report age={}, fallbacks={}, start lateness={:+d} "
        "us",
        rtpClient_->getPacketsReceived(), outputQueueMilliseconds, targetQueueMilliseconds,
        rtpClient_->getDialogBufferedFrames(), rtpClient_->getBgmBufferedFrames(), packetAgeText,
        rtpClient_->isReceiving() ? "yes" : "no", rtpClient_->getTimingModeName(), rtpClient_->getRtcpReportsReceived(),
        rtpClient_->getRtcpInvalidReports(), reportAgeText, rtpClient_->getRtcpFallbacks(),
        rtpClient_->getLastStartLatenessMicroseconds());
}

void AudioSubsystem::monitoringLoop() {
    setThreadName("AudioMon");

    log_->debug("Audio monitoring loop started");

    // Low-buffer warnings are edge triggered so a single incident does not
    // bury the rest of the controller log.
    enum class BufferState { Normal, Low };
    BufferState lastBufferState = BufferState::Normal;
    int samplesSinceSummary = 0;

    while (!stopMon_.load()) {
        // Sleep in smaller increments to be more responsive to shutdown
        for (int i = 0; i < STATS_INTERVAL_SEC * 10 && !stopMon_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (stopMon_.load())
            break;

        if (rtpClient_) {
            log_->debug("Audio stats: {}", getStats());

            const float bufferLevel = rtpClient_->getBufferLevel();
            const bool receiving = rtpClient_->isReceiving();

            BufferState bufferState = BufferState::Normal;
            if (bufferLevel < 0.25f && receiving) {
                bufferState = BufferState::Low;
            }

            if (bufferState != lastBufferState) {
                const double queuedMilliseconds =
                    static_cast<double>(rtpClient_->getOutputQueuedFrames()) * 1000.0 / SAMPLE_RATE;
                switch (bufferState) {
                case BufferState::Low:
                    log_->warn("Audio output queue low: {:.1f} ms", queuedMilliseconds);
                    break;
                case BufferState::Normal:
                    log_->info("Audio output queue normal: {:.1f} ms (receiving={})", queuedMilliseconds,
                               receiving ? "yes" : "no");
                    break;
                }
                lastBufferState = bufferState;
            }

            // Periodic summary, so the normal log still shows the audio path is
            // alive without a line on every sample
            if (++samplesSinceSummary >= SUMMARY_EVERY_N_SAMPLES) {
                samplesSinceSummary = 0;
                log_->info("Audio: {}", getStats());
            }
        }
    }

    log_->debug("Audio monitoring loop stopped");
}
