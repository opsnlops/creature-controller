
#include <string>
#include <vector>


#include "logging/Logger.h"
#include "MockMessageHandler.h"

namespace creatures {

    void MockMessageHandler::handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) {
       // Do nothing
    }

} // creatures