
#pragma once

#include <locale>
#include <string>

#include <fmt/format.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "controller-config.h"

#include "logging/Logger.h"
#include "logging/LoggingException.h"

namespace creatures {

/**
 * A spdlog-based implementation of the Logger interface
 */
class SpdlogLogger : public Logger {

  public:
    void init(std::string loggerName) override {
        try {
            // Set up our locale. If this vomits, install `locales-all`
            std::locale::global(std::locale("en_US.UTF-8"));
        } catch (const std::runtime_error &e) {
            throw LoggingException(
                fmt::format("Unable to set the locale: '{}' (Hint: Make sure "
                            "package locales-all is installed!)",
                            std::string(e.what())));
        }

        // Save our name
        this->ourName = std::move(loggerName);

        // Initialize and register the default logger
        ourLogger = spdlog::stdout_color_mt(ourName);
        ourLogger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");

        // Default to debug level logging
        ourLogger->set_level(spdlog::level::debug);
    }

  protected:
    void logTrace(const std::string &format, fmt::format_args args) override {
        ourLogger->trace(fmt::vformat(format, args));
    }

    void logDebug(const std::string &format, fmt::format_args args) override {
        ourLogger->debug(fmt::vformat(format, args));
    }

    void logInfo(const std::string &format, fmt::format_args args) override {
        ourLogger->info(fmt::vformat(format, args));
    }

    void logWarning(const std::string &format, fmt::format_args args) override {
        ourLogger->warn(fmt::vformat(format, args));
    }

    void logError(const std::string &format, fmt::format_args args) override {
        ourLogger->error(fmt::vformat(format, args));
    }

    void logCritical(const std::string &format,
                     fmt::format_args args) override {
        ourLogger->critical(fmt::vformat(format, args));
    }

  private:
    std::shared_ptr<spdlog::logger> ourLogger;
    std::string ourName;
};

} // namespace creatures