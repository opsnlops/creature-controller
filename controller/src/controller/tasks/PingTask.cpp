
#include <chrono>

#include "logging/Logger.h"
#include "controller/commands/Ping.h"
#include "controller/tasks/PingTask.h"
#include "util/thread_name.h"


// Keep track of the last time we sent a ping
std::chrono::time_point<std::chrono::high_resolution_clock> lastPingSentAt = std::chrono::high_resolution_clock::now();

namespace creatures::tasks {



    void PingTask::init(std::shared_ptr<SerialHandler> serialHandler) {
        this->serialHandler = serialHandler;
        logger->init("init'ed the ping task");

        running = false;
    }

    void PingTask::start() {
        running = true;
        workerThread = std::thread(&PingTask::worker, this);

        // Bye!
        workerThread.detach();
    }

    void PingTask::worker() {

        // Set thread name
        setThreadName("creatures::tasks::PingTask");
        logger->info("hello from the ping task! 🙋🏼‍♀️");

        // How long should we wait between pings?
        std::chrono::milliseconds interval(PING_INTERVAL_MS);

        while (running) {
            std::this_thread::sleep_for(interval);

            auto pingCommand = creatures::commands::Ping(logger);
            serialHandler->getOutgoingQueue()->push(pingCommand.toMessageWithChecksum());
            lastPingSentAt  = std::chrono::high_resolution_clock::now();

            logger->debug("sent ping");

        }

        logger->info("ping task shutting down");
    }

} // creatures::tasks
