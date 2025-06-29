//
// AudioSubsystem.cpp
//

#include "AudioSubsystem.h"

#include <chrono>
#include <thread>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "logging/Logger.h"
#include "util/thread_name.h"

namespace creatures::audio {

    AudioSubsystem::AudioSubsystem(std::shared_ptr<creatures::Logger> logger)
        : logger(std::move(logger)) {

        this->logger->debug("🐰 AudioSubsystem created");
    }

    bool AudioSubsystem::initialize(const std::string& multicastGroup,
                                   uint16_t port,
                                   uint8_t audioDevice) {

        logger->info("🐰 Initializing audio subsystem...");
        logger->debug("  • Multicast: {}", multicastGroup);
        logger->debug("  • Port: {}", port);
        logger->debug("  • Audio Device: {}", audioDevice);

        // Create the RTP audio client using constants from audio-config.h
        rtpClient = std::make_shared<RtpAudioClient>(
            logger,
            audioDevice,
            multicastGroup,
            port,
            DEVICE_AUDIO_CHANNELS,
            AUDIO_SAMPLE_RATE
        );

        if (audioDevice) {
            rtpClient->setAudioDevice(audioDevice);
            logger->debug("Set audio device to: {}", audioDevice);
        }

        // Set full volume - use hardware controls for adjustment! 🐰
        rtpClient->setVolume(DEFAULT_VOLUME);

        logger->info("🎵 Audio subsystem configured successfully");
        logger->debug("  • Format: {}", getAudioFormatDescription());
        logger->debug("  • Expected bandwidth: {:.1f} Mbps",
                     static_cast<float>(getExpectedBandwidthBps()) / 1024.0f / 1024.0f);
        logger->debug("  • Expected packet rate: {:.0f} Hz", getExpectedPacketRateHz());

        return true;
    }

    void AudioSubsystem::run() {
        if (!rtpClient) {
            logger->error("🐰 Cannot start audio - not initialized!");
            return;
        }

        logger->info("🐰 Starting audio subsystem...");

        auto threadName = fmt::format("AudioSubsystem::{}", this->rtpClient->getName());
        setThreadName(threadName);

        // Start the RTP client
        rtpClient->start();

        // Start monitoring thread
        shouldStop.store(false);
        monitoringThread = std::thread(&AudioSubsystem::monitoringLoop, this);

        // Give it a moment to establish connection
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        running.store(true);

        if (rtpClient->isReceiving()) {
            logger->info("🎵 Audio is hopping in loud and clear!");
        } else {
            logger->info("Audio client started, waiting for packets...");
        }
    }

    void AudioSubsystem::shutdown() {
        logger->info("🐰 Stopping audio subsystem...");

        running.store(false);
        shouldStop.store(true);

        // Stop monitoring thread first
        if (monitoringThread.joinable()) {
            monitoringThread.join();
        }

        // Stop the RTP client
        if (rtpClient) {
            rtpClient->shutdown();
        }

        logger->info("Audio subsystem stopped cleanly");
    }

    bool AudioSubsystem::isReceiving() const {
        return rtpClient && rtpClient->isReceiving();
    }

    std::string AudioSubsystem::getStats() const {
        if (!rtpClient) {
            return "Audio disabled";
        }

        return fmt::format("Packets: {}, Buffer: {:.1f}%, Receiving: {}",
                          rtpClient->getPacketsReceived(),
                          rtpClient->getBufferLevel() * 100.0f,
                          rtpClient->isReceiving() ? "yes" : "no");
    }

    void AudioSubsystem::monitoringLoop() {
        setThreadName("AudioMonitor");
        logger->debug("Audio monitoring thread started");

        while (!shouldStop.load()) {
            // Log stats at the configured interval
            std::this_thread::sleep_for(std::chrono::seconds(STATS_LOG_INTERVAL_SEC));

            if (!shouldStop.load() && rtpClient) {
                logger->info("🎵 Audio Status: {}", getStats());

                // Check for potential issues
                float bufferLevel = rtpClient->getBufferLevel();
                if (bufferLevel > BUFFER_HIGH_WATERMARK) {
                    logger->warn("🐰 Audio buffer high ({:.1%}) - possible network congestion", bufferLevel);
                } else if (bufferLevel < BUFFER_LOW_WATERMARK && isReceiving()) {
                    logger->warn("🐰 Audio buffer low ({:.1%}) - possible network issues", bufferLevel);
                }
            }
        }

        logger->debug("🐰 Audio monitoring thread finished");
    }

} // namespace creatures::audio