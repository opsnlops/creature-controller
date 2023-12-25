
#include <chrono>
#include <string>
#include <vector>

#include "controller-config.h"

#include "logging/Logger.h"
#include "io/handlers/PongHandler.h"

extern std::chrono::time_point<std::chrono::high_resolution_clock> lastPingSentAt;

namespace creatures {

    void PongHandler::handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) {

        // When did we receive this?
        auto pongTime = std::chrono::high_resolution_clock::now();

        // How long did it take?
        auto pingTimeMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(pongTime - lastPingSentAt).count();

        logger->info("Pong from firmware! ({}us)", pingTimeMicroseconds);

    }

} // creatures