// OpusRtpAudioClient.cpp

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>

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

    // Initialize audio frames
    for (auto& frame : dialogFrames_) {
        frame.data.fill(0);
        frame.ready.store(false);
    }
    for (auto& frame : bgmFrames_) {
        frame.data.fill(0);
        frame.ready.store(false);
    }
}

OpusRtpAudioClient::~OpusRtpAudioClient()
{
    stop_requested.store(true);

    // Join all worker threads
    if (dialogThread_.joinable()) dialogThread_.join();
    if (bgmThread_.joinable()) bgmThread_.join();
    if (mixingThread_.joinable()) mixingThread_.join();

    // Clean up resources
    if (sockDlg_ >= 0) close(sockDlg_);
    if (sockBgm_ >= 0) close(sockBgm_);
    if (decDlg_) opus_decoder_destroy(decDlg_);
    if (decBgm_) opus_decoder_destroy(decBgm_);
    if (dev_) SDL_CloseAudioDevice(dev_);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    log_->debug("OpusRtpAudioClient destroyed, {} SSRC resets occurred", ssrcResets_.load());
}

/* ── main thread entry ───────────────────────────────────── */
void OpusRtpAudioClient::run()
{
    setThreadName("opus-rtp-main");

    /* Initialize SDL Audio */
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

    /* Create Opus decoders */
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

    /* Open sockets */
    if (!openSocket(sockDlg_, dlgGrp_) || !openSocket(sockBgm_, bgmGrp_)) {
        log_->error("Failed to open RTP sockets");
        return;
    }

    log_->info("RTP audio client initialized successfully");
    running_.store(true);

    /* Start worker threads */
    dialogThread_ = std::thread(&OpusRtpAudioClient::dialogStreamThread, this);
    bgmThread_ = std::thread(&OpusRtpAudioClient::bgmStreamThread, this);
    mixingThread_ = std::thread(&OpusRtpAudioClient::audioMixingThread, this);

    log_->info("All audio threads hopping along nicely!");

    /* Main thread just monitors and manages threads */
    while (!stop_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Update buffer level periodically
        bufLvl_ = static_cast<float>(SDL_GetQueuedAudioSize(dev_)) /
                  (SAMPLE_RATE * sizeof(int16_t));

        // Update total packet count
        totalPkts_.store(dialogPkts_.load() + bgmPkts_.load());
    }

    log_->info("RTP audio client stopping");
    running_.store(false);
}

/* ── Dialog stream thread ────── */
void OpusRtpAudioClient::dialogStreamThread()
{
    setThreadName("opus-dialog");
    log_->debug("Dialog stream thread started (channel {})", dialogIdx_);

    std::vector<uint8_t> packet(1500);

    while (!stop_requested.load()) {
        if (!recvPacket(sockDlg_, packet)) {
            continue; // No packet ready, hop to next iteration
        }

        if (!isValidRtpPacket(packet)) {
            continue; // Bad packet, throw it in the compost
        }

        // Check for SSRC changes (new speaker hopped in!)
        uint32_t currentSSRC = extractSSRC(packet);
        {
            std::lock_guard<std::mutex> lock(dialogDecoderMutex_);
            checkAndHandleSSRCChange(currentSSRC, decDlg_, lastDialogSSRC_, "Dialog");

            // Get next write slot
            size_t writeIdx = dialogWriteIdx_.load();
            size_t nextIdx = (writeIdx + 1) % RING_BUFFER_SIZE;

            // Decode directly into the ring buffer
            AudioFrame& frame = dialogFrames_[writeIdx];
            int decodedFrames = opus_decode(decDlg_,
                                          packet.data() + 12, packet.size() - 12,
                                          frame.data.data(), FRAMES_PER_CHUNK, 0);

            if (decodedFrames == FRAMES_PER_CHUNK) {
                frame.ready.store(true);
                dialogWriteIdx_.store(nextIdx);
                dialogPkts_.fetch_add(1);
            } else if (decodedFrames > 0) {
                // Partial frame - fill rest with silence
                std::fill(frame.data.begin() + decodedFrames, frame.data.end(), 0);
                frame.ready.store(true);
                dialogWriteIdx_.store(nextIdx);
                dialogPkts_.fetch_add(1);
            } else {
                log_->warn("Dialog decode failed: {}", decodedFrames);
                // Fill with silence for failed decode
                frame.data.fill(0);
                frame.ready.store(true);
                dialogWriteIdx_.store(nextIdx);
            }
        }
    }

    log_->debug("Dialog stream thread stopped");
}

/* ── BGM stream thread ──────── */
void OpusRtpAudioClient::bgmStreamThread()
{
    setThreadName("opus-bgm");
    log_->debug("BGM stream thread started");

    std::vector<uint8_t> packet(1500);

    while (!stop_requested.load()) {
        if (!recvPacket(sockBgm_, packet)) {
            continue; // No packet ready, keep hopping
        }

        if (!isValidRtpPacket(packet)) {
            continue; // Bad packet, toss it
        }

        // Check for SSRC changes
        uint32_t currentSSRC = extractSSRC(packet);
        {
            std::lock_guard<std::mutex> lock(bgmDecoderMutex_);
            checkAndHandleSSRCChange(currentSSRC, decBgm_, lastBgmSSRC_, "BGM");

            // Get next write slot
            size_t writeIdx = bgmWriteIdx_.load();
            size_t nextIdx = (writeIdx + 1) % RING_BUFFER_SIZE;

            // Decode directly into the ring buffer
            AudioFrame& frame = bgmFrames_[writeIdx];
            int decodedFrames = opus_decode(decBgm_,
                                          packet.data() + 12, packet.size() - 12,
                                          frame.data.data(), FRAMES_PER_CHUNK, 0);

            if (decodedFrames == FRAMES_PER_CHUNK) {
                frame.ready.store(true);
                bgmWriteIdx_.store(nextIdx);
                bgmPkts_.fetch_add(1);
            } else if (decodedFrames > 0) {
                // Partial frame - fill rest with silence
                std::fill(frame.data.begin() + decodedFrames, frame.data.end(), 0);
                frame.ready.store(true);
                bgmWriteIdx_.store(nextIdx);
                bgmPkts_.fetch_add(1);
            } else {
                log_->warn("BGM decode failed: {}", decodedFrames);
                // Fill with silence for failed decode
                frame.data.fill(0);
                frame.ready.store(true);
                bgmWriteIdx_.store(nextIdx);
            }
        }
    }

    log_->debug("BGM stream thread stopped");
}

