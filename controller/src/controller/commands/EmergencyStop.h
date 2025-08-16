#pragma once

#include "controller-config.h"
#include "controller/commands/ICommand.h"
#include "logging/Logger.h"

namespace creatures::commands {

class EmergencyStop final : public ICommand {

  public:
    explicit EmergencyStop(std::shared_ptr<Logger> logger);
    std::string toMessage() override;

  private:
    std::shared_ptr<Logger> logger;
};

} // namespace creatures::commands