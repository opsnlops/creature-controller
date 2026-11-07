#pragma once

#include <string>

#include "audio-router/RouterConfig.h"
#include "network/MdnsService.h"

namespace creatures::audio::router {

// DNS-SD service type for a running audio router
inline constexpr const char *AUDIO_ROUTER_SERVICE_TYPE = "_creature-audio-router._udp";

/**
 * Describe a running audio router for mDNS
 *
 * @param name the router's name, such as the host name or host-instance
 * @param config the configuration the router is running
 * @param routerVersion this router's version
 */
network::MdnsService makeRouterService(const std::string &name, const RouterConfig &config,
                                       const std::string &routerVersion);

} // namespace creatures::audio::router
