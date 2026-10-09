#include "audio-router/RouterConfig.h"
#include "audio/AudioEngine.h"
#include <chrono>
#include <csignal>
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <thread>

namespace {
volatile std::sig_atomic_t stopRequested = 0;
void signalHandler(int) { stopRequested = 1; }

class RouterLogger final : public creatures::Logger {
  public:
    void init(std::string name) override { logger_ = spdlog::stdout_color_mt(std::move(name)); }
    void setLevel(const std::string &name) override { logger_->set_level(spdlog::level::from_str(name)); }

  protected:
    void logTrace(const std::string &format, fmt::format_args args) override {
        log(spdlog::level::trace, format, args);
    }
    void logDebug(const std::string &format, fmt::format_args args) override {
        log(spdlog::level::debug, format, args);
    }
    void logInfo(const std::string &format, fmt::format_args args) override { log(spdlog::level::info, format, args); }
    void logWarning(const std::string &format, fmt::format_args args) override {
        log(spdlog::level::warn, format, args);
    }
    void logError(const std::string &format, fmt::format_args args) override { log(spdlog::level::err, format, args); }
    void logCritical(const std::string &format, fmt::format_args args) override {
        log(spdlog::level::critical, format, args);
    }

  private:
    void log(spdlog::level::level_enum level, const std::string &format, fmt::format_args args) {
        if (logger_->should_log(level))
            logger_->log(level, fmt::vformat(format, args));
    }
    std::shared_ptr<spdlog::logger> logger_;
};
void help() {
    std::cout << "creature-audio-router " << CREATURE_AUDIO_ROUTER_VERSION << "\n"
              << "Usage: creature-audio-router [--config PATH] [--check-config]\n"
              << "  --config PATH         Configuration (default /etc/creature/audio-router.json)\n"
              << "  --check-config        Validate configuration without opening audio or sockets\n"
              << "  --list-sound-devices  List native audio outputs\n"
              << "  --version            Print product version\n"
              << "  --help               Show this help\n";
}
} // namespace
int main(int argc, char **argv) {
    try {
        std::string configPath = "/etc/creature/audio-router.json";
        bool check = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--version") {
                std::cout << CREATURE_AUDIO_ROUTER_VERSION << '\n';
                return 0;
            }
            if (arg == "--help" || arg == "-h") {
                help();
                return 0;
            }
            if (arg == "--list-sound-devices") {
                creatures::audio::listAudioOutputDevices(std::cout);
                return 0;
            }
            if (arg == "--check-config") {
                check = true;
                continue;
            }
            if (arg == "--config" && i + 1 < argc) {
                configPath = argv[++i];
                continue;
            }
            throw std::invalid_argument("Unknown or incomplete argument: " + arg);
        }
        auto config = creatures::audio::router::loadRouterConfig(configPath);
        if (check) {
            std::cout << "Configuration valid: " << configPath << '\n';
            return 0;
        }
        auto logger = std::make_shared<RouterLogger>();
        logger->init("creature-audio-router");
        logger->setLevel(config.logLevel);
        config.engine.interfaceIp = creatures::audio::router::resolveInterfaceAddress(config.networkInterface);
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        creatures::audio::AudioEngine engine(logger, std::move(config.engine));
        if (!engine.start())
            return 1;
        auto nextSummary = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        while (!stopRequested && engine.state() == creatures::audio::AudioEngine::State::Running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (std::chrono::steady_clock::now() >= nextSummary) {
                logger->info("Audio: {}", engine.getStats());
                nextSummary = std::chrono::steady_clock::now() + std::chrono::seconds(60);
            }
        }
        engine.shutdown();
        return engine.state() == creatures::audio::AudioEngine::State::Failed ? 1 : 0;
    } catch (const std::exception &error) {
        std::cerr << "creature-audio-router: " << error.what() << '\n';
        return 1;
    }
}
