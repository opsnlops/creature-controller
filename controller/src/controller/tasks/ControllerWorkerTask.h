
#pragma once

#include <atomic>

#include "controller/Controller.h"
#include "logging/Logger.h"

#include "controller-config.h"

namespace creatures::tasks {

    class ControllerWorkerTask {

    public:
        ControllerWorkerTask(std::shared_ptr<Logger> logger, std::shared_ptr<Controller> controller);
        ~ControllerWorkerTask() = default;

        void worker();
        void stop();

    private:
        std::atomic<bool> running = false;
        std::shared_ptr<Logger> logger;
        std::shared_ptr<Controller> controller;

        std::atomic<u64> number_of_frames = 0UL;

    };

} // creatures::tasks


