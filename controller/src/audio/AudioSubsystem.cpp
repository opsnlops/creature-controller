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

        this->logger->debug("AudioSubsystem created");
    }

    bool AudioSubsystem::initialize(const std::string& multicastGroup,
                               uint16_t port,
                               uint8_t audioDevice,
                               const std::string& networkDevice,
                               uint8_t creatureAudioChannel) {

        logger->info("Initializing audio subsystem for creature channel {}", creatureAudioChannel);
        logger->debug("Multicast: {}, Port: {}, Device: {}", multicastGroup, port, audioDevice);

        // Create the RTP audio client with creature channel
        rtpClient = std::make_shared<RtpAudioClient>(
            logger,
            audioDevice,
            multicastGroup,
            port,
            STREAM_AUDIO_CHANNELS,  // 17 channels in the stream
            AUDIO_SAMPLE_RATE,
            networkDevice,
            creatureAudioChannel    // Which creature channel to extract
        );

        if (audioDevice) {
            rtpClient->setAudioDevice(audioDevice);
            logger->debug("Set audio device to: {}", audioDevice);
        }

        // Set full volume - use hardware controls for adjustment
        rtpClient->setVolume(DEFAULT_VOLUME);

        logger->info("Audio subsystem configured for creature channel {} with BGM mixing", creatureAudioChannel);
        logger->debug("Format: {} -> Mono output, Bandwidth: {:.1f} Mbps, Packet rate: {:.0f} Hz",
                     getAudioFormatDescription(),
                     static_cast<float>(getExpectedBandwidthBps()) / 1024.0f / 1024.0f,
                     getExpectedPacketRateHz());

        return true;
    }

    void AudioSubsystem::run() {
        if (!rtpClient) {
            logger->error("Cannot start audio - not initialized");
            return;
        }

        logger->info("Starting audio subsystem");

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
            logger->info("Audio receiving packets");
        } else {
            logger->info("Audio client started, waiting for packets");
        }
    }

    void AudioSubsystem::shutdown() {
        logger->info("Stopping audio subsystem");

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

        logger->info("Audio subsystem stopped");
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
                logger->info("Audio status: {}", getStats());

                // Check for potential issues
                float bufferLevel = rtpClient->getBufferLevel();
                if (bufferLevel > BUFFER_HIGH_WATERMARK) {
                    logger->warn("Audio buffer high ({:.1f}%) - possible network congestion", bufferLevel * 100.0f);
                } else if (bufferLevel < BUFFER_LOW_WATERMARK && isReceiving()) {
                    logger->warn("Audio buffer low ({:.1f}%) - possible network issues", bufferLevel * 100.0f);
                }
            }
        }

        logger->debug("Audio monitoring thread finished");
    }

} // namespace creatures::audio