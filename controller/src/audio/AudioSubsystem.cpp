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
    threadName = "AudioSubsystem";
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

    dialogChannel_ = creatureChannel;
    try {
        audioEngine_ =
            std::make_unique<AudioEngine>(log_, creatureAudioConfig(creatureChannel, ifaceIp, audioConfig, port));
    } catch (const std::exception &error) {
        log_->error("Unable to initialize audio subsystem: {}", error.what());
        return false;
    }

    log_->info("Audio subsystem initialized successfully");
    return true;
}

void AudioSubsystem::setDialogGainDb(float gainDb) {
    if (audioEngine_) {
        audioEngine_->setInputGainDb(dialogChannel_, gainDb);
    }
}

void AudioSubsystem::setBgmGainDb(float gainDb) {
    if (audioEngine_) {
        audioEngine_->setInputGainDb(17, gainDb);
    }
}

AudioSubsystem::~AudioSubsystem() { shutdown(); }

void AudioSubsystem::start() { run(); }

void AudioSubsystem::run() {
    if (!audioEngine_) {
        log_->error("Audio subsystem not initialized - cannot start");
        return;
    }

    log_->info("Starting RTP audio client");
    if (!audioEngine_->start()) {
        log_->error("Audio engine failed to start");
        return;
    }

    stopMon_.store(false);
    try {
        monThread_ = std::thread(&AudioSubsystem::monitoringLoop, this);
    } catch (...) {
        audioEngine_->shutdown();
        throw;
    }

    log_->info("Audio subsystem running");
}

void AudioSubsystem::shutdown() {
    log_->info("Shutting down audio subsystem");

    stopMon_.store(true);

    if (monThread_.joinable()) {
        log_->debug("Waiting for monitoring thread to complete");
        monThread_.join();
    }

    if (audioEngine_) {
        log_->debug("Shutting down RTP client");
        audioEngine_->shutdown();
    }

    log_->info("Audio subsystem shutdown complete");
}

bool AudioSubsystem::isReceiving() const { return audioEngine_ && audioEngine_->isReceiving(); }

std::string AudioSubsystem::getStats() const {
    if (!audioEngine_) {
        return "audio disabled";
    }

    const double outputQueueMilliseconds =
        static_cast<double>(audioEngine_->getOutputQueuedFrames()) * 1000.0 / SAMPLE_RATE;
    const double targetQueueMilliseconds =
        static_cast<double>(TARGET_PLAYOUT_FRAMES * FRAMES_PER_CHUNK) * 1000.0 / SAMPLE_RATE;
    const auto packetAge = audioEngine_->getLastPacketAge();
    const std::string packetAgeText = packetAge.has_value() ? fmt::format("{} ms ago", packetAge->count()) : "never";
    const auto reportAge = audioEngine_->getLastRtcpReportAge();
    const std::string reportAgeText = reportAge.has_value() ? fmt::format("{} ms", reportAge->count()) : "none";

    return fmt::format(
        "packets received={}, output queue={:.1f}/{:.1f} ms, RTP queued dialog={} BGM={}, last packet={}, "
        "receiving={}, timing={}, RTCP reports={}/{} valid/invalid, report age={}, fallbacks={}, start lateness={:+d} "
        "us",
        audioEngine_->getPacketsReceived(), outputQueueMilliseconds, targetQueueMilliseconds,
        audioEngine_->getBufferedFrames(dialogChannel_), audioEngine_->getBufferedFrames(17), packetAgeText,
        audioEngine_->isReceiving() ? "yes" : "no", audioEngine_->getTimingModeName(),
        audioEngine_->getRtcpReportsReceived(), audioEngine_->getRtcpInvalidReports(), reportAgeText,
        audioEngine_->getRtcpFallbacks(), audioEngine_->getLastStartLatenessMicroseconds());
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

        if (audioEngine_) {
            log_->debug("Audio stats: {}", getStats());

            const float bufferLevel = audioEngine_->getBufferLevel();
            const bool receiving = audioEngine_->isReceiving();

            BufferState bufferState = BufferState::Normal;
            if (bufferLevel < 0.25f && receiving) {
                bufferState = BufferState::Low;
            }

            if (bufferState != lastBufferState) {
                const double queuedMilliseconds =
                    static_cast<double>(audioEngine_->getOutputQueuedFrames()) * 1000.0 / SAMPLE_RATE;
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
