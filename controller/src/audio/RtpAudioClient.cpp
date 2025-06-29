//
// RtpAudioClient.cpp
//

#include "RtpAudioClient.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "logging/Logger.h"
#include "util/thread_name.h"



namespace creatures::audio {

    RtpAudioClient::RtpAudioClient(std::shared_ptr<creatures::Logger> logger,
                                   uint8_t audioDevice,
                                   const std::string& multicastGroup,
                                   uint16_t port,
                                   uint8_t channels,
                                   uint32_t sampleRate)
        : logger(std::move(logger))
        , audioDevice(audioDevice)
        , multicastGroup(multicastGroup)
        , rtpPort(port)
        , audioChannels(channels)
        , sampleRate(sampleRate) {

        this->logger->debug("🐰 Hopping into RTP audio client setup!");
        this->logger->info("RTP Client Config: {}:{} - {} channels @ {}Hz (full volume, use hardware controls!)",
                          multicastGroup, port, channels, sampleRate);
    }

    RtpAudioClient::~RtpAudioClient() {
        shutdown();
    }

    void RtpAudioClient::start() {
        logger->info("🐰 Starting RTP audio client - time to catch some audio packets!");

        if (!initializeSDL()) {
            logger->error("Failed to initialize SDL audio - audio dreams dashed! 💔");
            return;
        }

        if (!initializeRTP()) {
            logger->error("Failed to initialize RTP client - no network nibbles for us!");
            cleanupSDL();
            return;
        }

        // Start the thread that will receive and process RTP packets
        StoppableThread::start();

        logger->info("🎵 RTP audio client is hopping and ready to receive audio!");
    }

    void RtpAudioClient::shutdown() {
        logger->info("🐰 Stopping RTP audio client - hop-ing off the network!");

        // Stop the thread first
        StoppableThread::shutdown();

        // Clean up resources
        cleanupRTP();
        cleanupSDL();

        logger->info("RTP audio client stopped cleanly");
    }

    float RtpAudioClient::getBufferLevel() const {
        std::lock_guard<std::mutex> lock(bufferMutex);
        return static_cast<float>(audioBufferQueue.size()) / static_cast<float>(MAX_BUFFER_QUEUE_SIZE);
    }

    void RtpAudioClient::run() {
        setThreadName("RtpAudioRx");
        logger->debug("🐰 RTP audio receive thread hopping into action!");

        auto lastStatsTime = std::chrono::steady_clock::now();

        while (!stop_requested) {
            try {
                // Receive RTP packet with timeout
                uvgrtp::frame::rtp_frame* frame = rtpStream->pull_frame(100); // 100ms timeout

                if (frame && frame->payload && frame->payload_len > 0) {
                    // Process the received audio packet
                    processRtpPacket(frame->payload, frame->payload_len, frame->header.timestamp);
                    packetsReceived++;

                    if (!receivingAudio.load()) {
                        logger->info("🎵 First audio packet received - the music starts hopping!");
                        receivingAudio.store(true);
                    }

                    // Release the frame back to uvgRTP
                    (void)uvgrtp::frame::dealloc_frame(frame);
                }

                // Log stats every 5 seconds
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now - lastStatsTime).count() >= 5) {
                    logAudioStats();
                    lastStatsTime = now;
                }

            } catch (const std::exception& e) {
                logger->warn("Exception in RTP receive loop: {} - but we keep hopping!", e.what());
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        logger->debug("🐰 RTP audio receive thread finishing its hop!");
    }

    bool RtpAudioClient::initializeSDL() {
        logger->debug("🐰 Initializing SDL audio - preparing our ears!");

        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            logger->error("Failed to initialize SDL: {}", SDL_GetError());
            return false;
        }

        // Configure audio specification
        SDL_AudioSpec desired{};
        desired.freq = static_cast<int>(sampleRate);
        desired.format = AUDIO_S16SYS;  // 16-bit signed, system byte order (matches L16)
        desired.channels = audioChannels;
        desired.samples = 1024;  // Buffer size in frames
        desired.callback = audioCallback;
        desired.userdata = this;

