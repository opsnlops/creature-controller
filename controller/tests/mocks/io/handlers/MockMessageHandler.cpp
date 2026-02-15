
#include <string>
#include <vector>

#include "MockMessageHandler.h"
#include "logging/Logger.h"

namespace creatures {

void MockMessageHandler::handle(std::shared_ptr<Logger> /*logger*/, const std::vector<std::string> & /*tokens*/) {
    // Do nothing
}

} // namespace creatures