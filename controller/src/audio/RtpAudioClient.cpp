/**
 * RtpAudioClient.cpp – 2025-06-30
 * Multicast RTP 17-channel (L16 / 48 kHz) → SDL queue streamer
 *  • Handles uvgrtp 10-byte per-fragment headers
 *  • Builds 5 ms mono blocks (240 frames) in a jitter FIFO
 *  • Pre-buffers 3 blocks (15 ms) before starting playback
 *  • Works on macOS (ip_mreq) and Linux (ip_mreqn)
 */

#include "RtpAudioClient.h"

// ─── POSIX / STL ────────────────────────────────────────────────────
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

// ─── Project helpers ───────────────────────────────────────────────
#include "util/thread_name.h"

namespace ca = creatures::audio;

/*───────────────────────────────────────────────────────────────
 *  ctor / dtor  (unchanged from header)
 *─────────────────────────────────────────────────────────────*/
ca::RtpAudioClient::RtpAudioClient(
        std::shared_ptr<creatures::Logger> log,
        uint8_t  audioDev,
        std::string mc,
        uint16_t port,
        uint8_t  chTot,
        uint32_t sr,
        std::string ifaceIp,
        uint8_t  dlgCh) noexcept
    : log_(std::move(log))
    , audio_dev_idx_(audioDev)
    , mcast_group_(std::move(mc))
    , rtp_port_(port)
    , total_channels_(std::clamp<uint8_t>(chTot, 2u, STREAM_CH_MAX))
    , iface_ip_(std::move(ifaceIp))
    , creature_ch_(std::clamp<uint8_t>(dlgCh, 1u, 16u))
{
    (void)sr; // sample-rate is compile-time constant for now
}

ca::RtpAudioClient::~RtpAudioClient()
{
    shutdown();
    shutdown_sdl();
    close_socket();
}

/*───────────────────────────────────────────────────────────────
 *  public helpers  (simple setters / getters)
 *─────────────────────────────────────────────────────────────*/
void ca::RtpAudioClient::setCreatureChannel(uint8_t c) noexcept
{ creature_ch_ = std::clamp<uint8_t>(c, 1u, 16u); }

void ca::RtpAudioClient::setVolume(int) noexcept {/* fixed at max */}
bool ca::RtpAudioClient::isReceiving() const noexcept { return running_; }
uint64_t ca::RtpAudioClient::getPacketsReceived() const noexcept { return packets_rx_; }
float ca::RtpAudioClient::getBufferLevel() const noexcept
{
    return static_cast<float>(SDL_GetQueuedAudioSize(dev_)) /
           static_cast<float>(QUEUE_TARGET_BYTES);
}

/*───────────────────────────────────────────────────────────────
 *  thread entry
 *─────────────────────────────────────────────────────────────*/
void ca::RtpAudioClient::run()
{
    setThreadName("rtp-client");

    if (!init_sdl() || !init_socket()) return;
    running_ = true;

    std::vector<std::uint8_t> pkt(9216);          // jumbo-safe
    std::vector<int16_t>      jitter;             // FIFO of mono samples

    constexpr std::size_t RTP_HDR   = 12;
    const std::size_t tupleBytes    = total_channels_ * sizeof(int16_t);
    bool primed                     = false;      // warm-up flag

    while (!stop_requested.load()) {
        /* ── receive one UDP datagram ──────────────────────────── */
        if (!recv_packet(pkt)) continue;
        std::size_t payload = pkt.size() - RTP_HDR;
        if (!payload) continue;

        /* uvgrtp adds 10-B header at start of each fragment */
        std::size_t skip   = payload % tupleBytes;
        std::size_t frames = (payload - skip) / tupleBytes;
        if (!frames) continue;

        ++packets_rx_;

        const auto* frameBE =
            reinterpret_cast<const int16_t*>(pkt.data() + RTP_HDR + skip);

        deinterleave_and_mix(frameBE, frames);
        jitter.insert(jitter.end(), mix_buf_, mix_buf_ + frames);

        /* ── feed SDL in 240-frame (5 ms) blocks ──────────────── */
        while (jitter.size() >= FRAMES_PER_CHUNK) {
            queue_pcm(jitter.data(), FRAMES_PER_CHUNK);
            jitter.erase(jitter.begin(),
                         jitter.begin() + FRAMES_PER_CHUNK);
        }

        /* ── warm-up: queue ≥3 blocks (15 ms) *before* un-pausing ─*/
        if (!primed) {
            if (SDL_GetQueuedAudioSize(dev_) >=
                FRAMES_PER_CHUNK * 3 * sizeof(int16_t)) {
                SDL_PauseAudioDevice(dev_, 0);    // start playback
                primed = true;
                log_->info("audio primed, playback started");
            }
        } else {
            log_stream_stats();
        }
    }
    running_ = false;
}

