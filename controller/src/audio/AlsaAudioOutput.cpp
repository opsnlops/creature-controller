#include "audio/AudioOutput.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <sstream>
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

std::string trim(std::string value) {
    const auto isNotSpace = [](unsigned char character) { return !std::isspace(character); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), isNotSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), isNotSpace).base(), value.end());
    return value;
}

bool isRelevantPcmDetail(const std::string &line) {
    return line.find("PCM") != std::string::npos || line.starts_with("access") || line.starts_with("format") ||
           line.starts_with("subformat") || line.starts_with("channels") || line.starts_with("rate") ||
           line.starts_with("exact rate");
}

class AlsaAudioOutput final : public AudioOutput {
  public:
    explicit AlsaAudioOutput(std::shared_ptr<creatures::Logger> log) : log_(std::move(log)) {}
    ~AlsaAudioOutput() override { close(); }

    bool open(const AudioConfig &config) override {
        close();
        channels_ = config.channels.size();
        if (channels_ == 0 || channels_ > 8)
            return false;

        std::string deviceName;
        std::optional<size_t> deviceIndex;
        if (config.deviceName.has_value()) {
            deviceName = *config.deviceName;
        } else {
            const auto devices = enumerateDevices();
            deviceIndex = config.deviceNumber;
            if (*deviceIndex >= devices.size()) {
                log_->warn("ALSA output device {} is unavailable; using device 0", *deviceIndex);
                deviceIndex = 0;
            }
            deviceName = devices[*deviceIndex].name;
        }

        int result = snd_pcm_open(&pcm_, deviceName.c_str(), SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
        if (result < 0) {
            // Linux drivers can return ENOTSUPP (524), which libc/ALSA do not describe.
            const char *reason =
                result == -524 ? "Playback is not supported by the selected device" : snd_strerror(result);
            log_->error("Unable to open configured ALSA output '{}' (error {}): {}", deviceName, result, reason);
            if (deviceName == "default") {
                log_->error("ALSA 'default' may select a different card than your DAC. Audio device 0 is the default "
                            "entry, not hardware card 0; select the intended output by name.");
            }
            if (result == -524) {
                log_->error("On Raspberry Pi, an HDMI output without a connected audio-capable display or AVR can "
                            "return this error. For an I2S or USB DAC, select that DAC's named ALSA output.");
            }
            log_->error("Run --list-sound-devices to find output names. Set audioDeviceName in the controller config "
                        "or output.deviceName in the audio router config (for example, plughw:CARD=<card>,DEV=0).");
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
            !check(snd_pcm_hw_params_set_channels(pcm_, hardwareParameters, static_cast<unsigned int>(channels_)),
                   "set output channel count")) {
            close();
            return false;
        }

        unsigned int sampleRate = SAMPLE_RATE;
        int direction = 0;
        if (!check(snd_pcm_hw_params_set_rate_near(pcm_, hardwareParameters, &sampleRate, &direction),
                   "set sample rate") ||
            sampleRate != SAMPLE_RATE) {
            if (sampleRate != SAMPLE_RATE) {
                log_->error("ALSA output '{}' cannot provide the required {} Hz sample rate (received {})", deviceName,
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

        snd_pcm_uframes_t requestedBuffer =
            std::max<size_t>(TARGET_PLAYOUT_FRAMES * FRAMES_PER_CHUNK,
                             static_cast<size_t>(config.commonPlayoutDelayMs) * SAMPLE_RATE / 1000 + FRAMES_PER_CHUNK);
        if (!check(snd_pcm_hw_params_set_buffer_size_near(pcm_, hardwareParameters, &requestedBuffer),
                   "set buffer size") ||
            !check(snd_pcm_hw_params(pcm_, hardwareParameters), "apply hardware parameters")) {
            close();
            return false;
        }

        if (channels_ > 1 && !configureChannelMap(config.channels)) {
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

        if (deviceIndex.has_value()) {
            log_->info(
                "ALSA output ready: device {} ('{}'), period={} frames ({:.2f} ms), buffer={} frames ({:.2f} ms)",
                *deviceIndex, deviceName, periodFrames_, framesToMilliseconds(periodFrames_), bufferFrames_,
                framesToMilliseconds(bufferFrames_));
        } else {
            log_->info(
                "ALSA output ready: named device '{}', period={} frames ({:.2f} ms), buffer={} frames ({:.2f} ms)",
                deviceName, periodFrames_, framesToMilliseconds(periodFrames_), bufferFrames_,
                framesToMilliseconds(bufferFrames_));
        }
        logPcmPath();
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
        if (pcm_ == nullptr || samples.size() % channels_ != 0) {
            return false;
        }

        size_t offset = 0;
        int waitsRemaining = 10;
        bool recovered = false;
        int recoveriesRemaining = 3;
        while (offset < samples.size()) {
            const snd_pcm_sframes_t written = snd_pcm_writei(
                pcm_, samples.data() + offset, static_cast<snd_pcm_uframes_t>((samples.size() - offset) / channels_));
            if (written > 0) {
                offset += static_cast<size_t>(written) * channels_;
                continue;
            }
            if (written == -EAGAIN && waitsRemaining-- > 0) {
                snd_pcm_wait(pcm_, PACKET_WAIT_MS);
                continue;
            }
            if ((written == -EPIPE || written == -ESTRPIPE) && recoveriesRemaining-- > 0) {
                underruns_.fetch_add(1);
                // Preparing drops a failed/suspended queue without an unbounded resume wait.
                const int recoverResult = snd_pcm_prepare(pcm_);
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
    bool configureChannelMap(const std::vector<SpeakerChannel> &channels) {
        std::vector<unsigned int> storage(1 + channels.size());
        auto *requested = reinterpret_cast<snd_pcm_chmap_t *>(storage.data());
        requested->channels = static_cast<unsigned int>(channels.size());
        for (size_t i = 0; i < channels.size(); ++i) {
            switch (channels[i]) {
            case SpeakerChannel::FrontLeft:
                requested->pos[i] = SND_CHMAP_FL;
                break;
            case SpeakerChannel::FrontRight:
                requested->pos[i] = SND_CHMAP_FR;
                break;
            case SpeakerChannel::Center:
                requested->pos[i] = SND_CHMAP_FC;
                break;
            case SpeakerChannel::Lfe:
                requested->pos[i] = SND_CHMAP_LFE;
                break;
            case SpeakerChannel::RearLeft:
                requested->pos[i] = SND_CHMAP_RL;
                break;
            case SpeakerChannel::RearRight:
                requested->pos[i] = SND_CHMAP_RR;
                break;
            case SpeakerChannel::SideLeft:
                requested->pos[i] = SND_CHMAP_SL;
                break;
            case SpeakerChannel::SideRight:
                requested->pos[i] = SND_CHMAP_SR;
                break;
            case SpeakerChannel::Mono:
                requested->pos[i] = SND_CHMAP_MONO;
                break;
            }
        }
        // Fixed maps may reject setting even when the existing map already matches.
        (void)snd_pcm_set_chmap(pcm_, requested);
        auto *actual = snd_pcm_get_chmap(pcm_);
        const bool matches = actual != nullptr && actual->channels == requested->channels &&
                             std::equal(requested->pos, requested->pos + requested->channels, actual->pos);
        std::free(actual);
        if (!matches)
            log_->error("ALSA cannot verify the requested speaker map; refusing ambiguous routing");
        return matches;
    }

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

    void logPcmPath() const {
        snd_output_t *output = nullptr;
        if (snd_output_buffer_open(&output) < 0 || output == nullptr) {
            log_->debug("Unable to create ALSA PCM diagnostic output");
            return;
        }

        const int dumpResult = snd_pcm_dump(pcm_, output);
        char *buffer = nullptr;
        const size_t bufferSize = snd_output_buffer_string(output, &buffer);
        const std::string dump =
            dumpResult >= 0 && buffer != nullptr && bufferSize > 0 ? std::string(buffer, bufferSize) : std::string{};
        snd_output_close(output);

        if (dump.empty()) {
            log_->debug("ALSA did not provide PCM path details");
            return;
        }

        std::istringstream lines(dump);
        std::string line;
        std::vector<std::string> details;
        while (std::getline(lines, line)) {
            line = trim(std::move(line));
            if (!line.empty() && isRelevantPcmDetail(line)) {
                details.push_back(std::move(line));
            }
        }

        if (details.empty()) {
            return;
        }

        log_->info("ALSA PCM path (different rates indicate resampling):");
        for (const std::string &detail : details) {
            log_->info("  {}", detail);
        }
    }

    std::shared_ptr<creatures::Logger> log_;
    snd_pcm_t *pcm_{nullptr};
    snd_pcm_uframes_t periodFrames_{0};
    snd_pcm_uframes_t bufferFrames_{0};
    size_t channels_{1};
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
