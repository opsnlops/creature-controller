
#include <fmt/format.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "logging/Logger.h"
#include "server/ServerMessage.h"
#include "util/Result.h"

namespace creatures ::server {

Result<std::string> ServerMessage::toWebSocketMessage(std::string creatureId) {

    logger->debug("creating JSON message to send down the websocket");

    json j;
    j["command"] = commandType;
    j["payload"] = message;

    // Ensure that the creature Id is set, always.
    j["payload"]["creature_id"] = creatureId;

    try {
        return Result<std::string>{j.dump()};
    } catch (std::exception &e) {
        auto errorMessage =
            fmt::format("Error creating JSON message: {}", e.what());
        logger->error(errorMessage);
        return Result<std::string>{ControllerError(
            ControllerError::UnprocessableMessage, errorMessage)};
    }
}

} // namespace creatures::server
