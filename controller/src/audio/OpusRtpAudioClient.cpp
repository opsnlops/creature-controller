//
// OpusRtpAudioClient.cpp - Enhanced with separate stream debugging
//

#include <algorithm>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <fstream>
#include <iostream>

#include "util/thread_name.h"
#include "OpusRtpAudioClient.h"

using namespace creatures::audio;

// AudioDebugger static member definitions
std::ofstream AudioDebugger::dialogAudioFile_;
std::ofstream AudioDebugger::bgmAudioFile_;
std::ofstream AudioDebugger::mixedAudioFile_;
std::ofstream AudioDebugger::dialogRtpFile_;
std::ofstream AudioDebugger::bgmRtpFile_;
std::atomic<bool> AudioDebugger::debugEnabled_{false};

// AudioDebugger method implementations
void AudioDebugger::enableDebugging() {
    debugEnabled_.store(true);

    // Create separate PCM files for each stream
    dialogAudioFile_.open("debug_dialog_audio.pcm", std::ios::binary);
    bgmAudioFile_.open("debug_bgm_audio.pcm", std::ios::binary);
    mixedAudioFile_.open("debug_mixed_audio.pcm", std::ios::binary);

    // Create separate RTP packet files for each stream
    dialogRtpFile_.open("debug_dialog_rtp.bin", std::ios::binary);
    bgmRtpFile_.open("debug_bgm_rtp.bin", std::ios::binary);

    if (dialogAudioFile_.is_open()) {
        std::cout << "🐰 Debug files created:\n";
        std::cout << "  - debug_dialog_audio.pcm (48kHz mono S16LE)\n";
        std::cout << "  - debug_bgm_audio.pcm (48kHz mono S16LE)\n";
        std::cout << "  - debug_mixed_audio.pcm (48kHz mono S16LE)\n";
        std::cout << "  - debug_dialog_rtp.bin\n";
        std::cout << "  - debug_bgm_rtp.bin\n";
        std::cout << "Import into Audacity as: 48000Hz, Mono, 16-bit PCM\n";
    }
}

void AudioDebugger::writeDialogAudio(const int16_t* samples, size_t count) {
    if (debugEnabled_.load() && dialogAudioFile_.is_open()) {
        dialogAudioFile_.write(reinterpret_cast<const char*>(samples), count * sizeof(int16_t));
        dialogAudioFile_.flush();
    }
}

void AudioDebugger::writeBgmAudio(const int16_t* samples, size_t count) {
    if (debugEnabled_.load() && bgmAudioFile_.is_open()) {
        bgmAudioFile_.write(reinterpret_cast<const char*>(samples), count * sizeof(int16_t));
        bgmAudioFile_.flush();
    }
}

void AudioDebugger::writeMixedAudio(const int16_t* samples, size_t count) {
    if (debugEnabled_.load() && mixedAudioFile_.is_open()) {
        mixedAudioFile_.write(reinterpret_cast<const char*>(samples), count * sizeof(int16_t));
        mixedAudioFile_.flush();
    }
}

void AudioDebugger::writeRtpPacket(const std::vector<uint8_t>& packet, const std::string& streamType) {
    if (!debugEnabled_.load()) return;

    std::ofstream* rtpFile = nullptr;
    if (streamType == "dialog" && dialogRtpFile_.is_open()) {
        rtpFile = &dialogRtpFile_;
    } else if (streamType == "bgm" && bgmRtpFile_.is_open()) {
        rtpFile = &bgmRtpFile_;
    }

    if (rtpFile) {
        uint32_t size = packet.size();
        rtpFile->write(reinterpret_cast<const char*>(&size), sizeof(size));
        rtpFile->write(reinterpret_cast<const char*>(packet.data()), packet.size());
        rtpFile->flush();
    }
}

/* ── Enhanced packet structure with RTP header parsing ──── */
struct RtpHeader {
    uint8_t  version;
    bool     padding;
    bool     extension;
    uint8_t  csrcCount;
    bool     marker;
    uint8_t  payloadType;
    uint16_t sequenceNumber;
    uint32_t timestamp;
    uint32_t ssrc;

