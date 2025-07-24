//
// OpusRtpAudioClient.h - Enhanced with separate stream debugging
//

#pragma once
#include <array>
#include <atomic>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <SDL.h>
#include <opus.h>

#include "audio/audio-config.h"
#include "logging/Logger.h"
#include "util/StoppableThread.h"

namespace creatures::audio {

// AudioDebugger class for debugging RTP and audio data with separate files
class AudioDebugger {
  public:
    static void enableDebugging();
    static void writeDialogAudio(const int16_t *samples, size_t count);
    static void writeBgmAudio(const int16_t *samples, size_t count);
    static void writeMixedAudio(const int16_t *samples, size_t count);
    static void writeRtpPacket(const std::vector<uint8_t> &packet, const std::string &streamType);

  private:
    static std::ofstream dialogAudioFile_;
    static std::ofstream bgmAudioFile_;
    static std::ofstream mixedAudioFile_;
    static std::ofstream dialogRtpFile_;
    static std::ofstream bgmRtpFile_;
    static std::atomic<bool> debugEnabled_;
};

class OpusRtpAudioClient final : public StoppableThread {
  public:
    OpusRtpAudioClient(std::shared_ptr<creatures::Logger> log,
                       std::string dialogGroup, // 239.19.63.<1-16>
                       std::string bgmGroup,    // 239.19.63.17
                       uint16_t port,           // 5004
                       uint8_t dialogIdx,       // 1-16
                       std::string ifaceIp);

    ~OpusRtpAudioClient() override;

    /* stats for AudioSubsystem */
    [[nodiscard]] bool isReceiving() const { return running_.load(); }
    [[nodiscard]] uint64_t getPacketsReceived() const { return totalPkts_.load(); }
    [[nodiscard]] float getBufferLevel() const { return bufLvl_.load(); }

  private:
    void run() override;

    /* Stream handling threads */
    void dialogStreamThread();
    void bgmStreamThread();
    void audioMixingThread();

    /* helpers */
    bool openSocket(int &sock, const std::string &group) const;
    static bool recvPacket(int sock, std::vector<uint8_t> &pkt);

    /* RTP packet parsing */
    static uint32_t extractSSRC(const std::vector<uint8_t> &packet);
    bool isValidRtpPacket(const std::vector<uint8_t> &packet);

    /* SSRC change detection and decoder reset */
    void checkAndHandleSSRCChange(uint32_t newSSRC, OpusDecoder *decoder, std::atomic<uint32_t> &lastSSRC,
                                  const std::string &streamName);

    /* immutable after ctor */
    std::shared_ptr<creatures::Logger> log_;
    const std::string dlgGrp_, bgmGrp_, ifaceIp_;
    const uint16_t port_;
    const uint8_t dialogIdx_;

    /* runtime */
    int sockDlg_{-1}, sockBgm_{-1};
    OpusDecoder *decDlg_{nullptr};
    OpusDecoder *decBgm_{nullptr};
    SDL_AudioDeviceID dev_{0};

    /* Enhanced audio frame structure with RTP metadata */
    struct AudioFrame {
        std::array<int16_t, FRAMES_PER_CHUNK> data{};
        std::atomic<bool> ready{false};
        uint16_t sequenceNumber{0}; // RTP sequence number
        uint32_t timestamp{0};      // RTP timestamp
    };

    // Ring buffers for each stream
    static constexpr size_t RING_BUFFER_SIZE = 16; // Increased for debugging
    std::array<AudioFrame, RING_BUFFER_SIZE> dialogFrames_;
    std::array<AudioFrame, RING_BUFFER_SIZE> bgmFrames_;

    std::atomic<size_t> dialogWriteIdx_{0};
    std::atomic<size_t> dialogReadIdx_{0};
    std::atomic<size_t> bgmWriteIdx_{0};
    std::atomic<size_t> bgmReadIdx_{0};

    /* Enhanced sequence number tracking */
    std::atomic<uint16_t> lastDialogSeq_{0};
    std::atomic<uint16_t> lastBgmSeq_{0};
    std::atomic<bool> dialogSeqInit_{false};
    std::atomic<bool> bgmSeqInit_{false};

    /* Worker threads */
    std::thread dialogThread_;
    std::thread bgmThread_;
    std::thread mixingThread_;

    /* SSRC tracking for decoder reset detection */
    std::atomic<uint32_t> lastDialogSSRC_{0};
    std::atomic<uint32_t> lastBgmSSRC_{0};
    std::atomic<bool> dialogSSRCInitialized_{false};
    std::atomic<bool> bgmSSRCInitialized_{false};

    /* Thread-safe decoder access */
    std::mutex dialogDecoderMutex_;
    std::mutex bgmDecoderMutex_;

    /* Statistics */
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> totalPkts_{0};
    std::atomic<uint64_t> dialogPkts_{0};
    std::atomic<uint64_t> bgmPkts_{0};
    std::atomic<float> bufLvl_{0.0f};
    std::atomic<uint64_t> ssrcResets_{0};

    /* Enhanced debugging counters */
    std::atomic<uint64_t> dialogDecodeSuccess_{0};
    std::atomic<uint64_t> dialogDecodeFailed_{0};
    std::atomic<uint64_t> dialogFramesProduced_{0};
    std::atomic<uint64_t> dialogBufferOverruns_{0};

    std::atomic<uint64_t> bgmDecodeSuccess_{0};
    std::atomic<uint64_t> bgmDecodeFailed_{0};
    std::atomic<uint64_t> bgmFramesProduced_{0};
    std::atomic<uint64_t> bgmBufferOverruns_{0};

    // Add this method to help debug ring buffer state
    void logRingBufferState(const std::string &streamName, size_t writeIdx, size_t readIdx,
                            const std::array<AudioFrame, RING_BUFFER_SIZE> &frames) {
        size_t availableFrames = 0;
        for (const auto &frame : frames) {
            if (frame.ready.load())
                availableFrames++;
        }

        log_->debug("{} ring buffer: write={}, read={}, available={}/{}", streamName, writeIdx, readIdx,
                    availableFrames, RING_BUFFER_SIZE);
    }
};

} // namespace creatures::audio