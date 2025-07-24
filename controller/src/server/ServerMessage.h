
#pragma once

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "logging/Logger.h"
#include "util/Result.h"

namespace creatures ::server {

class ServerMessage {
  public:
    virtual ~ServerMessage() = default;

    // Turn this message into a string that can be sent over the web socket
    Result<std::string> toWebSocketMessage(std::string creatureId);

  protected:
    std::string commandType;
    json message;
    std::shared_ptr<Logger> logger;
};

} // namespace creatures::server