    static RtpHeader parse(const std::vector<uint8_t>& packet) {
        RtpHeader header{};
        if (packet.size() < 12) return header;

        header.version = (packet[0] >> 6) & 0x03;
        header.padding = (packet[0] >> 5) & 0x01;
        header.extension = (packet[0] >> 4) & 0x01;
        header.csrcCount = packet[0] & 0x0F;
        header.marker = (packet[1] >> 7) & 0x01;
        header.payloadType = packet[1] & 0x7F;
        header.sequenceNumber = (packet[2] << 8) | packet[3];
        header.timestamp = (packet[4] << 24) | (packet[5] << 16) | (packet[6] << 8) | packet[7];
        header.ssrc = (packet[8] << 24) | (packet[9] << 16) | (packet[10] << 8) | packet[11];

        return header;
    }
};

/* ── Enhanced constructor with debugging ──────────────────── */
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

    // Enable debugging for the first 30 seconds
    //AudioDebugger::enableDebugging();

    // Initialize audio frames with proper indexing
    for (size_t i = 0; i < RING_BUFFER_SIZE; ++i) {
        dialogFrames_[i].data.fill(0);
        dialogFrames_[i].ready.store(false);
        dialogFrames_[i].sequenceNumber = 0;
        dialogFrames_[i].timestamp = 0;

        bgmFrames_[i].data.fill(0);
        bgmFrames_[i].ready.store(false);
        bgmFrames_[i].sequenceNumber = 0;
        bgmFrames_[i].timestamp = 0;
    }

    // Initialize sequence tracking
    lastDialogSeq_.store(0);
    lastBgmSeq_.store(0);
    dialogSeqInit_.store(false);
    bgmSeqInit_.store(false);
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

    log_->info("All audio threads started successfully");

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
    uint32_t packetsReceived = 0;
    uint32_t packetsDropped = 0;
    uint32_t decodeErrors = 0;
    uint32_t bufferOverruns = 0;
    uint32_t successfulDecodes = 0;

    while (!stop_requested.load()) {
        if (!recvPacket(sockDlg_, packet)) {
            continue; // No packet ready
        }

        packetsReceived++;

        // Write raw RTP packet for debugging - now separated by stream type
        AudioDebugger::writeRtpPacket(packet, "dialog");

        if (!isValidRtpPacket(packet)) {
            packetsDropped++;
            log_->warn("Invalid dialog RTP packet received (dropped: {})", packetsDropped);
            continue;
        }

        // Parse RTP header for detailed analysis
        RtpHeader rtpHeader = RtpHeader::parse(packet);

        // Debug every single packet for the first 100 packets to see decode flow
        if (packetsReceived <= 100) {
            log_->debug("Dialog packet {}: size={}, payload_size={}, seq={}, attempting decode...",
                       packetsReceived, packet.size(), packet.size() - 12, rtpHeader.sequenceNumber);
        }

        // Enhanced logging every 250 packets to catch issues faster
        if (packetsReceived % 250 == 0) {
            logRingBufferState("Dialog", dialogWriteIdx_.load(), dialogReadIdx_.load(), dialogFrames_);
            log_->debug("Dialog: rx={}, drop={}, decode_ok={}, decode_fail={}, buf_overrun={}, seq={}, ts={}, ssrc={:#x}",
                       packetsReceived, packetsDropped, successfulDecodes, decodeErrors, bufferOverruns,
                       rtpHeader.sequenceNumber, rtpHeader.timestamp, rtpHeader.ssrc);
        }

        // Check for sequence number issues
        if (dialogSeqInit_.load()) {
            uint16_t lastSeq = lastDialogSeq_.load();
            uint16_t expectedSeq = (lastSeq + 1) & 0xFFFF;
            if (rtpHeader.sequenceNumber != expectedSeq) {
                log_->warn("Dialog sequence jump: expected {}, got {} (gap: {})",
                          expectedSeq, rtpHeader.sequenceNumber,
                          static_cast<int16_t>(rtpHeader.sequenceNumber - expectedSeq));
            }
        } else {
            dialogSeqInit_.store(true);
        }
        lastDialogSeq_.store(rtpHeader.sequenceNumber);

        // Check for SSRC changes
        {
            uint32_t currentSSRC = rtpHeader.ssrc;
            std::lock_guard<std::mutex> lock(dialogDecoderMutex_);
            checkAndHandleSSRCChange(currentSSRC, decDlg_, lastDialogSSRC_, "Dialog");

            // Get next write slot with better synchronization
            size_t writeIdx = dialogWriteIdx_.load();
            size_t nextIdx = (writeIdx + 1) % RING_BUFFER_SIZE;

            // Check if we're about to overwrite an unread frame
            AudioFrame& nextFrame = dialogFrames_[nextIdx];
            if (nextFrame.ready.load()) {
                log_->warn("Dialog ring buffer overrun - dropping frame (write={}, read={})",
                          writeIdx, dialogReadIdx_.load());
                bufferOverruns++;
                dialogBufferOverruns_.fetch_add(1);
                packetsDropped++;
                continue;
            }

            // Decode into current frame
            AudioFrame& frame = dialogFrames_[writeIdx];
            frame.sequenceNumber = rtpHeader.sequenceNumber;
            frame.timestamp = rtpHeader.timestamp;

            // Extract Opus payload (skip RTP header)
            const uint8_t* opusPayload = packet.data() + 12;
            int opusPayloadSize = packet.size() - 12;

            log_->trace("Dialog decode: payload_size={}, expected_frames={}",
                       opusPayloadSize, FRAMES_PER_CHUNK);

            int decodedFrames = opus_decode(decDlg_, opusPayload, opusPayloadSize,
                                          frame.data.data(), FRAMES_PER_CHUNK, 0);

            if (decodedFrames == FRAMES_PER_CHUNK) {
                // Success! Write debug audio and advance
                AudioDebugger::writeDialogAudio(frame.data.data(), FRAMES_PER_CHUNK);

                frame.ready.store(true);  // Mark ready LAST
                dialogWriteIdx_.store(nextIdx);
                dialogPkts_.fetch_add(1);
                successfulDecodes++;
                dialogDecodeSuccess_.fetch_add(1);
                dialogFramesProduced_.fetch_add(1);

                log_->trace("Dialog decode SUCCESS: {} samples", decodedFrames);
            } else if (decodedFrames > 0) {
                // Partial frame - fill rest with silence
                std::fill(frame.data.begin() + decodedFrames, frame.data.end(), 0);
                AudioDebugger::writeDialogAudio(frame.data.data(), FRAMES_PER_CHUNK);

                frame.ready.store(true);
                dialogWriteIdx_.store(nextIdx);
                dialogPkts_.fetch_add(1);
                successfulDecodes++;
                dialogDecodeSuccess_.fetch_add(1);
                dialogFramesProduced_.fetch_add(1);

                log_->warn("Dialog PARTIAL decode: {} frames (expected {})", decodedFrames, FRAMES_PER_CHUNK);
            } else {
                decodeErrors++;
                dialogDecodeFailed_.fetch_add(1);
                log_->error("Dialog decode FAILED: {} (payload_size={}, total_errors={})",
                           decodedFrames, opusPayloadSize, decodeErrors);

                // Still produce a frame filled with silence so we don't fall behind
                frame.data.fill(0);
                frame.ready.store(true);
                dialogWriteIdx_.store(nextIdx);
                dialogFramesProduced_.fetch_add(1);
            }
        }
    }

    log_->info("Dialog stream thread stopped - received: {}, dropped: {}, decode_ok: {}, decode_errors: {}, overruns: {}",
               packetsReceived, packetsDropped, successfulDecodes, decodeErrors, bufferOverruns);
}

