
#pragma once

#include "logging/Logger.h"
#include "io/SerialHandler.h"

#include "controller-config.h"

#define PING_INTERVAL_MS 5000


namespace creatures::tasks {

        class PingTask {

        public:
            PingTask(std::shared_ptr<Logger> logger) : logger(logger) { }

            ~PingTask() = default;

            void init(std::shared_ptr<SerialHandler> serialHandler);
            void start();
            void stop();

        private:
            std::shared_ptr<Logger> logger;
            std::shared_ptr<SerialHandler> serialHandler;
            bool running;
            std::thread workerThread;

            /**
             * The thread that actually does the work, started by start()
             */
            void worker();

        };

} // creatures::tasks


