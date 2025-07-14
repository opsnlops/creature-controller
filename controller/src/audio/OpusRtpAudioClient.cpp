//
// OpusRtpAudioClient.cpp
//

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "util/thread_name.h"

#include "OpusRtpAudioClient.h"

using namespace creatures::audio;

/* ── ctor / dtor ──────────────────────────────────────────── */
OpusRtpAudioClient::OpusRtpAudioClient(
        std::shared_ptr<creatures::Logger> log,
        std::string dlg, std::string bgm,
        uint16_t port, uint8_t dlgIdx,
        std::string iface)
    : log_(std::move(log))
    , dlgGrp_(std::move(dlg))
    , bgmGrp_(std::move(bgm))
    , ifaceIp_(std::move(iface))
    , port_(port)
    , dialogIdx_(dlgIdx)
{
    log_->debug("Created OpusRtpAudioClient: dialog={}, bgm={}, port={}, idx={}",
                dlgGrp_, bgmGrp_, port_, dialogIdx_);
}

OpusRtpAudioClient::~OpusRtpAudioClient()
{
    stop_requested.store(true);
    if (sockDlg_>=0) close(sockDlg_);
    if (sockBgm_>=0) close(sockBgm_);
    if (decDlg_) opus_decoder_destroy(decDlg_);
    if (decBgm_) opus_decoder_destroy(decBgm_);
    if (dev_)    SDL_CloseAudioDevice(dev_);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    log_->debug("OpusRtpAudioClient destroyed, {} SSRC resets occurred", ssrcResets_.load());
}

/* ── thread entry ────────────────────────────────────────── */
void OpusRtpAudioClient::run()
{
    setThreadName("opus-rtp");

    /* SDL queue (paused) */
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_AudioSpec want{}, have{};
    want.freq = SAMPLE_RATE;
    want.channels = 1;
    want.format = AUDIO_S16SYS;
    want.samples = SDL_BUFFER_FRAMES;
    dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!dev_) {
        log_->error("SDL_OpenAudioDevice failed: {}", SDL_GetError());
        return;
    }
    SDL_PauseAudioDevice(dev_, 1);

    /* Opus decoders */
    int err = 0;
    decDlg_ = opus_decoder_create(SAMPLE_RATE, 1, &err);
    if (err) {
        log_->error("Failed to create dialog Opus decoder: {}", err);
        return;
    }

    decBgm_ = opus_decoder_create(SAMPLE_RATE, 1, &err);
    if (err) {
        log_->error("Failed to create BGM Opus decoder: {}", err);
        return;
    }

    /* Sockets */
    if (!openSocket(sockDlg_, dlgGrp_) || !openSocket(sockBgm_, bgmGrp_)) {
        log_->error("Failed to open RTP sockets");
        return;
    }

    log_->info("RTP audio client started successfully");

    std::vector<uint8_t> bufDlg(1500), bufBgm(1500);
    running_.store(true);

    while (!stop_requested.load()) {
        bool gotDlg = recvPkt(sockDlg_, bufDlg);
        bool gotBgm = recvPkt(sockBgm_, bufBgm);

        /* Process BGM packet if available */
        if (gotBgm && isValidRtpPacket(bufBgm)) {
            uint32_t currentBgmSSRC = extractSSRC(bufBgm);
            checkAndHandleSSRCChange(currentBgmSSRC, decBgm_, lastBgmSSRC_, "BGM");

            // Decode BGM packet and store latest slice
            int frames = opus_decode(decBgm_,
                                   bufBgm.data() + 12, bufBgm.size() - 12,
                                   bgmBuf_.data(), FRAMES_PER_CHUNK, 0);

            if (frames > 0) {
                ++pkts_;
            } else {
                log_->warn("BGM decode failed: {}", frames);
            }
        }

        /* Process dialog packet if available */
        if (gotDlg && isValidRtpPacket(bufDlg)) {
            uint32_t currentDlgSSRC = extractSSRC(bufDlg);
            checkAndHandleSSRCChange(currentDlgSSRC, decDlg_, lastDialogSSRC_, "Dialog");

            // Decode dialog packet
            int16_t dlgPCM[FRAMES_PER_CHUNK];
            int frames = opus_decode(decDlg_,
                                   bufDlg.data() + 12, bufDlg.size() - 12,
                                   dlgPCM, FRAMES_PER_CHUNK, 0);

            if (frames == FRAMES_PER_CHUNK) {
                mixAndQueue(dlgPCM, bgmBuf_.data(), frames);
                ++pkts_;
            } else if (frames > 0) {
                // Handle partial frame
                mixAndQueue(dlgPCM, bgmBuf_.data(), frames);
                ++pkts_;
            } else {
                log_->warn("Dialog decode failed: {}", frames);
            }
        }

        // Update buffer level
        bufLvl_ = static_cast<float>(SDL_GetQueuedAudioSize(dev_)) /
                  (SAMPLE_RATE * sizeof(int16_t));

        // Start audio playback once we have enough data buffered
        if (SDL_GetQueuedAudioSize(dev_) >= PREFILL_FRAMES * sizeof(int16_t)) {
            SDL_PauseAudioDevice(dev_, 0);
        }
    }

    log_->info("RTP audio client stopping");
    running_.store(false);
}

