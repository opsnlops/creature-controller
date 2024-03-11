
#include <chrono>

#include "logging/Logger.h"
#include "controller/commands/Ping.h"
#include "controller/tasks/PingTask.h"
#include "util/thread_name.h"


// Keep track of the last time we sent a ping
std::chrono::time_point<std::chrono::high_resolution_clock> lastPingSentAt = std::chrono::high_resolution_clock::now();

#define PING_LOOP_TIME 1000

namespace creatures::tasks {


    PingTask::~PingTask() {
        this->logger->info("ping task destroyed");
    }

    void PingTask::start() {
        logger->info("starting the ping task");
        creatures::StoppableThread::start();
    }

    void PingTask::run() {

        // Set thread name
        setThreadName("creatures::tasks::PingTask");
        logger->info("hello from the ping task!");

        // How long should we wait between pings?
        std::chrono::milliseconds interval(PING_LOOP_TIME);

        // Don't start out at zero so we don't send a ping right away
        u64 totalMilliseconds = PING_LOOP_TIME;

        while (!stop_requested.load()) {
            std::this_thread::sleep_for(interval);

            // This cycles every interval so that it's responsive to a shutdown, but
            // don't ping each time 😅
            if(totalMilliseconds % (PING_SECONDS * PING_LOOP_TIME) == 0) {
                auto pingCommand = creatures::commands::Ping(logger);
                serialHandler->getOutgoingQueue()->push(pingCommand.toMessageWithChecksum());
                lastPingSentAt  = std::chrono::high_resolution_clock::now();

                logger->debug("sent ping");
            }

            totalMilliseconds += PING_LOOP_TIME;
        }

        logger->info("ping task shutting down");
    }

} // creatures::tasks
