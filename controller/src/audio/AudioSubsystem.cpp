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

bool AudioSubsystem::initialize(uint8_t creatureChannel, const std::string &ifaceIp, uint8_t audioDev, uint16_t port) {
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
                                                      port, creatureChannel, ifaceIp, audioDev);

    if (!rtpClient_) {
        log_->error("Failed to create RTP audio client");
        return false;
    }

    log_->info("Audio subsystem initialized successfully");
    return true;
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

    return fmt::format("packets received={}, buffer={:.1f}%, receiving={}", rtpClient_->getPacketsReceived(),
                       rtpClient_->getBufferLevel() * 100.0f, rtpClient_->isReceiving() ? "yes" : "no");
}

void AudioSubsystem::monitoringLoop() {
    setThreadName("AudioMon");

    log_->debug("Audio monitoring loop started");

    // Buffer warnings are edge triggered. A buffer sitting outside its
    // watermarks stays a problem, but restating it on every sample buries
    // everything else in the log, so report entering and leaving the condition
    // rather than repeating it.
    enum class BufferState { Normal, High, Low };
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
            if (bufferLevel > BUF_HIGH_WATERMARK) {
                bufferState = BufferState::High;
            } else if (bufferLevel < BUF_LOW_WATERMARK && receiving) {
                bufferState = BufferState::Low;
            }

            if (bufferState != lastBufferState) {
                switch (bufferState) {
                case BufferState::High:
                    log_->warn("Audio buffer level high: {:.1f}%", bufferLevel * 100.0f);
                    break;
                case BufferState::Low:
                    log_->warn("Audio buffer level low: {:.1f}%", bufferLevel * 100.0f);
                    break;
                case BufferState::Normal:
                    log_->info("Audio buffer level normal: {:.1f}% (receiving={})", bufferLevel * 100.0f,
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