/* ── BGM stream thread ──────── */
// Replace the BGM thread with this enhanced version that matches the dialog pattern exactly:

void OpusRtpAudioClient::bgmStreamThread()
{
    setThreadName("opus-bgm");
    log_->debug("BGM stream thread started");

    std::vector<uint8_t> packet(1500);
    uint32_t packetsReceived = 0;
    uint32_t packetsDropped = 0;
    uint32_t decodeErrors = 0;
    uint32_t bufferOverruns = 0;
    uint32_t successfulDecodes = 0;

    while (!stop_requested.load()) {
        if (!recvPacket(sockBgm_, packet)) {
            continue; // No packet ready
        }

        packetsReceived++;

        // Write raw RTP packet for debugging - now separated by stream type
        AudioDebugger::writeRtpPacket(packet, "bgm");

        if (!isValidRtpPacket(packet)) {
            packetsDropped++;
            log_->warn("Invalid BGM RTP packet received (dropped: {})", packetsDropped);
            continue;
        }

        // Parse RTP header for detailed analysis
        RtpHeader rtpHeader = RtpHeader::parse(packet);

        // Debug every single packet for the first 100 packets to see decode flow
        if (packetsReceived <= 100) {
            log_->debug("BGM packet {}: size={}, payload_size={}, seq={}, attempting decode...",
                       packetsReceived, packet.size(), packet.size() - 12, rtpHeader.sequenceNumber);
        }

        // Enhanced logging every 250 packets to catch issues faster - SAME AS DIALOG
        if (packetsReceived % 250 == 0) {
            logRingBufferState("BGM", bgmWriteIdx_.load(), bgmReadIdx_.load(), bgmFrames_);
            log_->debug("BGM: rx={}, drop={}, decode_ok={}, decode_fail={}, buf_overrun={}, seq={}, ts={}, ssrc={:#x}",
                       packetsReceived, packetsDropped, successfulDecodes, decodeErrors, bufferOverruns,
                       rtpHeader.sequenceNumber, rtpHeader.timestamp, rtpHeader.ssrc);
        }

        // Check for sequence number issues
        if (bgmSeqInit_.load()) {
            uint16_t lastSeq = lastBgmSeq_.load();
            uint16_t expectedSeq = (lastSeq + 1) & 0xFFFF;
            if (rtpHeader.sequenceNumber != expectedSeq) {
                log_->warn("BGM sequence jump: expected {}, got {} (gap: {})",
                          expectedSeq, rtpHeader.sequenceNumber,
                          (int16_t)(rtpHeader.sequenceNumber - expectedSeq));
            }
        } else {
            bgmSeqInit_.store(true);
        }
        lastBgmSeq_.store(rtpHeader.sequenceNumber);

        // Check for SSRC changes
        {
            const uint32_t currentSSRC = rtpHeader.ssrc;
            std::lock_guard<std::mutex> lock(bgmDecoderMutex_);
            checkAndHandleSSRCChange(currentSSRC, decBgm_, lastBgmSSRC_, "BGM");

            // Get next write slot with better synchronization
            size_t writeIdx = bgmWriteIdx_.load();
            size_t nextIdx = (writeIdx + 1) % RING_BUFFER_SIZE;

            // Check if we're about to overwrite an unread frame
            AudioFrame& nextFrame = bgmFrames_[nextIdx];
            if (nextFrame.ready.load()) {
                log_->warn("BGM ring buffer overrun - dropping frame (write={}, read={})",
                          writeIdx, bgmReadIdx_.load());
                bufferOverruns++;
                bgmBufferOverruns_.fetch_add(1);
                packetsDropped++;
                continue;
            }

            // Decode into current frame
            AudioFrame& frame = bgmFrames_[writeIdx];
            frame.sequenceNumber = rtpHeader.sequenceNumber;
            frame.timestamp = rtpHeader.timestamp;

            // Extract Opus payload (skip RTP header)
            const uint8_t* opusPayload = packet.data() + 12;
            int opusPayloadSize = packet.size() - 12;

            log_->trace("BGM decode: payload_size={}, expected_frames={}",
                       opusPayloadSize, FRAMES_PER_CHUNK);

            int decodedFrames = opus_decode(decBgm_, opusPayload, opusPayloadSize,
                                          frame.data.data(), FRAMES_PER_CHUNK, 0);

            if (decodedFrames == FRAMES_PER_CHUNK) {
                // Success! Write debug audio and advance
                AudioDebugger::writeBgmAudio(frame.data.data(), FRAMES_PER_CHUNK);

                frame.ready.store(true);  // Mark ready LAST
                bgmWriteIdx_.store(nextIdx);
                bgmPkts_.fetch_add(1);
                successfulDecodes++;
                bgmDecodeSuccess_.fetch_add(1);
                bgmFramesProduced_.fetch_add(1);

                log_->trace("BGM decode SUCCESS: {} samples", decodedFrames);
            } else if (decodedFrames > 0) {
                // Partial frame - fill rest with silence
                std::fill(frame.data.begin() + decodedFrames, frame.data.end(), 0);
                AudioDebugger::writeBgmAudio(frame.data.data(), FRAMES_PER_CHUNK);

                frame.ready.store(true);
                bgmWriteIdx_.store(nextIdx);
                bgmPkts_.fetch_add(1);
                successfulDecodes++;
                bgmDecodeSuccess_.fetch_add(1);
                bgmFramesProduced_.fetch_add(1);

                log_->warn("BGM PARTIAL decode: {} frames (expected {})", decodedFrames, FRAMES_PER_CHUNK);
            } else {
                decodeErrors++;
                bgmDecodeFailed_.fetch_add(1);
                log_->error("BGM decode FAILED: {} (payload_size={}, total_errors={})",
                           decodedFrames, opusPayloadSize, decodeErrors);

                // Still produce a frame filled with silence so we don't fall behind
                frame.data.fill(0);
                frame.ready.store(true);
                bgmWriteIdx_.store(nextIdx);
                bgmFramesProduced_.fetch_add(1);
            }
        }
    }

    log_->info("BGM stream thread stopped - received: {}, dropped: {}, decode_ok: {}, decode_errors: {}, overruns: {}",
               packetsReceived, packetsDropped, successfulDecodes, decodeErrors, bufferOverruns);
}

