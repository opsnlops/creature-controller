//
// RtpAudioClient.cpp
//

#include "RtpAudioClient.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <unistd.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <sys/fcntl.h>

#include "logging/Logger.h"
#include "util/thread_name.h"

namespace creatures::audio {

    RtpAudioClient::RtpAudioClient(std::shared_ptr<creatures::Logger> logger,
                                   uint8_t audioDevice,
                                   const std::string& multicastGroup,
                                   uint16_t port,
                                   uint8_t channels,
                                   uint32_t sampleRate,
                                   const std::string& networkDevice)
        : logger(std::move(logger))
        , audioDevice(audioDevice)
        , multicastGroup(multicastGroup)
        , rtpPort(port)
        , audioChannels(channels)
        , sampleRate(sampleRate)
        , networkDevice(networkDevice)
    {
        this->logger->debug("Initializing RTP audio client");
        this->logger->info("RTP Client Config: {}:{} - {} channels @ {}Hz",
                          multicastGroup, port, channels, sampleRate);
    }

    RtpAudioClient::~RtpAudioClient() {
        shutdown();
    }

    void RtpAudioClient::start() {
        logger->info("Starting RTP audio client");

        if (!initializeSDL()) {
            logger->error("Failed to initialize SDL audio");
            return;
        }

        if (!initializeRTP()) {
            logger->error("Failed to initialize RTP client");
            cleanupSDL();
            return;
        }

        // Start the thread that will receive and process RTP packets
        StoppableThread::start();

        logger->info("RTP audio client started successfully");
    }

    void RtpAudioClient::shutdown() {
        logger->info("Stopping RTP audio client");

        // Stop the thread first
        StoppableThread::shutdown();

        // Clean up resources
        cleanupRTP();
        cleanupSDL();

        logger->info("RTP audio client stopped");
    }

    float RtpAudioClient::getBufferLevel() const {
        std::lock_guard<std::mutex> lock(bufferMutex);
        return static_cast<float>(audioBufferQueue.size()) / static_cast<float>(MAX_BUFFER_QUEUE_SIZE);
    }

