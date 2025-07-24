//
// AudioSubsystem.h
//

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "audio/OpusRtpAudioClient.h"
#include "audio/audio-config.h"
#include "logging/Logger.h"
#include "util/StoppableThread.h"

namespace creatures::audio {

class AudioSubsystem : public StoppableThread {
  public:
    explicit AudioSubsystem(std::shared_ptr<creatures::Logger> log);

    /**
     * Initialize the audio subsystem with creature-specific configuration
     * @param creatureChannel The dialog channel for this creature (1-16)
     * @param ifaceIp The network interface IP address
     * @param audioDeviceIndex SDL audio device index (unused but kept for
     * compatibility)
     * @param port RTP port number
     * @return true if initialization succeeded
     */
    bool initialize(uint8_t creatureChannel, // 1-16
                    const std::string &ifaceIp = "10.19.63.11", uint8_t audioDeviceIndex = DEFAULT_SOUND_DEVICE_NUMBER,
                    uint16_t port = RTP_PORT);

    /* StoppableThread interface */
    void run() override;
    void shutdown() override;

    /* Status and monitoring */
    [[nodiscard]] bool isReceiving() const;
    [[nodiscard]] std::string getStats() const;
    [[nodiscard]] bool isRunning() const { return running_.load(); }

  private:
    void monitoringLoop();

    std::shared_ptr<creatures::Logger> log_;
    std::shared_ptr<OpusRtpAudioClient> rtpClient_;
    std::thread monThread_;

    std::atomic<bool> stopMon_{false};
    std::atomic<bool> running_{false};
};

} // namespace creatures::audio