/* ── Audio mixing thread ─────────── */
void OpusRtpAudioClient::audioMixingThread()
{
    setThreadName("opus-mixer");
    log_->debug("Audio mixing thread started");

    bool audioStarted = false;
    std::array<int16_t, FRAMES_PER_CHUNK> mixedBuffer{};

    // More precise timing
    auto frameStart = std::chrono::steady_clock::now();
    constexpr auto frameDuration = std::chrono::microseconds(FRAME_MS * 1000); // 20ms

    uint64_t frameCount = 0;
    uint64_t underruns = 0;
    uint64_t dialogMisses = 0;
    uint64_t bgmMisses = 0;
    uint64_t dialogHits = 0;
    uint64_t bgmHits = 0;

    // Add frame availability tracking
    uint64_t lastDialogFramesSeen = 0;
    uint64_t lastBgmFramesSeen = 0;

    while (!stop_requested.load()) {
        frameCount++;

        // More aggressive timing - don't sleep, just wait precisely
        std::this_thread::sleep_until(frameStart);
        frameStart += frameDuration;

        // Get current read positions
        const size_t dialogReadIdx = dialogReadIdx_.load();
        const size_t bgmReadIdx = bgmReadIdx_.load();

        // Check frame availability
        AudioFrame& dialogFrame = dialogFrames_[dialogReadIdx];
        AudioFrame& bgmFrame = bgmFrames_[bgmReadIdx];

        const bool hasDialog = dialogFrame.ready.load();
        const bool hasBgm = bgmFrame.ready.load();

        // Track hits and misses for debugging
        if (hasDialog) {
            dialogHits++;
        } else {
            dialogMisses++;
        }

        if (hasBgm) {
            bgmHits++;
        } else {
            bgmMisses++;
        }

        // Debug frame production rate every 512 frames
        if (frameCount % 512 == 0) {
            const uint64_t currentDialogFrames = dialogFramesProduced_.load();
            const uint64_t currentBgmFrames = bgmFramesProduced_.load();

            uint64_t dialogFrameRate = currentDialogFrames - lastDialogFramesSeen;
            uint64_t bgmFrameRate = currentBgmFrames - lastBgmFramesSeen;

            log_->debug("Frame production rates: dialog={}/512, bgm={}/512 (expected=512/512 if audio is playing)",
                       dialogFrameRate, bgmFrameRate);
            log_->debug("Decode stats: dialog_ok={}, dialog_fail={}, bgm_ok={}, bgm_fail={}",
                       dialogDecodeSuccess_.load(), dialogDecodeFailed_.load(),
                       bgmDecodeSuccess_.load(), bgmDecodeFailed_.load());

            lastDialogFramesSeen = currentDialogFrames;
            lastBgmFramesSeen = currentBgmFrames;
        }

        // Mix available audio with proper clamping
        for (int i = 0; i < FRAMES_PER_CHUNK; ++i) {
            const int32_t dialogSample = hasDialog ? dialogFrame.data[i] : 0;
            const int32_t bgmSample = hasBgm ? bgmFrame.data[i] : 0;

            // Simple addition mixing - you mentioned 1:1 volume
            int32_t mixed = dialogSample + bgmSample;
            mixedBuffer[i] = std::clamp(mixed, -32768, 32767);
        }

        // Write the mixed audio to debug file
        AudioDebugger::writeMixedAudio(mixedBuffer.data(), FRAMES_PER_CHUNK);

        // Queue the mixed audio
        SDL_QueueAudio(dev_, mixedBuffer.data(), FRAMES_PER_CHUNK * sizeof(int16_t));

        // Check for audio underruns
        uint32_t queuedBytes = SDL_GetQueuedAudioSize(dev_);
        if (audioStarted && queuedBytes < (FRAMES_PER_CHUNK * sizeof(int16_t))) {
            underruns++;
        }

        // Mark frames as consumed and advance read indices
        if (hasDialog) {
            dialogFrame.ready.store(false);
            dialogReadIdx_.store((dialogReadIdx + 1) % RING_BUFFER_SIZE);
        }
        if (hasBgm) {
            bgmFrame.ready.store(false);
            bgmReadIdx_.store((bgmReadIdx + 1) % RING_BUFFER_SIZE);
        }

        // Start audio playback with more conservative buffering
        if (!audioStarted && queuedBytes >= (PREFILL_FRAMES * FRAMES_PER_CHUNK * sizeof(int16_t))) {
            SDL_PauseAudioDevice(dev_, 0);
            audioStarted = true;
            log_->info("Audio playback started with {} bytes buffered", queuedBytes);
        }

        // Enhanced stats every 5 seconds (250 frames at 20ms)
        if (frameCount % 250 == 0) {
            float dialogHitRate = (float)dialogHits / frameCount * 100.0f;
            float bgmHitRate = (float)bgmHits / frameCount * 100.0f;

            log_->info("Mix stats: frames={}, underruns={}, dialog_hits={:.1f}%, bgm_hits={:.1f}%, queued={}bytes",
                      frameCount, underruns, dialogHitRate, bgmHitRate, queuedBytes);
            log_->info("Buffer overruns: dialog={}, bgm={}",
                      dialogBufferOverruns_.load(), bgmBufferOverruns_.load());
        }
    }

    log_->info("Audio mixing thread stopped - mixed {} frames total", frameCount);
}

