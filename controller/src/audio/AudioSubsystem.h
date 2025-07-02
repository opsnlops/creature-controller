#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "audio/audio-config.h"
#include "audio/OpusRtpAudioClient.h"
#include "logging/Logger.h"
#include "util/StoppableThread.h"

namespace creatures::audio {

    class AudioSubsystem : public StoppableThread {
    public:
        explicit AudioSubsystem(std::shared_ptr<creatures::Logger> log);

        /** one-time setup – call before start() */
        bool initialize(uint8_t creatureChannel,                // 1-16
                        const std::string& ifaceIp   = "10.19.63.11",
                        uint8_t audioDeviceIndex     = DEFAULT_SOUND_DEVICE_NUMBER,
                        uint16_t port                = RTP_PORT);

        /* StoppableThread */
        void run() override;
        void shutdown() override;

        [[nodiscard]] bool        isReceiving() const;
        [[nodiscard]] std::string getStats()    const;
        [[nodiscard]] bool        isRunning()   const { return running_.load(); }


    private:
        void monitoringLoop();

        std::shared_ptr<creatures::Logger>  log_;
        std::shared_ptr<OpusRtpAudioClient> rtpClient_;
        std::thread                         monThread_;

        std::atomic<bool> stopMon_{false};
        std::atomic<bool> running_{false};
    };

} // namespace creatures::audio