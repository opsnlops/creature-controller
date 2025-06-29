//
// audio-config.h
// Configuration constants for RTP audio reception in creature-controller
// These should match the server settings for optimal compatibility!
//

#pragma once

#include <cstdint>

namespace creatures::audio {

    // RTP Network Configuration
    // These should match the creature-server's RTP settings exactly!
    static constexpr char       DEFAULT_MULTICAST_GROUP[] = "239.19.63.1";
    static constexpr uint16_t   DEFAULT_RTP_PORT = 5004;

    // Audio Format Configuration
    // Must match the L16 format from creature-server
    static constexpr uint32_t   AUDIO_SAMPLE_RATE = 48000;     // 48kHz
    static constexpr uint8_t    STREAM_AUDIO_CHANNELS = 17;           // 16 creatures + 1 BGM
    static constexpr uint16_t   AUDIO_BITS_PER_SAMPLE = 16;    // L16 format

    // Audio Volume Configuration
    // Keep it simple - full volume, use hardware controls! 🐰
    static constexpr uint8_t    DEFAULT_VOLUME = 255;          // Maximum volume

    static constexpr uint8_t    DEFAULT_SOUND_DEVICE_NUMBER = 0; // Default sound device index
    static constexpr uint8_t    DEVICE_AUDIO_CHANNELS = 2; // Number of audio channels

    // Buffer Management
    // Tuned for 1ms controller timing and 5ms server chunks
    static constexpr size_t     MAX_AUDIO_BUFFER_QUEUE = 100;  // ~500ms worth at 5ms chunks
    static constexpr size_t     TARGET_BUFFER_SIZE = 20;       // ~100ms target buffer
    static constexpr size_t     MIN_BUFFER_START = 5;          // Start playback after 5 chunks

    // SDL Audio Configuration
    static constexpr size_t     SDL_BUFFER_FRAMES = 1024;      // SDL callback buffer size
    static constexpr int        SDL_FREQUENCY = AUDIO_SAMPLE_RATE;
    static constexpr uint16_t   SDL_FORMAT = 0x8010;           // AUDIO_S16SYS

    // Performance Tuning for Soft Real-Time
    // These values are tuned for 1ms timing precision 🐰
    static constexpr int        RTP_RECEIVE_TIMEOUT_MS = 100;  // Don't block too long
    static constexpr int        STATS_LOG_INTERVAL_SEC = 10;   // Log stats every 10 seconds
    static constexpr int        AUDIO_THREAD_PRIORITY = 1;     // Slightly higher than normal

    // Network Quality Monitoring
    static constexpr float      BUFFER_HIGH_WATERMARK = 0.8f;  // Warn if buffer > 80%
    static constexpr float      BUFFER_LOW_WATERMARK = 0.1f;   // Warn if buffer < 10%
    static constexpr uint32_t   PACKET_LOSS_WARNING = 100;     // Warn if we lose 100+ packets

    // Expected packet timing (must match server RTP_FRAME_MS)
    static constexpr int        EXPECTED_PACKET_INTERVAL_MS = 5;  // 5ms chunks from server
    static constexpr size_t     EXPECTED_PACKET_SIZE = AUDIO_SAMPLE_RATE * EXPECTED_PACKET_INTERVAL_MS / 1000 *
                                                      sizeof(int16_t) * STREAM_AUDIO_CHANNELS;  // Should be 8160 bytes

    // Compile-time validation (because rabbits are careful! 🐰)
    static_assert(STREAM_AUDIO_CHANNELS > 0, "Must have at least one audio channel");
    static_assert(AUDIO_SAMPLE_RATE > 0, "Sample rate must be positive");
    static_assert(MAX_AUDIO_BUFFER_QUEUE > TARGET_BUFFER_SIZE, "Max buffer must be larger than target");
    static_assert(TARGET_BUFFER_SIZE > MIN_BUFFER_START, "Target buffer must be larger than start threshold");

    // Helper function to get human-readable format info
    inline const char* getAudioFormatDescription() {
        return "L16 17-channel @ 48kHz (creature-server compatible)";
    }

    // Helper function to calculate expected bandwidth
    inline uint32_t getExpectedBandwidthBps() {
        // bytes per second = sample_rate * channels * bytes_per_sample
        return AUDIO_SAMPLE_RATE * STREAM_AUDIO_CHANNELS * (AUDIO_BITS_PER_SAMPLE / 8);
    }

    // Helper function to get expected packet rate
    inline float getExpectedPacketRateHz() {
        return 1000.0f / static_cast<float>(EXPECTED_PACKET_INTERVAL_MS);  // Should be 200 Hz
    }

} // namespace creatures::audio

// Friendly reminder comments for integration 🐰
/*
 * 🐰 Integration Notes:
 *
 * 1. Make sure your creature-server is configured with:
 *    - RTP_MULTICAST_GROUP = "239.19.63.1"
 *    - RTP_PORT = 5004
 *    - RTP_STREAMING_CHANNELS = 17
 *    - RTP_SRATE = 48000
 *    - RTP_FRAME_MS = 5
 *
 * 2. Network requirements:
 *    - Multicast must be enabled on your network
 *    - Expected bandwidth: ~1.6 Mbps per client
 *    - Packet rate: 200 packets/second
 *
 * 3. For best performance:
 *    - Use wired Ethernet if possible
 *    - Enable jumbo frames if available
 *    - Consider network QoS for audio traffic
 *    - Set speaker volume using hardware controls (software streams at full volume)
 *
 * 4. Troubleshooting:
 *    - Audio is streamed at full volume (255)
 *    - Use hardware volume controls on your speakers
 *    - No software gain processing for maximum fidelity
 *    - Check firewall settings for multicast
 *    - Verify network interfaces support multicast
 *    - Monitor packet loss with --verbose logging
 */