
#include <chrono>

#include "controller/Controller.h"
#include "controller/commands/SetServoPositions.h"
#include "creature/Creature.h"

#include "logging/Logger.h"

#include "controller-config.h"
#include "ControllerWorkerTask.h"

namespace creatures::tasks {

    ControllerWorkerTask::ControllerWorkerTask(std::shared_ptr<Logger> logger, std::shared_ptr<Controller> controller) :
        logger(logger), controller(controller) {

        logger->info("hello from the controller worker!");
    }

    void ControllerWorkerTask::stop() {
        running = false;
    }

    void ControllerWorkerTask::worker() {

        using namespace std::chrono;

        logger->info("controller worker now running");

        // Mark ourselves as running
        running = true;

        auto target_delta = microseconds( 1000000 / controller->getCreature()->getServoUpdateFrequencyHz());
        auto next_target_time = high_resolution_clock::now() + target_delta;

        while (running) {

            number_of_frames = number_of_frames + 1;


            // Go fetch the positions
            std::vector<creatures::ServoPosition> requestedPositions = controller->getCreature()->getRequestedServoPositions();

            auto command = std::make_shared<creatures::commands::SetServoPositions>(logger);
            for(auto& position : requestedPositions) {
                command->addServoPosition(position);
            }

            // Fire this off to the controller
            controller->sendCommand(command);

            // Tell the creature to get ready for next time
            controller->getCreature()->calculateNextServoPositions();

            // Figure out how much time we have until the next tick
            auto remaining_time = next_target_time - high_resolution_clock::now();

            // If there's time left, wait.
            if (remaining_time > nanoseconds(0)) {
                // Sleep for the remaining time
                std::this_thread::sleep_for(remaining_time);
            }

            // Update the target time for the next iteration
            next_target_time += target_delta;

        }

        logger->info("controller worker stopped");

    }

} // creatures::tasks