/*───────────────────────────────────────────────────────────────
 *  SDL initialisation / shutdown
 *─────────────────────────────────────────────────────────────*/
bool ca::RtpAudioClient::init_sdl()
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        log_->error("SDL audio init: {}", SDL_GetError()); return false;
    }
    SDL_AudioSpec want{}, have{};
    want.freq     = static_cast<int>(SAMPLE_RATE);
    want.format   = AUDIO_S16SYS;
    want.channels = 1;
    want.samples  = 1024;
    want.callback = nullptr;               // queue mode

    dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!dev_) { log_->error("SDL_OpenAudioDevice: {}", SDL_GetError()); return false; }

    SDL_PauseAudioDevice(dev_, 1);         // start paused (we’ll un-pause after warm-up)
    log_->info("SDL audio opened: {} Hz mono queue", have.freq);
    return true;
}

void ca::RtpAudioClient::shutdown_sdl() noexcept
{
    if (dev_) SDL_CloseAudioDevice(dev_);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

/*───────────────────────────────────────────────────────────────
 *  Socket initialisation / recv  (unchanged)
 *─────────────────────────────────────────────────────────────*/
bool ca::RtpAudioClient::init_socket()
{
    sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ < 0) { log_->error("socket: {}", strerror(errno)); return false; }

    int reuse = 1;
    setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in bindAddr{};
    bindAddr.sin_family      = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddr.sin_port        = htons(rtp_port_);
    if (bind(sock_, reinterpret_cast<sockaddr*>(&bindAddr),
             sizeof(bindAddr))) {
        log_->error("bind: {}", strerror(errno)); return false;
    }

#ifdef __APPLE__                              // macOS / BSD
    ip_mreq m{};
    m.imr_multiaddr.s_addr = inet_addr(mcast_group_.c_str());
    m.imr_interface.s_addr = inet_addr(iface_ip_.c_str());
#else                                          // Linux
    ip_mreqn m{};
    m.imr_multiaddr.s_addr = inet_addr(mcast_group_.c_str());
    m.imr_address.s_addr   = inet_addr(iface_ip_.c_str());
    m.imr_ifindex          = if_nametoindex("eth0"); // fallback
#endif
    if (setsockopt(sock_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &m, sizeof(m))) {
        log_->error("IP_ADD_MEMBERSHIP: {}", strerror(errno)); return false;
    }

    int fl = fcntl(sock_, F_GETFL, 0);
    fcntl(sock_, F_SETFL, fl | O_NONBLOCK);
    log_->info("RTP joined {}:{} via {}", mcast_group_, rtp_port_, iface_ip_);
    return true;
}

void ca::RtpAudioClient::close_socket() noexcept
{ if (sock_ >= 0) ::close(sock_); }

bool ca::RtpAudioClient::recv_packet(std::vector<std::uint8_t>& buf)
{
    fd_set rfds; FD_ZERO(&rfds); FD_SET(sock_, &rfds);
    timeval tv{0, 50'000};
    if (select(sock_+1, &rfds, nullptr, nullptr, &tv) <= 0) return false;

    ssize_t n = ::recv(sock_, buf.data(), buf.size(), 0);
    if (n <= 0) return false;
    buf.resize(static_cast<std::size_t>(n));
    return true;
}

/*──────────────────────────────────────────────────────────────
 *  Mix helpers  (dialog 100 %, bgm 50 %)
 *────────────────────────────────────────────────────────────*/
void ca::RtpAudioClient::deinterleave_and_mix(const int16_t* be,
                                              std::size_t frames)
{
    const uint8_t dlgIdx = static_cast<uint8_t>(creature_ch_ - 1);
    for (std::size_t f = 0; f < frames; ++f) {
        const int16_t* in = be + f * total_channels_;
        int32_t dlg = net_to_host_16(in[dlgIdx]);
        int32_t bgm = net_to_host_16(in[STREAM_CH_MAX - 1]) >> 1;
        int32_t mix = dlg + bgm;
        mix_buf_[f] = static_cast<int16_t>(
            std::clamp(mix, -32768, 32767));
    }
}

void ca::RtpAudioClient::queue_pcm(const int16_t* pcm,
                                   std::size_t frames) noexcept
{
    SDL_QueueAudio(dev_, pcm,
                   static_cast<Uint32>(frames * sizeof(int16_t)));
}

/*──────────────────────────────────────────────────────────────
 *  stats every 100 pkts
 *────────────────────────────────────────────────────────────*/
void ca::RtpAudioClient::log_stream_stats() const
{
    constexpr uint64_t EVERY = 100;
    if (packets_rx_ % EVERY) return;

    Uint32 q = SDL_GetQueuedAudioSize(dev_);
    log_->info("pkts={}  queued={} B  buf={:.0f} %",
               packets_rx_.load(),
               q,
               100.0f * static_cast<float>(q) / QUEUE_TARGET_BYTES);
}