//
// audio-config.h
//
#pragma once
#include <cstdint>

namespace creatures::audio {

// ── Device ────────────────────────────────────────────────
inline constexpr uint8_t DEFAULT_SOUND_DEVICE_NUMBER = 0; // SDL default

// ── Network (match server) ────────────────────────────────
inline constexpr char DIALOG_GROUP_BASE[] = "239.19.63."; // +1…+16
inline constexpr char BGM_GROUP[] = "239.19.63.17";
inline constexpr uint16_t RTP_PORT = 5004;

// ── Opus framing ──────────────────────────────────────────
inline constexpr uint32_t SAMPLE_RATE = 48'000;                             // Hz
inline constexpr uint16_t FRAME_MS = 10;                                    // Opus ptime
inline constexpr uint16_t FRAMES_PER_CHUNK = SAMPLE_RATE * FRAME_MS / 1000; // 480 samples
inline constexpr uint8_t OUTPUT_CH = 1;                                     // mono out

// (handy byte count if you ever need it)
// inline constexpr size_t  CHUNK_BYTES = FRAMES_PER_CHUNK * sizeof(int16_t);

// ── SDL queue / buffering ─────────────────────────────────
inline constexpr size_t SDL_BUFFER_FRAMES = 2048; // ~20 ms
inline constexpr size_t PREFILL_FRAMES = 3;       // 30 ms warm-up

// ── Monitoring thresholds ────────────────────────────────
inline constexpr float BUF_HIGH_WATERMARK = 0.8f;
inline constexpr float BUF_LOW_WATERMARK = 0.1f;

// How often the monitoring loop samples the client. Detailed stats are emitted
// at debug on every pass; the info-level summary is far less frequent so a
// healthy system stays quiet in the normal log.
inline constexpr int STATS_INTERVAL_SEC = 5;
inline constexpr int SUMMARY_INTERVAL_SEC = 60;
inline constexpr int SUMMARY_EVERY_N_SAMPLES = SUMMARY_INTERVAL_SEC / STATS_INTERVAL_SEC;

// Mixer reporting, counted in mixed frames. Each frame is FRAME_MS of audio.
inline constexpr uint64_t MIX_STATS_FRAME_INTERVAL = 250;
inline constexpr uint64_t MIX_SUMMARY_FRAME_INTERVAL = MIX_STATS_FRAME_INTERVAL * 24;

} // namespace creatures::audio