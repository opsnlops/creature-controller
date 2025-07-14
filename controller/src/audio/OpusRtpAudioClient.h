// OpusRtpAudioClient.h


#pragma once
#include <atomic>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <mutex>

#include <opus.h>
#include <SDL.h>

#include "audio/audio-config.h"
#include "logging/Logger.h"
#include "util/StoppableThread.h"

namespace creatures::audio {

    class OpusRtpAudioClient final : public StoppableThread {
    public:
        OpusRtpAudioClient(std::shared_ptr<creatures::Logger> log,
                           std::string dialogGroup,   // 239.19.63.<1-16>
                           std::string bgmGroup,      // 239.19.63.17
                           uint16_t    port,          // 5004
                           uint8_t     dialogIdx,     // 1-16
                           std::string ifaceIp);

        ~OpusRtpAudioClient() override;

        /* stats for AudioSubsystem */
        [[nodiscard]] bool     isReceiving()        const { return running_.load(); }
        [[nodiscard]] uint64_t getPacketsReceived() const { return totalPkts_.load(); }
        [[nodiscard]] float    getBufferLevel()     const { return bufLvl_.load(); }

    private:
        void run() override;

        /* Stream handling threads */
        void dialogStreamThread();
        void bgmStreamThread();
        void audioMixingThread();

        /* helpers */
        bool openSocket(int& sock, const std::string& group);
        static bool recvPacket(int sock, std::vector<uint8_t>& pkt);

        /* RTP packet parsing */
        uint32_t extractSSRC(const std::vector<uint8_t>& packet) ;
        bool isValidRtpPacket(const std::vector<uint8_t>& packet) ;

        /* SSRC change detection and decoder reset */
        void checkAndHandleSSRCChange(uint32_t newSSRC, OpusDecoder* decoder,
                                     std::atomic<uint32_t>& lastSSRC, const std::string& streamName);

        /* immutable after ctor */
        std::shared_ptr<creatures::Logger> log_;
        const std::string dlgGrp_, bgmGrp_, ifaceIp_;
        const uint16_t    port_;
        const uint8_t     dialogIdx_;

        /* runtime */
        int          sockDlg_{-1}, sockBgm_{-1};
        OpusDecoder* decDlg_{nullptr};
        OpusDecoder* decBgm_{nullptr};
        SDL_AudioDeviceID dev_{0};

        /* Audio buffers with thread synchronization */
        struct AudioFrame {
            std::array<int16_t, FRAMES_PER_CHUNK> data{};
            std::atomic<bool> ready{false};
        };

        // Ring buffers for each stream (small buffer since we mix immediately)
        static constexpr size_t RING_BUFFER_SIZE = 8;
        std::array<AudioFrame, RING_BUFFER_SIZE> dialogFrames_;
        std::array<AudioFrame, RING_BUFFER_SIZE> bgmFrames_;

        std::atomic<size_t> dialogWriteIdx_{0};
        std::atomic<size_t> dialogReadIdx_{0};
        std::atomic<size_t> bgmWriteIdx_{0};
        std::atomic<size_t> bgmReadIdx_{0};

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
        std::atomic<bool>     running_{false};
        std::atomic<uint64_t> totalPkts_{0};
        std::atomic<uint64_t> dialogPkts_{0};
        std::atomic<uint64_t> bgmPkts_{0};
        std::atomic<float>    bufLvl_{0.0f};
        std::atomic<uint64_t> ssrcResets_{0};
    };

} // namespace creatures::audio