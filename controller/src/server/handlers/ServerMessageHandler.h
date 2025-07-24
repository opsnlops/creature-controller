
#pragma once

#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "logging/Logger.h"
#include "util/Result.h"

namespace creatures ::server {

/**
 * Server Message Handler Interface
 *
 * This is a lot like the `IMessageHandler` interface, but it is specifically
 * for handling messages from the Creature Server. Rather than send in a vector
 * of strings, we send in a JSON object. This fits the model of what the
 * Creature Server will be sending us better.
 *
 */
class ServerMessageHandler {
  public:
    virtual ~ServerMessageHandler() = default;
    virtual Result<std::string> handle(std::shared_ptr<Logger> logger,
                                       json message) = 0;

    // This must be set by the implementing class
    std::string commandType;
};

} // namespace creatures::server