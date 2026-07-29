#include "audio/AudioOutput.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <alsa/asoundlib.h>

namespace creatures::audio {
namespace {

struct AlsaDevice {
    std::string name;
    std::string description;
};

std::string singleLineDescription(const char *description) {
    if (description == nullptr) {
        return {};
    }

    std::string result(description);
    std::replace(result.begin(), result.end(), '\n', ' ');
    return result;
}

std::vector<AlsaDevice> enumerateDevices() {
    std::vector<AlsaDevice> devices;
    std::unordered_set<std::string> seen;

    devices.push_back({"default", "Default ALSA output"});
    seen.emplace("default");

    void **hints = nullptr;
    if (snd_device_name_hint(-1, "pcm", &hints) < 0 || hints == nullptr) {
        return devices;
    }

    for (void **hint = hints; *hint != nullptr; ++hint) {
        char *name = snd_device_name_get_hint(*hint, "NAME");
        char *description = snd_device_name_get_hint(*hint, "DESC");
        char *ioId = snd_device_name_get_hint(*hint, "IOID");

        const bool isOutput = ioId == nullptr || std::strcmp(ioId, "Output") == 0;
        if (name != nullptr && isOutput && std::strcmp(name, "null") != 0 && seen.emplace(name).second) {
            devices.push_back({name, singleLineDescription(description)});
        }

        std::free(name);
        std::free(description);
        std::free(ioId);
    }

    snd_device_name_free_hint(hints);
    return devices;
}

class AlsaAudioOutput final : public AudioOutput {
  public:
    explicit AlsaAudioOutput(std::shared_ptr<creatures::Logger> log) : log_(std::move(log)) {}
    ~AlsaAudioOutput() override { close(); }

