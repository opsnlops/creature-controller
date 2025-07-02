#include "OpusRtpAudioClient.h"

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "util/thread_name.h"

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
{}

OpusRtpAudioClient::~OpusRtpAudioClient()
{
    stop_requested.store(true);
    if (sockDlg_>=0) close(sockDlg_);
    if (sockBgm_>=0) close(sockBgm_);
    if (decDlg_) opus_decoder_destroy(decDlg_);
    if (decBgm_) opus_decoder_destroy(decBgm_);
    if (dev_)    SDL_CloseAudioDevice(dev_);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
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
    if (!dev_) { log_->error("SDL_OpenAudioDevice: {}", SDL_GetError()); return; }
    SDL_PauseAudioDevice(dev_, 1);

    /* Opus decoders */
    int err = 0;
    decDlg_ = opus_decoder_create(SAMPLE_RATE, 1, &err);
    decBgm_ = opus_decoder_create(SAMPLE_RATE, 1, &err);
    if (err) { log_->error("opus decoder create: {}", err); return; }

    /* Sockets */
    if (!openSocket(sockDlg_, dlgGrp_) || !openSocket(sockBgm_, bgmGrp_))
        return;

    std::vector<uint8_t> bufDlg(1500), bufBgm(1500);
    running_.store(true);

    while (!stop_requested.load()) {
        bool gotDlg = recvPkt(sockDlg_, bufDlg);
        bool gotBgm = recvPkt(sockBgm_, bufBgm);

        /* Decode BGM packet immediately; store latest slice */
        if (gotBgm) {
            bool marker = bufBgm[1] & 0x80;              // RTP M bit
            if (marker) {
                opus_decoder_ctl(decBgm_, OPUS_RESET_STATE);
                std::fill(bgmBuf_.begin(), bgmBuf_.end(), 0);
                log_->debug("BGM track restart");
            }
            opus_decode(decBgm_,
                        bufBgm.data()+12, bufBgm.size()-12,
                        bgmBuf_.data(), FRAMES_PER_CHUNK, 0);
            ++pkts_;
        }

        /* When a dialog packet arrives, mix with latest BGM */
        if (gotDlg) {
            bool marker = bufDlg[1] & 0x80;
            if (marker) {                                  // new song
                opus_decoder_ctl(decDlg_, OPUS_RESET_STATE);
                SDL_ClearQueuedAudio(dev_);
                log_->debug("Dialog track restart");
            }

            int16_t dlgPCM[FRAMES_PER_CHUNK];
            int fr = opus_decode(decDlg_,
                                 bufDlg.data()+12, bufDlg.size()-12,
                                 dlgPCM, FRAMES_PER_CHUNK, 0);
            if (fr == FRAMES_PER_CHUNK)
                mixAndQueue(dlgPCM, bgmBuf_.data(), fr);
            ++pkts_;
        }

        bufLvl_ = static_cast<float>(SDL_GetQueuedAudioSize(dev_)) /
                  (SAMPLE_RATE * sizeof(int16_t));

        if (SDL_GetQueuedAudioSize(dev_) >= PREFILL_FRAMES * sizeof(int16_t))
            SDL_PauseAudioDevice(dev_, 0);
    }
    running_.store(false);
}

/* ── socket helpers ───────────────────────────────────────── */
bool OpusRtpAudioClient::openSocket(int& sock, const std::string& group)
{
    sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { log_->error("socket: {}", strerror(errno)); return false; }

    int reuse=1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,  &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif

    sockaddr_in addr{}; addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port_);
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        log_->error("bind: {}", strerror(errno)); return false;
    }

    ip_mreq m{}; m.imr_multiaddr.s_addr = inet_addr(group.c_str());
    m.imr_interface.s_addr = inet_addr(ifaceIp_.c_str());
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof(m))) {
        log_->error("IP_ADD_MEMBERSHIP {}: {}", group, strerror(errno));
        return false;
    }
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
    return true;
}

bool OpusRtpAudioClient::recvPkt(int sock, std::vector<uint8_t>& pkt)
{
    fd_set rf; FD_ZERO(&rf); FD_SET(sock,&rf);
    timeval tv{0, 5'000};
    if (select(sock+1,&rf,nullptr,nullptr,&tv)<=0) return false;

    ssize_t n = ::recv(sock,pkt.data(),pkt.size(),0);
    if (n<=0) return false;
    pkt.resize(static_cast<size_t>(n));
    return true;
}

/* ── mix & queue ─────────────────────────────────────────── */
void OpusRtpAudioClient::mixAndQueue(const int16_t* dlg,
                                     const int16_t* bgm,
                                     int frames)
{
    int16_t mixed[FRAMES_PER_CHUNK];
    for (int i = 0; i < frames; ++i) {
        int32_t m = dlg[i] + (bgm[i] >> 1);   // bgm 50 %
        mixed[i]  = std::clamp(m, -32768, 32767);
    }
    SDL_QueueAudio(dev_, mixed, frames * sizeof(int16_t));
}