   void RtpAudioClient::run() {
        setThreadName("RtpAudioRx");
        logger->debug("RTP audio receive thread started");

        auto lastStatsTime = std::chrono::steady_clock::now();

        int receiveAttempts = 0;
        int consecutiveTimeouts = 0;

        uint8_t buffer[65536];  // Large buffer for RTP packets

        fd_set readfds;
        struct timeval timeout;

        logger->info("Starting multicast reception loop");

        while (!stop_requested.load()) {
            try {
                receiveAttempts++;

                // Use select() for timeout
                FD_ZERO(&readfds);
                FD_SET(rawMulticastSocket, &readfds);
                timeout.tv_sec = 0;
                timeout.tv_usec = 100000;  // 100ms timeout

                int selectResult = select(rawMulticastSocket + 1, &readfds, nullptr, nullptr, &timeout);

                if (selectResult > 0 && FD_ISSET(rawMulticastSocket, &readfds)) {
                    // Data available - receive it
                    struct sockaddr_in fromAddr{};
                    socklen_t fromLen = sizeof(fromAddr);

                    ssize_t bytesReceived = recvfrom(rawMulticastSocket, buffer, sizeof(buffer), 0,
                                                   (struct sockaddr*)&fromAddr, &fromLen);

                    if (bytesReceived > 0) {
                        consecutiveTimeouts = 0;

                        char fromIP[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &fromAddr.sin_addr, fromIP, INET_ADDRSTRLEN);

                        logger->trace("Received {} bytes from {}:{}", bytesReceived, fromIP, ntohs(fromAddr.sin_port));

                        // Simple RTP header parsing (12 bytes minimum)
                        if (bytesReceived >= 12) {
                            uint8_t version = (buffer[0] >> 6) & 0x03;
                            uint8_t payloadType = buffer[1] & 0x7F;
                            uint16_t sequence = (buffer[2] << 8) | buffer[3];
                            uint32_t timestamp = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];

                            logger->trace("RTP Header: Version: {}, PT: {}, Seq: {}, TS: {}",
                                          version, payloadType, sequence, timestamp);

                            // Extract audio payload (skip 12-byte RTP header)
                            uint8_t* audioPayload = buffer + 12;
                            size_t audioSize = bytesReceived - 12;

                            if (audioSize > 0) {
                                processRtpPacket(audioPayload, audioSize, timestamp);
                                packetsReceived++;

                                if (!receivingAudio.load()) {
                                    logger->info("First audio packet received");
                                    receivingAudio.store(true);
                                }
                            }
                        }
                    }

                } else if (selectResult == 0) {
                    // Timeout
                    consecutiveTimeouts++;
                    if (consecutiveTimeouts % 50 == 0) {  // Log every 5 seconds (50 * 100ms)
                        logger->debug("Socket timeout count: {}", consecutiveTimeouts);
                    }
                } else {
                    // Error
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        logger->warn("Select error: {}", strerror(errno));
                    }
                }

                // Stats every 5 seconds
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now - lastStatsTime).count() >= 5) {
                    logAudioStats();
                    lastStatsTime = now;
                }

            } catch (const std::exception& e) {
                logger->warn("Exception in receive loop: {}", e.what());
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        logger->debug("RTP receive thread finished");
    }

    bool RtpAudioClient::initializeSDL() {
        logger->debug("Initializing SDL audio");

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
        auto audioDeviceName = SDL_GetAudioDeviceName(audioDevice, 0);
        if (!audioDeviceName) {
            logger->error("Failed to get audio device name: {}", SDL_GetError());
            return false;
        }
        logger->debug("Using audio device: {}", audioDeviceName);

        audioDevice = SDL_OpenAudioDevice(audioDeviceName, 0, &desired, &audioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
        if (audioDevice == 0) {
            logger->error("Failed to open audio device: {}", SDL_GetError());
            SDL_Quit();
            return false;
        }

        logger->info("SDL audio device opened successfully");
        logger->debug("Audio config: {}Hz, {} channels, {} samples buffer",
                     audioSpec.freq, audioSpec.channels, audioSpec.samples);

        // Start audio playback (will call our callback when it needs data)
        SDL_PauseAudioDevice(audioDevice, 0);

        return true;
    }

    bool RtpAudioClient::initializeRTP() {
        logger->debug("Setting up multicast socket");

        try {
            // Create raw UDP socket for multicast reception
            rawMulticastSocket = socket(AF_INET, SOCK_DGRAM, 0);
            if (rawMulticastSocket < 0) {
                logger->error("Failed to create multicast socket: {}", strerror(errno));
                return false;
            }

            // Enable socket reuse
            int reuse = 1;
            if (setsockopt(rawMulticastSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
                logger->warn("Failed to set SO_REUSEADDR: {}", strerror(errno));
            }

            // Bind to the multicast port
            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;  // Accept on any interface
            addr.sin_port = htons(rtpPort);

            if (bind(rawMulticastSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                logger->error("Failed to bind to port {}: {}", rtpPort, strerror(errno));
                close(rawMulticastSocket);
                rawMulticastSocket = -1;
                return false;
            }

            // Join the multicast group on the specific interface
            struct ip_mreq mreq{};
            if (inet_pton(AF_INET, multicastGroup.c_str(), &mreq.imr_multiaddr) != 1) {
                logger->error("Invalid multicast address: {}", multicastGroup);
                close(rawMulticastSocket);
                rawMulticastSocket = -1;
                return false;
            }

            if (inet_pton(AF_INET, "10.19.63.11", &mreq.imr_interface) != 1) {
                logger->error("Invalid interface address: 10.19.63.11");
                close(rawMulticastSocket);
                rawMulticastSocket = -1;
                return false;
            }

            if (setsockopt(rawMulticastSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
                logger->error("Failed to join multicast group: {}", strerror(errno));
                close(rawMulticastSocket);
                rawMulticastSocket = -1;
                return false;
            }

            // Set socket to non-blocking for timeout behavior
            int flags = fcntl(rawMulticastSocket, F_GETFL, 0);
            fcntl(rawMulticastSocket, F_SETFL, flags | O_NONBLOCK);

            logger->info("Multicast socket configured: {}:{} on interface 10.19.63.11",
                        multicastGroup, rtpPort);

            return true;

        } catch (const std::exception& e) {
            logger->error("Exception setting up multicast: {}", e.what());
            if (rawMulticastSocket >= 0) {
                close(rawMulticastSocket);
                rawMulticastSocket = -1;
            }
            return false;
        }
    }

    void RtpAudioClient::cleanupSDL() {
        if (audioDevice != 0) {
            SDL_CloseAudioDevice(audioDevice);
            audioDevice = 0;
            logger->debug("SDL audio device closed");
        }

        SDL_Quit();
        logger->debug("SDL audio subsystem shut down");
    }

    void RtpAudioClient::cleanupRTP() {
        if (rawMulticastSocket >= 0) {
            close(rawMulticastSocket);
            rawMulticastSocket = -1;
            logger->debug("Multicast socket closed");
        }
        receivingAudio.store(false);
    }

    void RtpAudioClient::processRtpPacket(uint8_t* data, size_t size, uint32_t timestamp) {
        // Validate packet size (should be multiple of 2 bytes per sample * channels)
        size_t bytesPerFrame = sizeof(int16_t) * audioChannels;
        if (size % bytesPerFrame != 0) {
            logger->warn("Invalid RTP packet size: {} bytes (not divisible by {} bytes per frame)",
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
                logger->trace("Audio buffer overflow - dropped oldest packet");
            }

            audioBufferQueue.push(std::move(buffer));
        }

        // Wake up audio thread if it's waiting
        bufferCondition.notify_one();

        logger->trace("Processed RTP packet: {} frames ({} bytes) @ TS {}",
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

            // Debug occasionally to avoid spam
            static int debugCounter = 0;
            if (++debugCounter % 2000 == 0) {  // Every 2000th call
                logger->trace("Audio queue size: {} (call #{})", audioBufferQueue.size(), debugCounter);
            }

            // Fill buffer from our audio queue
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

        // Only log underruns occasionally to avoid spam
        if (samplesWritten < totalSamples) {
            static int underrunCounter = 0;
            if (++underrunCounter % 200 == 0) {
                logger->trace("Audio underrun #{}: {} of {} samples available",
                             underrunCounter, samplesWritten, totalSamples);
            }
        }
    }

    void RtpAudioClient::logAudioStats() {
        uint64_t packets = packetsReceived.load();
        float bufferLevel = getBufferLevel();
        bool receiving = receivingAudio.load();

        logger->info("Audio stats: {} packets, {:.1f}% buffer, receiving: {}",
                    packets, bufferLevel * 100.0f, receiving ? "yes" : "no");

        if (bufferLevel > 0.8f) {
            logger->warn("Audio buffer high ({:.1f}%) - possible network congestion",
                        bufferLevel * 100.0f);
        } else if (bufferLevel < 0.1f && receiving) {
            logger->warn("Audio buffer low ({:.1f}%) - possible network issues",
                        bufferLevel * 100.0f);
        }
    }

    bool RtpAudioClient::hasEnoughBufferedAudio() const {
        std::lock_guard<std::mutex> lock(bufferMutex);
        return audioBufferQueue.size() >= TARGET_BUFFER_SIZE;
    }

} // namespace creatures::audio