        // Open audio device
        // Get the name of the default
        auto audioDeviceName = SDL_GetAudioDeviceName(audioDevice, 0);
        if (!audioDeviceName) {
            logger->error("Failed to get audio device name: {}", SDL_GetError());
            return false;
        }
        logger->debug("Using audio device name: {}", audioDeviceName);


        audioDevice = SDL_OpenAudioDevice(audioDeviceName, 0, &desired, &audioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
        if (audioDevice == 0) {
            logger->error("Failed to open audio device: {}", SDL_GetError());
            SDL_Quit();
            return false;
        }

        logger->info("🎵 SDL Audio device opened successfully!");
        logger->debug("  • Device: {}", audioDeviceName ? audioDeviceName : "default");
        logger->debug("  • Sample Rate: {}Hz (requested: {}Hz)", audioSpec.freq, sampleRate);
        logger->debug("  • Channels: {} (requested: {})", audioSpec.channels, audioChannels);
        logger->debug("  • Format: {} (requested: AUDIO_S16SYS)", audioSpec.format);
        logger->debug("  • Buffer Size: {} frames", audioSpec.samples);
        logger->debug("  • Volume: {} (use hardware controls for adjustment! 🐰)", audioVolume);

        // Start audio playback (will call our callback when it needs data)
        SDL_PauseAudioDevice(audioDevice, 0);

        return true;
    }

    bool RtpAudioClient::initializeRTP() {
        logger->debug("🐰 Setting up RTP session - preparing to catch network packets!");

        try {
            // Create RTP session for multicast receiving
            rtpSession = rtpContext.create_session(multicastGroup);
            if (!rtpSession) {
                logger->error("Failed to create RTP session for multicast group: {}", multicastGroup);
                return false;
            }

            // Create receive-only stream
            rtpStream = rtpSession->create_stream(
                rtpPort, rtpPort,           // Local port, remote port (same for multicast)
                RTP_FORMAT_GENERIC,         // Generic format (we'll handle L16 ourselves)
                RCE_RECEIVE_ONLY           // We only receive, don't send
            );

            if (!rtpStream) {
                logger->error("Failed to create RTP stream on port: {}", rtpPort);
                rtpContext.destroy_session(rtpSession);
                rtpSession = nullptr;
                return false;
            }

            // Configure the stream for L16 audio
            rtpStream->configure_ctx(RCC_DYN_PAYLOAD_TYPE, 97);    // L16 dynamic payload type
            rtpStream->configure_ctx(RCC_CLOCK_RATE, sampleRate);  // Sample rate

            logger->info("🎵 RTP stream configured successfully!");
            logger->debug("  • Multicast Group: {}", multicastGroup);
            logger->debug("  • Port: {}", rtpPort);
            logger->debug("  • Payload Type: 97 (L16)");
            logger->debug("  • Sample Rate: {}Hz", sampleRate);

            return true;

        } catch (const std::exception& e) {
            logger->error("Exception setting up RTP: {} - network nibbles failed!", e.what());
            return false;
        }
    }

    void RtpAudioClient::cleanupSDL() {
        if (audioDevice != 0) {
            SDL_CloseAudioDevice(audioDevice);
            audioDevice = 0;
            logger->debug("🐰 SDL audio device closed");
        }

        SDL_Quit();
        logger->debug("SDL audio subsystem shut down");
    }

    void RtpAudioClient::cleanupRTP() {
        if (rtpStream && rtpSession) {
            rtpSession->destroy_stream(rtpStream);
            rtpStream = nullptr;
            logger->debug("🐰 RTP stream destroyed");
        }

        if (rtpSession) {
            rtpContext.destroy_session(rtpSession);
            rtpSession = nullptr;
            logger->debug("RTP session destroyed");
        }

        receivingAudio.store(false);
    }

