//
// OpusRtpAudioClient.h
//

#pragma once
#include <atomic>
#include <array>
#include <memory>
#include <string>
#include <vector>

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
        [[nodiscard]] uint64_t getPacketsReceived() const { return pkts_.load();    }
        [[nodiscard]] float    getBufferLevel()     const { return bufLvl_.load();  }

    private:
        void run() override;

        /* helpers */
        bool openSocket(int& sock, const std::string& group);
        bool recvPkt(int sock, std::vector<uint8_t>& pkt);
        void mixAndQueue(const int16_t* dlg, const int16_t* bgm, int frames);

        /* RTP packet parsing */
        uint32_t extractSSRC(const std::vector<uint8_t>& packet) const;
        bool isValidRtpPacket(const std::vector<uint8_t>& packet) const;

        /* SSRC change detection and decoder reset */
        void checkAndHandleSSRCChange(uint32_t newSSRC, OpusDecoder* decoder,
                                     uint32_t& lastSSRC, const std::string& streamName);

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

        std::array<int16_t, FRAMES_PER_CHUNK> bgmBuf_{};   // latest bgm slice

        /* SSRC tracking for decoder reset detection */
        uint32_t lastDialogSSRC_{0};
        uint32_t lastBgmSSRC_{0};
        bool dialogSSRCInitialized_{false};
        bool bgmSSRCInitialized_{false};

        std::atomic<bool>     running_{false};
        std::atomic<uint64_t> pkts_{0};
        std::atomic<float>    bufLvl_{0.0f};
        std::atomic<uint64_t> ssrcResets_{0};  // Track the number of decoder resets
    };

} // namespace creatures::audio