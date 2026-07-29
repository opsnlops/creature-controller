//
// audio-config.h
//
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace creatures::audio {

// Device
inline constexpr uint8_t DEFAULT_SOUND_DEVICE_NUMBER = 0;

// Network (match server)
inline constexpr char DIALOG_GROUP_BASE[] = "239.19.63."; // +1...+16
inline constexpr char BGM_GROUP[] = "239.19.63.17";
inline constexpr uint16_t RTP_PORT = 5004;
inline constexpr uint8_t RTP_OPUS_PAYLOAD_TYPE = 96;
inline constexpr size_t MAX_RTP_PACKET_SIZE = 2048;

// Opus framing
inline constexpr uint32_t SAMPLE_RATE = 48'000;
inline constexpr uint16_t FRAME_MS = 10;
inline constexpr uint16_t FRAMES_PER_CHUNK = SAMPLE_RATE * FRAME_MS / 1000;
inline constexpr uint8_t OUTPUT_CH = 1;

// Playout
inline constexpr size_t RTP_JITTER_BUFFER_FRAMES = 32;
inline constexpr size_t INITIAL_PLAYOUT_FRAMES = 2;
inline constexpr size_t TARGET_PLAYOUT_FRAMES = 2;
inline constexpr size_t AUDIO_DEVICE_PERIOD_FRAMES = 256;
inline constexpr size_t AUDIO_OUTPUT_RING_FRAMES = 8192;
inline constexpr uint16_t PACKET_WAIT_MS = 2;
inline constexpr uint16_t STREAM_IDLE_TIMEOUT_MS = 250;
inline constexpr uint16_t GAIN_RAMP_MS = 2;

// Mixer
inline constexpr float DEFAULT_DIALOG_GAIN_DB = 0.0f;
inline constexpr float DEFAULT_BGM_GAIN_DB = 0.0f;
inline constexpr float DEFAULT_LIMITER_CEILING_DB = -1.0f;
inline constexpr float MIN_GAIN_DB = -90.0f;
inline constexpr float MAX_GAIN_DB = 12.0f;

struct AudioConfig {
    uint8_t deviceNumber{DEFAULT_SOUND_DEVICE_NUMBER};
    std::optional<std::string> deviceName;
    float dialogGainDb{DEFAULT_DIALOG_GAIN_DB};
    float bgmGainDb{DEFAULT_BGM_GAIN_DB};
    float limiterCeilingDb{DEFAULT_LIMITER_CEILING_DB};
    std::optional<uint8_t> outputVolumePercent;
    std::string alsaMixerCard{"default"};
    std::string alsaMixerElement;
};

// Monitoring
inline constexpr int STATS_INTERVAL_SEC = 5;
inline constexpr int SUMMARY_INTERVAL_SEC = 60;
inline constexpr int SUMMARY_EVERY_N_SAMPLES = SUMMARY_INTERVAL_SEC / STATS_INTERVAL_SEC;

inline constexpr uint64_t MIX_STATS_FRAME_INTERVAL = 250;
inline constexpr uint64_t MIX_SUMMARY_FRAME_INTERVAL = MIX_STATS_FRAME_INTERVAL * 24;

} // namespace creatures::audio
