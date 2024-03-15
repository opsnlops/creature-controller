
#include <cstdlib>
#include <csignal>
#include <future>
#include <thread>
#include <vector>

#include "controller-config.h"

#include "config/CommandLine.h"
#include "config/CreatureBuilder.h"
#include "controller/Controller.h"
#include "controller/tasks/PingTask.h"
#include "device/GPIO.h"
#include "dmx/E131Client.h"
#include "io/Message.h"
#include "io/MessageProcessor.h"
#include "io/MessageRouter.h"
#include "io/SerialHandler.h"
#include "logging/Logger.h"
#include "logging/SpdlogLogger.h"
#include "util/thread_name.h"
#include "util/StoppableThread.h"
#include "Version.h"


// Default to not shutting down
std::atomic<bool> shutdown_requested(false);


// Signal handler to stop the event loop
void signal_handler(int signal) {
    if (signal == SIGINT) {
        printf("stopping...\n\n");
        shutdown_requested.store(true);
    }
}

void timedShutdown(std::shared_ptr<creatures::StoppableThread> &workerThread, unsigned long timeout_ms) {
    auto shutdownTask = std::async(std::launch::async, [&]{
        workerThread->shutdown();
    });

    if(shutdownTask.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
        // Timeout reached, thread didn't shut down in time
        // Move on to the next thread
    }
}

int main(int argc, char **argv) {

    using creatures::io::Message;

    // Fire up the signal handlers
    std::signal(SIGINT, signal_handler);

    std::string version = fmt::format("{}.{}.{}",
                                      CREATURE_CONTROLLER_VERSION_MAJOR,
                                      CREATURE_CONTROLLER_VERSION_MINOR,
                                      CREATURE_CONTROLLER_VERSION_PATCH);

    // Get the logger up and running ASAP
    std::shared_ptr<creatures::Logger> logger = std::make_shared<creatures::SpdlogLogger>();
    logger->init("controller");

    logger->info("Welcome to the Creature Controller! v{} 🦜", version);

    // Leave some version info to be found
    logger->debug("spdlog version {}.{}.{}", SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH);
    logger->debug("fmt version {}", FMT_VERSION);


    // Parse out the command line options
    auto commandLine = std::make_unique<creatures::CommandLine>(logger);
    auto config = commandLine->parseCommandLine(argc, argv);


    auto builder = creatures::config::CreatureBuilder(logger, config->getCreatureConfigFile());
    auto creature = builder.build();


    // Hooray, we did it!
    logger->info("working with {}! ({})", creature->getName(), creature->getDescription());
    logger->debug("{} has {} servos and {} steppers", creature->getName(),
          creature->getNumberOfServos(), creature->getNumberOfSteppers());

    // Label the thread so it shows up in ps
    setThreadName("main for " + creature->getName());


    // Bring up the GPIO pins if enabled on the command line
    auto gpio = std::make_shared<creatures::device::GPIO>(logger, config->getUseGPIO());
    gpio->init();
    gpio->toggleFirmwareReset();

    // Keep track of our threads
    std::vector<std::shared_ptr<creatures::StoppableThread>> workerThreads;


    // Start up the SerialHandler
    auto topLevelOutgoingQueue = std::make_shared<creatures::MessageQueue<Message>>();
    auto topLevelIncomingQueue = std::make_shared<creatures::MessageQueue<Message>>();

    for( const auto& uart : config->getUARTDevices() ) {

        logger->debug("creating a SerialHandler for module {} on {}",
                      UARTDevice::moduleNameToString(uart.getModule()),
                      uart.getDeviceNode());

        // Create this UART's queues
        auto outgoingQueue = std::make_shared<creatures::MessageQueue<Message>>();
        auto incomingQueue = std::make_shared<creatures::MessageQueue<Message>>();

        // Create the SerialHandler and start it
        auto serialHandler = std::make_shared<creatures::SerialHandler>(logger, uart.getDeviceNode(), uart.getModule(),
                                                                        outgoingQueue, incomingQueue);
        serialHandler->start();

        // Add this SerialHandler's threads to the list of threads
        for( const auto& thread : serialHandler->threads ) {
            workerThreads.push_back(thread);
        }
    }

    // Make the MessageRouter
    auto messageRouter = std::make_shared<creatures::io::MessageRouter>(logger);

    // Fire up the controller
    auto controller = std::make_shared<Controller>(logger, creature, messageRouter);
    controller->start();
    workerThreads.push_back(controller);

    // Fire up the MessageProcessor
    auto messageProcessor = std::make_shared<creatures::MessageProcessor>(logger, messageRouter, controller);
    messageProcessor->start();

    // Now that the controller is running, we can start the creature
    creature->init(controller);
    creature->start();

    // Create and start the e1.13 client
    logger->debug("starting the e1.13 client");
    auto e131Client = std::make_unique<creatures::dmx::E131Client>(logger);
    e131Client->init(creature, controller, config->getNetworkDeviceIPAddress());
    e131Client->start();
    workerThreads.push_back(std::move(e131Client));

    // Before we start sending pings, ask the controller to flush its buffer
    controller->sendFlushBufferRequest();

    // Fire up the ping task
    auto pingTask = std::make_unique<creatures::tasks::PingTask>(logger, messageRouter);
    pingTask->start();
    workerThreads.push_back(std::move(pingTask));


    while(!shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds (500));
    }

    // Stop the creature first
    creature->shutdown();

    // Shut down the worker threads in LIFO order
    const unsigned long timeout_ms = 150;
    for (auto & workerThread : std::ranges::reverse_view(workerThreads)) {
        timedShutdown(workerThread, timeout_ms);
    }

    logger->info("the main thread says bye! good luck little threads! 👋🏻");
    return EXIT_SUCCESS;
}

