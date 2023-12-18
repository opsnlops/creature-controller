
#pragma once

#include <locale>
#include <string>

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "logging/Logger.h"
#include "logging/LoggingException.h"

namespace creatures {

    /**
     * A spdlog-based implementation of the Logger interface
     */
    class SpdlogLogger : public Logger {

    public:
        void init() override {
            try {
                // Set up our locale. If this vomits, install `locales-all`
                std::locale::global(std::locale("en_US.UTF-8"));
            }
            catch (const std::runtime_error &e) {
                throw LoggingException(fmt::format("Unable to set the locale: '{}' (Hint: Make sure package locales-all is installed!)",
                                                   std::string(e.what())));
            }

            // Initialize and register the default logger
            auto console = spdlog::stdout_color_mt("controller");
            ::spdlog::set_default_logger(console);

            // Default to trace-level logging
            ::spdlog::set_level(spdlog::level::trace);
        }

    protected:
        void logTrace(const std::string &format, fmt::format_args args) override {
            ::spdlog::trace(fmt::vformat(format, args));
        }

        void logDebug(const std::string &format, fmt::format_args args) override {
            ::spdlog::debug(fmt::vformat(format, args));
        }

        void logInfo(const std::string &format, fmt::format_args args) override {
            ::spdlog::info(fmt::vformat(format, args));
        }

        void logWarning(const std::string &format, fmt::format_args args) override {
            ::spdlog::warn(fmt::vformat(format, args));
        }

        void logError(const std::string &format, fmt::format_args args) override {
            ::spdlog::error(fmt::vformat(format, args));
        }

        void logCritical(const std::string &format, fmt::format_args args) override {
            ::spdlog::critical(fmt::vformat(format, args));
        }

    };

}