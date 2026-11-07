#include <utility>

#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/strlst.h>

#include "network/AvahiPublisher.h"

namespace creatures::network {

AvahiPublisher::AvahiPublisher(std::shared_ptr<Logger> logger, MdnsService service)
    : logger(std::move(logger)), service(std::move(service)) {
    currentName = this->service.name;
}

AvahiPublisher::~AvahiPublisher() { stop(); }

bool AvahiPublisher::start() {
    poll = avahi_threaded_poll_new();
    if (!poll) {
        logger->warn("mDNS: unable to create the Avahi poll; not advertising {}", currentName);
        return false;
    }

    if (!connect()) {
        avahi_threaded_poll_free(poll);
        poll = nullptr;
        return false;
    }

    if (avahi_threaded_poll_start(poll) < 0) {
        logger->warn("mDNS: unable to start the Avahi poll thread; not advertising {}", currentName);
        stop();
        return false;
    }

    return true;
}

bool AvahiPublisher::connect() {
    // Without NO_FAIL, a missing daemon would be an error. With it, we wait for one.
    int error = 0;
    client = avahi_client_new(avahi_threaded_poll_get(poll), AVAHI_CLIENT_NO_FAIL, clientCallback, this, &error);
    if (!client) {
        logger->warn("mDNS: unable to create an Avahi client: {}", avahi_strerror(error));
        return false;
    }
    return true;
}

void AvahiPublisher::stop() {
    if (poll) {
        avahi_threaded_poll_stop(poll);
    }

    // Freeing the client also frees its entry group, which withdraws the service
    if (client) {
        avahi_client_free(client);
        client = nullptr;
        group = nullptr;
        logger->info("mDNS: stopped advertising {}", currentName);
    }

    if (poll) {
        avahi_threaded_poll_free(poll);
        poll = nullptr;
    }
}

void AvahiPublisher::clientCallback(AvahiClient *client, AvahiClientState state, void *userdata) {
    static_cast<AvahiPublisher *>(userdata)->onClientState(client, state);
}

void AvahiPublisher::groupCallback(AvahiEntryGroup *group, AvahiEntryGroupState state, void *userdata) {
    static_cast<AvahiPublisher *>(userdata)->onGroupState(group, state);
}

void AvahiPublisher::onClientState(AvahiClient *avahiClient, AvahiClientState state) {
    switch (state) {
    case AVAHI_CLIENT_S_RUNNING:
        createService(avahiClient);
        break;

    case AVAHI_CLIENT_S_COLLISION:
    case AVAHI_CLIENT_S_REGISTERING:
        // The host name is changing; withdraw until the daemon settles again
        if (group) {
            avahi_entry_group_reset(group);
        }
        break;

    case AVAHI_CLIENT_CONNECTING:
        logger->info("mDNS: waiting for avahi-daemon before advertising {}", currentName);
        break;

    case AVAHI_CLIENT_FAILURE:
        if (avahi_client_errno(avahiClient) == AVAHI_ERR_DISCONNECTED) {
            // The daemon went away. Start over and wait for it to come back.
            logger->warn("mDNS: lost avahi-daemon; will advertise {} when it returns", currentName);
            avahi_client_free(avahiClient);
            client = nullptr;
            group = nullptr;
            connect();
        } else {
            logger->warn("mDNS: Avahi client failed: {}", avahi_strerror(avahi_client_errno(avahiClient)));
        }
        break;

    default:
        break;
    }
}

void AvahiPublisher::onGroupState(AvahiEntryGroup *entryGroup, AvahiEntryGroupState state) {
    switch (state) {
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
        logger->info("mDNS: advertising {} as {} on port {}", currentName, service.type, service.port);
        break;

    case AVAHI_ENTRY_GROUP_COLLISION:
        chooseAlternativeName();
        avahi_entry_group_reset(entryGroup);
        createService(avahi_entry_group_get_client(entryGroup));
        break;

    case AVAHI_ENTRY_GROUP_FAILURE:
        logger->warn("mDNS: unable to advertise {}: {}", currentName,
                     avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(entryGroup))));
        break;

    default:
        break;
    }
}

void AvahiPublisher::chooseAlternativeName() {
    char *alternative = avahi_alternative_service_name(currentName.c_str());
    logger->warn("mDNS: the name {} is taken; using {}", currentName, alternative);
    currentName = alternative;
    avahi_free(alternative);
}

void AvahiPublisher::createService(AvahiClient *avahiClient) {
    if (!group) {
        group = avahi_entry_group_new(avahiClient, groupCallback, this);
        if (!group) {
            logger->warn("mDNS: unable to create an entry group: {}", avahi_strerror(avahi_client_errno(avahiClient)));
            return;
        }
    }

    if (!avahi_entry_group_is_empty(group)) {
        return;
    }

    AvahiStringList *txt = nullptr;
    for (const auto &[key, value] : service.txt) {
        txt = avahi_string_list_add_pair(txt, key.c_str(), value.c_str());
    }

    int result = avahi_entry_group_add_service_strlst(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                                                      static_cast<AvahiPublishFlags>(0), currentName.c_str(),
                                                      service.type.c_str(), nullptr, nullptr, service.port, txt);
    avahi_string_list_free(txt);

    if (result == AVAHI_ERR_COLLISION) {
        chooseAlternativeName();
        avahi_entry_group_reset(group);
        createService(avahiClient);
        return;
    }

    if (result < 0) {
        logger->warn("mDNS: unable to add the {} service: {}", currentName, avahi_strerror(result));
        return;
    }

    result = avahi_entry_group_commit(group);
    if (result < 0) {
        logger->warn("mDNS: unable to publish {}: {}", currentName, avahi_strerror(result));
    }
}

} // namespace creatures::network
