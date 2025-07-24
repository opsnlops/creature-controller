
#pragma once

#include "io/handlers/IMessageHandler.h"
#include "logging/Logger.h"

#define SENSOR_MESSAGE "SENSOR"

namespace creatures {

class StatsHandler : public IMessageHandler {
  public:
    void handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) override;
};

} // namespace creatures
