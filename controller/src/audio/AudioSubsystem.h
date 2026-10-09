//
// AudioSubsystem.h
//

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "audio/AudioEngine.h"
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
     * @param audioConfig Output device and mixer configuration
     * @param port RTP port number
     * @return true if initialization succeeded
     */
    bool initialize(uint8_t creatureChannel, // 1-16
                    const std::string &ifaceIp = "10.19.63.11", const AudioConfig &audioConfig = {},
                    uint16_t port = RTP_PORT);

    void setDialogGainDb(float gainDb);
    void setBgmGainDb(float gainDb);

    /* StoppableThread interface */
    ~AudioSubsystem() override;
    void start() override;
    void run() override;
    void shutdown() override;

    /* Status and monitoring */
    [[nodiscard]] bool isReceiving() const;
    [[nodiscard]] std::string getStats() const;
    [[nodiscard]] bool isRunning() const {
        return audioEngine_ && audioEngine_->state() == AudioEngine::State::Running;
    }

  private:
    void monitoringLoop();

    std::shared_ptr<creatures::Logger> log_;
    std::unique_ptr<AudioEngine> audioEngine_;
    std::thread monThread_;

    uint8_t dialogChannel_{1};
    std::atomic<bool> stopMon_{false};
};

} // namespace creatures::audio
