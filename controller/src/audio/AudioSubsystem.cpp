//
// AudioSubsystem.cpp
//

#include <chrono>
#include <thread>

#include <fmt/format.h>

#include "util/thread_name.h"

#include "AudioSubsystem.h"

using namespace creatures::audio;

AudioSubsystem::AudioSubsystem(std::shared_ptr<creatures::Logger> log)
    : log_(log)
{
    log_->debug("AudioSubsystem created");
}

bool AudioSubsystem::initialize(uint8_t creatureChannel,
                                const std::string& ifaceIp,
                                uint8_t audioDev,
                                uint16_t port)
{
    if (creatureChannel < 1 || creatureChannel > 16) {
        log_->error("Invalid creature channel: {} (must be 1-16)", creatureChannel);
        return false;
    }

    log_->info("Initializing audio subsystem: creature channel {}, interface {}, port {}",
               creatureChannel, ifaceIp, port);

    // Construct multicast addresses
    std::string dialogGroup = fmt::format("{}{}", DIALOG_GROUP_BASE, creatureChannel);
    std::string bgmGroup = BGM_GROUP;

    log_->debug("Dialog multicast group: {}", dialogGroup);
    log_->debug("BGM multicast group: {}", bgmGroup);

    rtpClient_ = std::make_shared<OpusRtpAudioClient>(
        log_,
        dialogGroup,     // Dialog channel for this creature
        bgmGroup,        // BGM channel (always channel 17)
        port,
        creatureChannel,
        ifaceIp);

    if (!rtpClient_) {
        log_->error("Failed to create RTP audio client");
        return false;
    }

    log_->info("Audio subsystem initialized successfully");
    return true;
}

void AudioSubsystem::run()
{
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

void AudioSubsystem::shutdown()
{
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

bool AudioSubsystem::isReceiving() const
{
    return rtpClient_ && rtpClient_->isReceiving();
}

std::string AudioSubsystem::getStats() const
{
    if (!rtpClient_) {
        return "audio disabled";
    }

    return fmt::format("packets received={}, buffer={:.1f}%, receiving={}",
                       rtpClient_->getPacketsReceived(),
                       rtpClient_->getBufferLevel() * 100.0f,
                       rtpClient_->isReceiving() ? "yes" : "no");
}

void AudioSubsystem::monitoringLoop()
{
    setThreadName("AudioMon");

    log_->debug("Audio monitoring loop started");

    while (!stopMon_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(STATS_INTERVAL_SEC));
        if (stopMon_.load()) break;

        if (rtpClient_) {
            log_->info("Audio stats: {}", getStats());

            // Log warnings for unusual conditions
            float bufferLevel = rtpClient_->getBufferLevel();
            if (bufferLevel > BUF_HIGH_WATERMARK) {
                log_->warn("Audio buffer level high: {:.1f}%", bufferLevel * 100.0f);
            } else if (bufferLevel < BUF_LOW_WATERMARK && rtpClient_->isReceiving()) {
                log_->warn("Audio buffer level low: {:.1f}%", bufferLevel * 100.0f);
            }
        }
    }

    log_->debug("Audio monitoring loop stopped");
}