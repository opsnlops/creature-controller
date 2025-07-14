//
// audio-config.h  — final tidy
//
#pragma once
#include <cstdint>

namespace creatures::audio {

    // ── Device ────────────────────────────────────────────────
    inline constexpr uint8_t  DEFAULT_SOUND_DEVICE_NUMBER = 0;   // SDL default

    // ── Network (match server) ────────────────────────────────
    inline constexpr char     DIALOG_GROUP_BASE[] = "239.19.63.";  // +1…+16
    inline constexpr char     BGM_GROUP[]         = "239.19.63.17";
    inline constexpr uint16_t RTP_PORT            = 5004;

    // ── Opus framing ──────────────────────────────────────────
    inline constexpr uint32_t SAMPLE_RATE  = 48'000;   // Hz
    inline constexpr uint16_t FRAME_MS     = 20;       // Opus ptime
    inline constexpr uint16_t FRAMES_PER_CHUNK =
        SAMPLE_RATE * FRAME_MS / 1000;                 // 480 samples
    inline constexpr uint8_t  OUTPUT_CH    = 1;        // mono out

    // (handy byte count if you ever need it)
    // inline constexpr size_t  CHUNK_BYTES = FRAMES_PER_CHUNK * sizeof(int16_t);

    // ── SDL queue / buffering ─────────────────────────────────
    inline constexpr size_t SDL_BUFFER_FRAMES = 2048;  // ≈21 ms
    inline constexpr size_t PREFILL_FRAMES    = 3;     // 30 ms warm-up

    // ── Monitoring thresholds ────────────────────────────────
    inline constexpr float  BUF_HIGH_WATERMARK = 0.8f;
    inline constexpr float  BUF_LOW_WATERMARK  = 0.1f;
    inline constexpr int    STATS_INTERVAL_SEC = 5;

} // namespace creatures::audio