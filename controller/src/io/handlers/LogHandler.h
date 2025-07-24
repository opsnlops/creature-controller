
#pragma once

#include "io/handlers/IMessageHandler.h"
#include "logging/Logger.h"

namespace creatures {

class LogHandler : public IMessageHandler {
  public:
    void handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) override;
};

} // namespace creatures
