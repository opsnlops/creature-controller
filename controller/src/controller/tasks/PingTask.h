
#pragma once

#include "io/MessageRouter.h"
#include "logging/Logger.h"
#include "util/StoppableThread.h"

#include "controller-config.h"

#define PING_SECONDS 5

namespace creatures::tasks {

using creatures::io::MessageRouter;

class PingTask : public StoppableThread {

  public:
    PingTask(std::shared_ptr<Logger> logger, const std::shared_ptr<MessageRouter> &messageRouter)
        : logger(logger), messageRouter(messageRouter) {}
    ~PingTask();

    void start() override;

  protected:
    void run() override;

  private:
    std::shared_ptr<Logger> logger;
    std::shared_ptr<MessageRouter> messageRouter;
};

} // namespace creatures::tasks