    void RtpAudioClient::processRtpPacket(uint8_t* data, size_t size, uint32_t timestamp) {
        // Validate packet size (should be multiple of 2 bytes per sample * channels)
        size_t bytesPerFrame = sizeof(int16_t) * audioChannels;
        if (size % bytesPerFrame != 0) {
            logger->warn("🐰 Received RTP packet with invalid size: {} bytes (not divisible by {} bytes per frame)",
                        size, bytesPerFrame);
            return;
        }

        size_t frameCount = size / bytesPerFrame;

        // Create audio buffer
        AudioBuffer buffer;
        buffer.data.resize(frameCount * audioChannels);
        buffer.timestamp = timestamp;
        buffer.frameCount = frameCount;

        // Copy the L16 data directly (it's already in the right format)
        std::memcpy(buffer.data.data(), data, size);

        // Add to queue (with bounds checking to prevent memory issues)
        {
            std::lock_guard<std::mutex> lock(bufferMutex);

            if (audioBufferQueue.size() >= MAX_BUFFER_QUEUE_SIZE) {
                // Buffer is full - drop oldest packet to prevent memory bloat
                audioBufferQueue.pop();
                logger->trace("🐰 Audio buffer overflow - dropped oldest packet to make room");
            }

            audioBufferQueue.push(std::move(buffer));
        }

        // Wake up audio thread if it's waiting
        bufferCondition.notify_one();

        logger->trace("🎵 Processed RTP audio packet: {} frames ({} bytes) @ timestamp {}",
                     frameCount, size, timestamp);
    }

    void RtpAudioClient::audioCallback(void* userdata, Uint8* stream, int len) {
        auto* client = static_cast<RtpAudioClient*>(userdata);

        // Calculate how many frames we need
        size_t frameCount = len / (sizeof(int16_t) * client->audioChannels);
        auto* buffer = reinterpret_cast<int16_t*>(stream);

        // Fill the buffer with audio data
        client->fillAudioBuffer(buffer, frameCount);
    }

    void RtpAudioClient::fillAudioBuffer(int16_t* buffer, size_t frameCount) {
        size_t totalSamples = frameCount * audioChannels;

        // Start with silence
        std::memset(buffer, 0, totalSamples * sizeof(int16_t));

        size_t samplesWritten = 0;

        {
            std::lock_guard<std::mutex> lock(bufferMutex);

            // Fill buffer from our audio queue - no gain processing, just raw audio! 🐰
            while (samplesWritten < totalSamples && !audioBufferQueue.empty()) {
                AudioBuffer& audioBuffer = audioBufferQueue.front();

                size_t availableSamples = audioBuffer.data.size();
                size_t samplesToWrite = std::min(totalSamples - samplesWritten, availableSamples);

                // Copy audio data
                std::memcpy(buffer + samplesWritten, audioBuffer.data.data(),
                           samplesToWrite * sizeof(int16_t));

                samplesWritten += samplesToWrite;

                // Remove this buffer if we've used it all
                if (samplesToWrite >= availableSamples) {
                    audioBufferQueue.pop();
                } else {
                    // Partial use - remove the samples we used
                    audioBuffer.data.erase(audioBuffer.data.begin(),
                                         audioBuffer.data.begin() + samplesToWrite);
                }
            }
        }

        // Log if we had underrun (not enough audio data)
        if (samplesWritten < totalSamples) {
            logger->trace("🐰 Audio underrun: only {} of {} samples available",
                         samplesWritten, totalSamples);
        }
    }

    void RtpAudioClient::logAudioStats() {
        uint64_t packets = packetsReceived.load();
        float bufferLevel = getBufferLevel();
        bool receiving = receivingAudio.load();

        logger->info("🐰 Audio Stats: {} packets received, {:.1f}% buffer, receiving: {}, volume: {}",
                    packets, bufferLevel * 100.0f, receiving ? "yes" : "no", audioVolume);


        if (bufferLevel > 0.8f) {
            logger->warn("🐰 Audio buffer getting full ({:.1%}) - might have network congestion!",
                        bufferLevel);
        } else if (bufferLevel < 0.1f && receiving) {
            logger->warn("🐰 Audio buffer getting low ({:.1%}) - might have network issues!",
                        bufferLevel);
        }
    }

    bool RtpAudioClient::hasEnoughBufferedAudio() const {
        std::lock_guard<std::mutex> lock(bufferMutex);
        return audioBufferQueue.size() >= TARGET_BUFFER_SIZE;
    }

} // namespace creatures::audio