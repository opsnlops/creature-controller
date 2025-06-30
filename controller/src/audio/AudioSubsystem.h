//
// AudioSubsystem.h
//

#pragma once

#include <memory>
#include <atomic>
#include <thread>
#include <string>

#include "audio/RtpAudioClient.h"
#include "audio/audio-config.h"
#include "logging/Logger.h"
#include "util/StoppableThread.h"

namespace creatures::audio {

    /**
     * Audio subsystem manager - handles RTP audio reception independently
     * This runs as its own subsystem and manages audio completely separate from Controller 🐰
     */
    class AudioSubsystem : public StoppableThread {
    public:
        explicit AudioSubsystem(std::shared_ptr<creatures::Logger> logger);
        ~AudioSubsystem() = default;

        /**
         * Initialize the audio subsystem with configuration
         * @param multicastGroup Multicast IP address (defaults from audio-config.h)
         * @param port RTP port (defaults from audio-config.h)
         * @param audioDevice SDL audio device name, or nullptr for default
         * @param networkDevice the network device to bind to (ie eth0, wlan0, etc)
         * @return true if initialization succeeded
         */
        bool initialize(const std::string& multicastGroup = DEFAULT_MULTICAST_GROUP,
                       uint16_t port = DEFAULT_RTP_PORT,
                       uint8_t audioDevice = DEFAULT_SOUND_DEVICE_NUMBER,
                       std::string networkDevice = "eth0");

        /**
         * Start the audio subsystem
         * Begins RTP reception and audio playback
         */
        void run() override;

        /**
         * Stop the audio subsystem
         * Cleanly shuts down all audio operations
         */
        void shutdown() override;

        /**
         * Check if audio is being received
         */
        [[nodiscard]] bool isReceiving() const;

        /**
         * Get current audio statistics as a formatted string
         */
        [[nodiscard]] std::string getStats() const;

        /**
         * Check if the subsystem is running
         */
        [[nodiscard]] bool isRunning() const { return running.load(); }

    private:
        std::shared_ptr<creatures::Logger> logger;
        std::shared_ptr<RtpAudioClient> rtpClient;
        std::thread monitoringThread;
        std::atomic<bool> shouldStop{false};
        std::atomic<bool> running{false};

        /**
         * Monitoring loop that runs in a separate thread
         * Logs periodic statistics and health checks
         */
        void monitoringLoop();
    };

} // namespace creatures::audio