/* ── RTP packet validation and SSRC extraction ─────────────── */
bool OpusRtpAudioClient::isValidRtpPacket(const std::vector<uint8_t>& packet) const
{
    if (packet.size() < 12) {
        return false;
    }

    // Check RTP version (should be 2)
    uint8_t version = (packet[0] >> 6) & 0x03;
    return version == 2;
}

uint32_t OpusRtpAudioClient::extractSSRC(const std::vector<uint8_t>& packet) const
{
    if (packet.size() < 12) {
        return 0;
    }

    // SSRC is at bytes 8-11 in network byte order
    return (static_cast<uint32_t>(packet[8]) << 24) |
           (static_cast<uint32_t>(packet[9]) << 16) |
           (static_cast<uint32_t>(packet[10]) << 8) |
           static_cast<uint32_t>(packet[11]);
}

/* ── SSRC change detection and decoder reset ───────────────── */
void OpusRtpAudioClient::checkAndHandleSSRCChange(uint32_t newSSRC,
                                                  OpusDecoder* decoder,
                                                  uint32_t& lastSSRC,
                                                  const std::string& streamName)
{
    bool isInitialized = (streamName == "Dialog") ? dialogSSRCInitialized_ : bgmSSRCInitialized_;

    if (!isInitialized) {
        // First packet for this stream
        lastSSRC = newSSRC;
        if (streamName == "Dialog") {
            dialogSSRCInitialized_ = true;
        } else {
            bgmSSRCInitialized_ = true;
        }
        log_->info("{} stream initialized with SSRC: {}", streamName, newSSRC);
        return;
    }

    if (newSSRC != lastSSRC) {
        // SSRC changed - reset decoder and clear audio queue
        log_->info("{} SSRC changed: {} -> {}, resetting decoder",
                  streamName, lastSSRC, newSSRC);

        opus_decoder_ctl(decoder, OPUS_RESET_STATE);

        // Clear audio queue only on dialog SSRC change to avoid audio glitches
        if (streamName == "Dialog") {
            SDL_ClearQueuedAudio(dev_);
            log_->debug("Cleared audio queue due to dialog SSRC change");
        }

        // Zero out BGM buffer if BGM stream resets
        if (streamName == "BGM") {
            std::fill(bgmBuf_.begin(), bgmBuf_.end(), 0);
            log_->debug("Cleared BGM buffer due to BGM SSRC change");
        }

        lastSSRC = newSSRC;
        ssrcResets_.fetch_add(1);
    }
}

/* ── socket helpers ───────────────────────────────────────── */
bool OpusRtpAudioClient::openSocket(int& sock, const std::string& group)
{
    sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        log_->error("Failed to create socket: {}", strerror(errno));
        return false;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        log_->error("Failed to bind socket to port {}: {}", port_, strerror(errno));
        return false;
    }

    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(group.c_str());
    mreq.imr_interface.s_addr = inet_addr(ifaceIp_.c_str());

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))) {
        log_->error("Failed to join multicast group {}: {}", group, strerror(errno));
        return false;
    }

    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

    log_->debug("Successfully joined multicast group: {}", group);
    return true;
}

bool OpusRtpAudioClient::recvPkt(int sock, std::vector<uint8_t>& pkt)
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    timeval timeout{0, 5000}; // 5ms timeout
    if (select(sock + 1, &readfds, nullptr, nullptr, &timeout) <= 0) {
        return false;
    }

    ssize_t bytesReceived = ::recv(sock, pkt.data(), pkt.size(), 0);
    if (bytesReceived <= 0) {
        return false;
    }

    pkt.resize(static_cast<size_t>(bytesReceived));
    return true;
}

/* ── mix & queue ─────────────────────────────────────────── */
void OpusRtpAudioClient::mixAndQueue(const int16_t* dlg,
                                     const int16_t* bgm,
                                     int frames)
{
    int16_t mixed[FRAMES_PER_CHUNK];
    for (int i = 0; i < frames; ++i) {
        // Mix dialog and BGM
        int32_t mixedSample = dlg[i] + bgm[i];
        mixed[i] = std::clamp(mixedSample, -32768, 32767);
    }
    SDL_QueueAudio(dev_, mixed, frames * sizeof(int16_t));
}