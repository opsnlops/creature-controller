
#pragma once

#include "logging/Logger.h"
#include "io/SerialHandler.h"
#include "util/StoppableThread.h"

#include "controller-config.h"

#define PING_SECONDS 5

namespace creatures::tasks {

    class PingTask : public StoppableThread {

    public:
        PingTask(std::shared_ptr<Logger> logger, std::shared_ptr<SerialHandler> serialHandler) :
            logger(logger), serialHandler(serialHandler) { }
        ~PingTask();

        void start() override;

    protected:
        void run() override;

    private:
        std::shared_ptr<Logger> logger;
        std::shared_ptr<SerialHandler> serialHandler;

    };

} // creatures::tasks


