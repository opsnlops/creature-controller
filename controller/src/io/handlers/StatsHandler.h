
#pragma once

#include "controller-config.h"
#include "io/handlers/IMessageHandler.h"
#include "logging/Logger.h"

#define SENSOR_MESSAGE "SENSOR"

namespace creatures {

class StatsHandler : public IMessageHandler {
  public:
    void handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) override;

  private:
    // The firmware's drop counters are cumulative, so we remember the previous
    // report to detect new drops. One StatsHandler lives per module, so this
    // state is correctly scoped to a single firmware board.
    bool haveDropBaseline = false;
    u64 lastOutgoingMessagesDropped = 0UL;
    u64 lastIncomingMessagesDropped = 0UL;
};

} // namespace creatures
