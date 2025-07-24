
#pragma once

#include "controller-config.h"

#include "controller/commands/ICommand.h"
#include "logging/Logger.h"

namespace creatures::commands {

/**
 * This is a very simple simple message that contains only a single character.
 * It's a magic signal to the firmware to flush its buffer and get ready to
 * start over. That magic character is `\a`, a bell character. 🔔
 */
class FlushBuffer final : public ICommand {

  public:
    explicit FlushBuffer(const std::shared_ptr<Logger> &logger);
    std::string toMessage() override;

  private:
    std::shared_ptr<Logger> logger;
};

} // namespace creatures::commands