/* ── Helper methods ──────────────────────────────────────── */
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

bool OpusRtpAudioClient::openSocket(int& sock, const std::string& group) const {
    sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        log_->error("Failed to create socket: {}", strerror(errno));
        return false;
    }

    // Set socket options for better performance
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        log_->warn("Failed to set SO_REUSEADDR: {}", strerror(errno));
    }

#ifdef SO_REUSEPORT
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        log_->warn("Failed to set SO_REUSEPORT: {}", strerror(errno));
    }
#endif

    // Increase receive buffer size to handle bursts
    int rcvBufSize = 256 * 1024; // 256KB
    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvBufSize, sizeof(rcvBufSize)) < 0) {
        log_->warn("Failed to increase receive buffer: {}", strerror(errno));
    }

    // Bind to the port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        log_->error("Failed to bind socket to port {}: {}", port_, strerror(errno));
        close(sock);
        sock = -1;
        return false;
    }

    // Join multicast group
    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(group.c_str());
    mreq.imr_interface.s_addr = inet_addr(ifaceIp_.c_str());

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))) {
        log_->error("Failed to join multicast group {}: {}", group, strerror(errno));
        close(sock);
        sock = -1;
        return false;
    }

    // Set non-blocking mode
    int flags = fcntl(sock, F_GETFL, 0);
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        log_->error("Failed to set non-blocking mode: {}", strerror(errno));
        close(sock);
        sock = -1;
        return false;
    }

    log_->info("Successfully joined multicast group: {} on interface: {}", group, ifaceIp_);
    return true;
}

bool OpusRtpAudioClient::recvPacket(int sock, std::vector<uint8_t>& pkt)
{
    // Use a shorter timeout to reduce latency
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    timeval timeout{0, 1000}; // 1ms timeout
    int selectResult = select(sock + 1, &readfds, nullptr, nullptr, &timeout);

    if (selectResult <= 0) {
        return false; // Timeout or error
    }

    if (!FD_ISSET(sock, &readfds)) {
        return false; // Not ready (shouldn't happen but be safe)
    }

    ssize_t bytesReceived = ::recv(sock, pkt.data(), pkt.size(), 0);
    if (bytesReceived <= 0) {
        return false;
    }

    pkt.resize(static_cast<size_t>(bytesReceived));
    return true;
}