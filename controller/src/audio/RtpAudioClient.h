/**
 * @file   RtpAudioClient.h
 * @brief  Multicast RTP (L16, 48 kHz) → SDL queue streamer.
 *
 * Streams a 17-channel program and mixes:
 *   • creature dialog  (selectable channel 1-16)
 *   • background music (channel 17)
 *
 * Pops are avoided by pushing 50 ms ahead through SDL_QueueAudio().
 * The class derives from StoppableThread; destroy or call shutdown()
 * to stop the worker.
 */
#pragma once

// ────── STL
#include <atomic>
#include <bit>
#include <cstdint>
#include <memory>
#include <string>

// ────── SDL2 (queue API only)
#include <SDL.h>

// ────── Project
#include "audio-config.h"
#include "logging/Logger.h"
#include "util/StoppableThread.h"

namespace creatures::audio {

/*──── Endian helper ────────────────────────────────────────────────*/
[[nodiscard]]
constexpr int16_t net_to_host_16(int16_t be) noexcept
{
    return static_cast<int16_t>(ntohs(static_cast<uint16_t>(be)));
}

/*──── Client class ────────────────────────────────────────────────*/
class RtpAudioClient final : public StoppableThread
{
public:
    /*-----------------------------------------------------------------
     *  Ctor with *eight* parameters  (matches AudioSubsystem.cpp)
     *----------------------------------------------------------------*/
    explicit RtpAudioClient(
        std::shared_ptr<creatures::Logger> log,
        uint8_t            audioDevice        = DEFAULT_SOUND_DEVICE_NUMBER,
        std::string        mcastGroup         = "239.19.63.1",
        uint16_t           rtpPort            = 5004,
        uint8_t            totalChannels      = 17,
        uint32_t           sampleRateHz       = 48'000,
        std::string        ifaceIp            = "10.19.63.11",
        uint8_t            creatureChannel    = 1) noexcept;

    ~RtpAudioClient() override;

    /* StoppableThread */
    void run() override;

    /* Runtime knobs   (needed by AudioSubsystem) */
    void           setCreatureChannel(uint8_t ch)           noexcept;
    void           setVolume(int v /*0-128*/)               noexcept;
    void           setAudioDevice(uint8_t)                  noexcept {/*stub*/}
    [[nodiscard]] bool        isReceiving()        const    noexcept;
    [[nodiscard]] uint64_t    getPacketsReceived() const    noexcept;
    [[nodiscard]] float       getBufferLevel()     const    noexcept;

    /* Compile-time constants */
    static constexpr uint32_t SAMPLE_RATE        = 48'000;   // Hz
    static constexpr uint8_t  STREAM_CH_MAX      = 17;       // channels
    static constexpr uint16_t FRAME_MS           = 5;        // ms
    static constexpr uint16_t FRAMES_PER_CHUNK   =
        SAMPLE_RATE * FRAME_MS / 1000;                       // 240
    static constexpr int      MAX_VOLUME         = 128;      // SDL range

private:
    /* Init / teardown */
    bool init_socket();
    void close_socket()                noexcept;
    bool init_sdl();
    void shutdown_sdl()                noexcept;

    /* Helpers */
    bool recv_packet(std::vector<std::uint8_t>& buf);
    void deinterleave_and_mix(const int16_t* frameBE, std::size_t frames);
    void queue_pcm(const int16_t* pcm, std::size_t frames)    noexcept;

    /* Immutable after construction */
    std::shared_ptr<creatures::Logger> log_;
    const uint8_t      audio_dev_idx_;
    const std::string  mcast_group_;
    const uint16_t     rtp_port_;
    const uint8_t      total_channels_;
    const std::string  iface_ip_;

    /* Runtime state */
    std::atomic<uint64_t> packets_rx_{0};
    std::atomic<bool>     running_{false};
    SDL_AudioDeviceID     dev_{0};
    int                   volume_{MAX_VOLUME};
    uint8_t               creature_ch_{1};          // 1-16
    int                   sock_{-1};

    void log_stream_stats() const;

    /* Buffers */
    static constexpr std::size_t QUEUE_TARGET_BYTES =
        SAMPLE_RATE * sizeof(int16_t) * 24 / 100;  // 480 ms = 46 080 B
    int16_t mix_buf_[FRAMES_PER_CHUNK]{};            // scratch

    /* Endian sanity */
    static_assert(std::endian::native == std::endian::little,
                  "Tested only on little-endian hosts.");
};

} // namespace creatures::audio