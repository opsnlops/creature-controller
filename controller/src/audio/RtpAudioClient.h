//
// RtpAudioClient.h
//

#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>

#include <SDL2/SDL.h>
#include <uvgrtp/lib.hh>

#include "audio-config.h"
#include "logging/Logger.h"
#include "util/StoppableThread.h"

namespace creatures::audio {

    /**
     * RTP Audio Client for receiving and playing multicast L16 audio streams
     * Some-bunny's got to handle those hop-coming audio packets! 🐰
     */
    class RtpAudioClient : public StoppableThread {
    public:
        /**
         * Constructor for the RTP audio client
         * @param logger Shared logger instance
         * @param audioDevice SDL audio device number (DEFAULT_SOUND_DEVICE_NUMBER is default)
         * @param multicastGroup Multicast IP address (e.g., "239.19.63.1")
         * @param port RTP port (e.g., 5004)
         * @param channels Number of audio channels (e.g., 17)
         * @param sampleRate Sample rate in Hz (e.g., 48000)
         */
        RtpAudioClient(std::shared_ptr<creatures::Logger> logger,
                       uint8_t audioDevice = DEFAULT_SOUND_DEVICE_NUMBER,
                       const std::string& multicastGroup = "239.19.63.1",
                       uint16_t port = 5004,
                       uint8_t channels = 17,
                       uint32_t sampleRate = 48000);

        ~RtpAudioClient() override;

        /**
         * Start the RTP client - begins receiving and playing audio
         * This is where the magic hoppens! 🐰
         */
        void start() override;

        /**
         * Stop the RTP client
         */
        void shutdown() override;

        /**
         * Check if the client is successfully receiving audio
         */
        [[nodiscard]] bool isReceiving() const { return receivingAudio.load(); }

        /**
         * Get the number of audio packets received
         */
        [[nodiscard]] uint64_t getPacketsReceived() const { return packetsReceived.load(); }

        /**
         * Get the current audio buffer level (0.0 to 1.0)
         * Helps detect if we're getting audio fast enough or too slowly
         */
        [[nodiscard]] float getBufferLevel() const;

        /**
         * Set the output audio device (call before start())
         * @param deviceName Name of the SDL audio device, or nullptr for default
         */
        void setAudioDevice(uint8_t deviceNumber) { audioDevice = deviceNumber; }

        /**
         * Set the audio volume (0-255, default 255 for full volume)
         * We keep it simple - use hardware volume controls! 🐰
         */
        void setVolume(uint8_t volume) { audioVolume = volume; }

    protected:
        /**
         * Main thread loop - receives RTP packets and manages audio playback
         */
        void run() override;

    private:
        // Core components
        std::shared_ptr<creatures::Logger> logger;

        // RTP configuration
        std::string multicastGroup;
        uint16_t rtpPort;
        uint8_t audioChannels;
        uint32_t sampleRate;

        // uvgRTP components
        uvgrtp::context rtpContext;
        uvgrtp::session* rtpSession = nullptr;
        uvgrtp::media_stream* rtpStream = nullptr;

        // SDL Audio components
        SDL_AudioDeviceID audioDevice = DEFAULT_SOUND_DEVICE_NUMBER;
        SDL_AudioSpec audioSpec{};

        // Audio buffer management
        struct AudioBuffer {
            std::vector<int16_t> data;
            uint32_t timestamp;
            size_t frameCount;
        };

        std::queue<AudioBuffer> audioBufferQueue;
        mutable std::mutex bufferMutex;
        std::condition_variable bufferCondition;

        // Thread state
        std::atomic<bool> receivingAudio{false};
        std::atomic<uint64_t> packetsReceived{0};
        uint8_t audioVolume = 255;  // Full volume by default - use hardware controls! 🐰

        // Configuration constants
        static constexpr size_t MAX_BUFFER_QUEUE_SIZE = 100;  // About 500ms at 5ms chunks
        static constexpr size_t TARGET_BUFFER_SIZE = 20;      // Target ~100ms buffer

        /**
         * Initialize SDL audio subsystem
         */
        bool initializeSDL();

        /**
         * Set up RTP receiving session
         */
        bool initializeRTP();

        /**
         * Clean up SDL resources
         */
        void cleanupSDL();

        /**
         * Clean up RTP resources
         */
        void cleanupRTP();

        /**
         * Process incoming RTP packet
         * This is where we catch those hopping packets! 🐰
         */
        void processRtpPacket(uint8_t* data, size_t size, uint32_t timestamp);

        /**
         * SDL audio callback - called when SDL needs more audio data
         */
        static void audioCallback(void* userdata, Uint8* stream, int len);

        /**
         * Fill audio buffer with data from our queue
         */
        void fillAudioBuffer(int16_t* buffer, size_t frameCount);

        /**
         * Log audio statistics periodically
         */
        void logAudioStats();

        /**
         * Check if we have enough audio buffered to start playback
         */
        bool hasEnoughBufferedAudio() const;
    };

} // namespace creatures::audio