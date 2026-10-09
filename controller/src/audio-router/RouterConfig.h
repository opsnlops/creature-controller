#pragma once
#include "audio/audio-config.h"
#include <nlohmann/json_fwd.hpp>
#include <string>

namespace creatures::audio::router {
struct RouterConfig {
    std::string networkInterface;
    std::string logLevel{"info"};
    AudioEngineConfig engine;
};
[[nodiscard]] RouterConfig parseRouterConfig(const nlohmann::json &json);
[[nodiscard]] RouterConfig loadRouterConfig(const std::string &path);
[[nodiscard]] std::string resolveInterfaceAddress(const std::string &interfaceName);
} // namespace creatures::audio::router
