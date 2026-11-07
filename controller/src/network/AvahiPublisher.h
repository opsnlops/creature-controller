#pragma once

#include <memory>
#include <string>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/thread-watch.h>

#include "logging/Logger.h"
#include "network/MdnsRecords.h"

namespace creatures::network {

/**
 * Advertises a service over mDNS through avahi-daemon for as long as it runs
 *
 * Advertising is a convenience, so nothing here stops the controller. If the daemon
 * isn't running, the publisher waits for it, and it re-registers if the daemon
 * restarts or another device already uses the name.
 */
class AvahiPublisher {
  public:
    AvahiPublisher(std::shared_ptr<Logger> logger, MdnsService service);
    ~AvahiPublisher();

    AvahiPublisher(const AvahiPublisher &) = delete;
    AvahiPublisher &operator=(const AvahiPublisher &) = delete;

    /**
     * Start advertising
     *
     * @return false if the publisher couldn't be set up at all
     */
    bool start();

    /**
     * Stop advertising and release everything
     */
    void stop();

  private:
    static void clientCallback(AvahiClient *client, AvahiClientState state, void *userdata);
    static void groupCallback(AvahiEntryGroup *group, AvahiEntryGroupState state, void *userdata);

    // These run on Avahi's poll thread, apart from the first client callback
    void onClientState(AvahiClient *client, AvahiClientState state);
    void onGroupState(AvahiEntryGroup *group, AvahiEntryGroupState state);
    void createService(AvahiClient *client);
    void chooseAlternativeName();
    bool connect();

    std::shared_ptr<Logger> logger;
    MdnsService service;
    std::string currentName;

    AvahiThreadedPoll *poll = nullptr;
    AvahiClient *client = nullptr;
    AvahiEntryGroup *group = nullptr;
};

} // namespace creatures::network
