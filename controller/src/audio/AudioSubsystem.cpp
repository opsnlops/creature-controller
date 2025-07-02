#include "AudioSubsystem.h"
#include <chrono>
#include <thread>
#include <fmt/format.h>
#include "util/thread_name.h"

using namespace creatures::audio;

AudioSubsystem::AudioSubsystem(std::shared_ptr<creatures::Logger> log)
    : log_(log)
{
    log_->debug("AudioSubsystem created");
}

bool AudioSubsystem::initialize(uint8_t creatureChannel,
                                const std::string& ifaceIp,
                                uint8_t audioDev,
                                uint16_t port)
{
    log_->info("Init audio: creature ch {}", creatureChannel);

    rtpClient_ = std::make_shared<OpusRtpAudioClient>(
        log_,
        fmt::format("{}{}", DIALOG_GROUP_BASE, creatureChannel), // dialog
        BGM_GROUP,                                               // bgm
        port,
        creatureChannel,
        ifaceIp);

    return true;
}

void AudioSubsystem::run()
{
    if (!rtpClient_) { log_->error("Audio not initialised"); return; }

    setThreadName("AudioSubsystem");
    rtpClient_->start();

    stopMon_.store(false);
    monThread_ = std::thread(&AudioSubsystem::monitoringLoop, this);

    running_.store(true);
}

void AudioSubsystem::shutdown()
{
    running_.store(false);
    stopMon_.store(true);

    if (monThread_.joinable()) monThread_.join();
    if (rtpClient_) rtpClient_->shutdown();
}

bool AudioSubsystem::isReceiving() const
{
    return rtpClient_ && rtpClient_->isReceiving();
}

std::string AudioSubsystem::getStats() const
{
    if (!rtpClient_) return "audio disabled";

    return fmt::format("pkts={}  buf={:.0f} %",
                       rtpClient_->getPacketsReceived(),
                       rtpClient_->getBufferLevel()*100.0f);
}

void AudioSubsystem::monitoringLoop()
{
    setThreadName("AudioMon");
    using namespace std::chrono_literals;

    while (!stopMon_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(STATS_INTERVAL_SEC));
        if (stopMon_.load()) break;

        log_->info("audio: {}", getStats());
    }
}