    bool open(const AudioConfig &config) override {
        close();

        const auto devices = enumerateDevices();
        size_t deviceIndex = config.deviceNumber;
        if (deviceIndex >= devices.size()) {
            log_->warn("ALSA output device {} is unavailable; using device 0", deviceIndex);
            deviceIndex = 0;
        }
        const AlsaDevice &device = devices[deviceIndex];

        int result = snd_pcm_open(&pcm_, device.name.c_str(), SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
        if (result < 0) {
            log_->error("Unable to open ALSA output '{}': {}", device.name, snd_strerror(result));
            pcm_ = nullptr;
            return false;
        }

        snd_pcm_hw_params_t *hardwareParameters = nullptr;
        snd_pcm_hw_params_alloca(&hardwareParameters);
        if (!check(snd_pcm_hw_params_any(pcm_, hardwareParameters), "initialize hardware parameters") ||
            !check(snd_pcm_hw_params_set_access(pcm_, hardwareParameters, SND_PCM_ACCESS_RW_INTERLEAVED),
                   "set interleaved access") ||
            !check(snd_pcm_hw_params_set_format(pcm_, hardwareParameters, SND_PCM_FORMAT_S16),
                   "set signed 16-bit format") ||
            !check(snd_pcm_hw_params_set_channels(pcm_, hardwareParameters, OUTPUT_CH), "set mono output")) {
            close();
            return false;
        }

        unsigned int sampleRate = SAMPLE_RATE;
        int direction = 0;
        if (!check(snd_pcm_hw_params_set_rate_near(pcm_, hardwareParameters, &sampleRate, &direction),
                   "set sample rate") ||
            sampleRate != SAMPLE_RATE) {
            if (sampleRate != SAMPLE_RATE) {
                log_->error("ALSA output '{}' cannot provide the required {} Hz sample rate (received {})", device.name,
                            SAMPLE_RATE, sampleRate);
            }
            close();
            return false;
        }

        snd_pcm_uframes_t requestedPeriod = AUDIO_DEVICE_PERIOD_FRAMES;
        direction = 0;
        if (!check(snd_pcm_hw_params_set_period_size_near(pcm_, hardwareParameters, &requestedPeriod, &direction),
                   "set period size")) {
            close();
            return false;
        }

        snd_pcm_uframes_t requestedBuffer = TARGET_PLAYOUT_FRAMES * FRAMES_PER_CHUNK;
        if (!check(snd_pcm_hw_params_set_buffer_size_near(pcm_, hardwareParameters, &requestedBuffer),
                   "set buffer size") ||
            !check(snd_pcm_hw_params(pcm_, hardwareParameters), "apply hardware parameters")) {
            close();
            return false;
        }

        snd_pcm_hw_params_get_period_size(hardwareParameters, &periodFrames_, &direction);
        snd_pcm_hw_params_get_buffer_size(hardwareParameters, &bufferFrames_);

        snd_pcm_sw_params_t *softwareParameters = nullptr;
        snd_pcm_sw_params_alloca(&softwareParameters);
        snd_pcm_uframes_t boundary = 0;
        if (!check(snd_pcm_sw_params_current(pcm_, softwareParameters), "read software parameters") ||
            !check(snd_pcm_sw_params_get_boundary(softwareParameters, &boundary), "read PCM boundary") ||
            !check(snd_pcm_sw_params_set_start_threshold(pcm_, softwareParameters, boundary),
                   "disable automatic start") ||
            !check(snd_pcm_sw_params_set_avail_min(pcm_, softwareParameters, periodFrames_), "set wakeup period") ||
            !check(snd_pcm_sw_params(pcm_, softwareParameters), "apply software parameters") ||
            !check(snd_pcm_prepare(pcm_), "prepare output")) {
            close();
            return false;
        }

        log_->info("ALSA output ready: device {} ('{}'), period={} frames ({:.2f} ms), buffer={} frames ({:.2f} ms)",
                   deviceIndex, device.name, periodFrames_, framesToMilliseconds(periodFrames_), bufferFrames_,
                   framesToMilliseconds(bufferFrames_));
        return true;
    }

    bool start() override {
        if (pcm_ == nullptr) {
            return false;
        }

        const int result = snd_pcm_start(pcm_);
        if (result < 0) {
            log_->error("Unable to start ALSA output: {}", snd_strerror(result));
            return false;
        }
        started_ = true;
        return true;
    }

    void stopAndClear() override {
        if (pcm_ == nullptr) {
            return;
        }

        started_ = false;
        snd_pcm_drop(pcm_);
        const int result = snd_pcm_prepare(pcm_);
        if (result < 0) {
            log_->warn("Unable to prepare ALSA output after clearing it: {}", snd_strerror(result));
        }
    }

    void close() override {
        if (pcm_ == nullptr) {
            return;
        }

        started_ = false;
        snd_pcm_drop(pcm_);
        snd_pcm_close(pcm_);
        pcm_ = nullptr;
    }

    bool write(std::span<const int16_t> samples) override {
        if (pcm_ == nullptr) {
            return false;
        }

        size_t offset = 0;
        int waitsRemaining = 10;
        bool recovered = false;
        while (offset < samples.size()) {
            const snd_pcm_sframes_t written =
                snd_pcm_writei(pcm_, samples.data() + offset, static_cast<snd_pcm_uframes_t>(samples.size() - offset));
            if (written > 0) {
                offset += static_cast<size_t>(written);
                continue;
            }
            if (written == -EAGAIN && waitsRemaining-- > 0) {
                snd_pcm_wait(pcm_, PACKET_WAIT_MS);
                continue;
            }
            if (written == -EPIPE || written == -ESTRPIPE) {
                underruns_.fetch_add(1);
                const int recoverResult = snd_pcm_recover(pcm_, static_cast<int>(written), 1);
                if (recoverResult < 0) {
                    log_->error("Unable to recover ALSA output: {}", snd_strerror(recoverResult));
                    return false;
                }
                recovered = true;
                continue;
            }

            log_->error("Unable to write ALSA audio: {}", snd_strerror(static_cast<int>(written)));
            return false;
        }

        if (recovered && started_) {
            const int result = snd_pcm_start(pcm_);
            if (result < 0) {
                log_->error("Unable to restart ALSA output after underrun: {}", snd_strerror(result));
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] size_t queuedFrames() const override {
        if (pcm_ == nullptr) {
            return 0;
        }

        snd_pcm_sframes_t delay = 0;
        if (snd_pcm_delay(pcm_, &delay) < 0 || delay <= 0) {
            return 0;
        }
        return static_cast<size_t>(delay);
    }

    [[nodiscard]] size_t pipelineLatencyFrames() const override { return 0; }

    [[nodiscard]] uint64_t underruns() const override { return underruns_.load(); }

  private:
    bool check(int result, const char *operation) const {
        if (result >= 0) {
            return true;
        }
        log_->error("Unable to {} for ALSA output: {}", operation, snd_strerror(result));
        return false;
    }

    static double framesToMilliseconds(snd_pcm_uframes_t frames) {
        return static_cast<double>(frames) * 1000.0 / SAMPLE_RATE;
    }

    std::shared_ptr<creatures::Logger> log_;
    snd_pcm_t *pcm_{nullptr};
    snd_pcm_uframes_t periodFrames_{0};
    snd_pcm_uframes_t bufferFrames_{0};
    bool started_{false};
    std::atomic<uint64_t> underruns_{0};
};

} // namespace

std::unique_ptr<AudioOutput> createAudioOutput(std::shared_ptr<creatures::Logger> log) {
    return std::make_unique<AlsaAudioOutput>(std::move(log));
}

void listAudioOutputDevices(std::ostream &output) {
    const auto devices = enumerateDevices();
    output << "Available ALSA audio devices for RTP playback:\n";
    output << "Number of audio devices: " << devices.size() << '\n';
    for (size_t index = 0; index < devices.size(); ++index) {
        output << "  Device " << index << ": " << devices[index].name;
        if (!devices[index].description.empty()) {
            output << " — " << devices[index].description;
        }
        output << '\n';
    }
}

} // namespace creatures::audio