/* ── Audio mixing thread (the master chef bunny!) ─────────── */
void OpusRtpAudioClient::audioMixingThread()
{
    setThreadName("opus-mixer");
    log_->debug("Audio mixing thread started");

    bool audioStarted = false;
    std::array<int16_t, FRAMES_PER_CHUNK> mixedBuffer{};

    // Timing for consistent frame processing
    auto nextFrameTime = std::chrono::steady_clock::now();
    auto frameDuration = std::chrono::microseconds(FRAME_MS * 1000); // 20ms

    while (!stop_requested.load()) {
        // Wait for the next frame timing
        std::this_thread::sleep_until(nextFrameTime);
        nextFrameTime += frameDuration;

        // Get current read indices
        size_t dialogReadIdx = dialogReadIdx_.load();
        size_t bgmReadIdx = bgmReadIdx_.load();

        // Check if we have frames to mix
        AudioFrame& dialogFrame = dialogFrames_[dialogReadIdx];
        AudioFrame& bgmFrame = bgmFrames_[bgmReadIdx];

        bool hasDialog = dialogFrame.ready.load();
        bool hasBgm = bgmFrame.ready.load();

        // Mix available audio (silence if not ready)
        for (int i = 0; i < FRAMES_PER_CHUNK; ++i) {
            int32_t dialogSample = hasDialog ? dialogFrame.data[i] : 0;
            int32_t bgmSample = hasBgm ? bgmFrame.data[i] : 0;
            int32_t mixed = dialogSample + bgmSample;
            mixedBuffer[i] = std::clamp(mixed, -32768, 32767);
        }

        // Queue the mixed audio
        SDL_QueueAudio(dev_, mixedBuffer.data(), FRAMES_PER_CHUNK * sizeof(int16_t));

        // Mark frames as consumed and advance read indices
        if (hasDialog) {
            dialogFrame.ready.store(false);
            dialogReadIdx_.store((dialogReadIdx + 1) % RING_BUFFER_SIZE);
        }
        if (hasBgm) {
            bgmFrame.ready.store(false);
            bgmReadIdx_.store((bgmReadIdx + 1) % RING_BUFFER_SIZE);
        }

        // Start audio playback once we have enough buffered
        if (!audioStarted && SDL_GetQueuedAudioSize(dev_) >= PREFILL_FRAMES * sizeof(int16_t)) {
            SDL_PauseAudioDevice(dev_, 0);
            audioStarted = true;
            log_->info("Audio playback started (buffer filled to {} frames)", PREFILL_FRAMES);
        }
    }

    log_->debug("Audio mixing thread stopped");
}

/* ── Helper methods (utility bunnies!) ──────────────────── */
bool OpusRtpAudioClient::isValidRtpPacket(const std::vector<uint8_t>& packet)
{
    if (packet.size() < 12) {
        log_->warn("Received RTP packet too small: {} bytes", packet.size());
        return false;
    }

    if (uint8_t version = (packet[0] >> 6) & 0x03; version != 2) {
        log_->warn("Received RTP packet with unsupported version: {}", version);
        return false;
    }

    return true;
}

uint32_t OpusRtpAudioClient::extractSSRC(const std::vector<uint8_t>& packet)
{
    if (packet.size() < 12) {
        return 0;
    }
    return (static_cast<uint32_t>(packet[8]) << 24) |
           (static_cast<uint32_t>(packet[9]) << 16) |
           (static_cast<uint32_t>(packet[10]) << 8) |
           static_cast<uint32_t>(packet[11]);
}

void OpusRtpAudioClient::checkAndHandleSSRCChange(uint32_t newSSRC,
                                                  OpusDecoder* decoder,
                                                  std::atomic<uint32_t>& lastSSRC,
                                                  const std::string& streamName)
{
    bool isDialog = (streamName == "Dialog");
    std::atomic<bool>& initialized = isDialog ? dialogSSRCInitialized_ : bgmSSRCInitialized_;

    if (!initialized.load()) {
        lastSSRC.store(newSSRC);
        initialized.store(true);
        log_->info("{} stream initialized with SSRC: {}", streamName, newSSRC);
        return;
    }

    uint32_t oldSSRC = lastSSRC.load();
    if (newSSRC != oldSSRC) {
        log_->info("{} SSRC changed: {} -> {}, resetting decoder",
                  streamName, oldSSRC, newSSRC);

        opus_decoder_ctl(decoder, OPUS_RESET_STATE);

        if (isDialog) {
            SDL_ClearQueuedAudio(dev_);
            log_->debug("Cleared audio queue due to dialog SSRC change");
        }

        lastSSRC.store(newSSRC);
        ssrcResets_.fetch_add(1);
    }
}

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

bool OpusRtpAudioClient::recvPacket(int sock, std::vector<uint8_t>& pkt)
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    timeval timeout{0, 5000}; // 5ms timeout - enough time for a quick carrot nibble
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