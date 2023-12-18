

#include <string>
#include <vector>

#include "controller-config.h"
#include "namespace-stuffs.h"

#include "io/handlers/StatsHandler.h"

namespace creatures {

    void StatsHandler::handle(const std::vector<std::string> &tokens) {

        debug("incoming stats!");
        for(std::string token : tokens) {
            debug(" {}", token);
        }
    }

} // creatures