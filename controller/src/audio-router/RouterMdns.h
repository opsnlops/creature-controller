#pragma once

#include <string>

#include "audio-router/RouterConfig.h"
#include "creature/network/MdnsService.h"
#include "creature/timesync/TimeSync.h"

namespace creatures::audio::router {

// DNS-SD service type for a running audio router
inline constexpr const char *AUDIO_ROUTER_SERVICE_TYPE = "_creature-audio-router._udp";

/**
 * Describe a running audio router for mDNS
 *
 * @param name the router's name, such as the host name or host-instance
 * @param config the configuration the router is running
 * @param routerVersion this router's version
 * @param timeSync the host's PTP sync status
 */
network::MdnsService makeRouterService(const std::string &name, const RouterConfig &config,
                                       const std::string &routerVersion, const timesync::TimeSyncStatus &timeSync = {});

} // namespace creatures::audio::router
