
#include <cstdlib>
#include <csignal>
#include <future>
#include <thread>
#include <vector>

#include "controller-config.h"

#include "config/CommandLine.h"
#include "config/CreatureBuilder.h"
#include "controller/Controller.h"
#include "controller/ServoModuleHandler.h"
#include "controller/tasks/PingTask.h"
#include "device/GPIO.h"
#include "dmx/E131Client.h"
#include "io/Message.h"
#include "io/MessageProcessor.h"
#include "io/MessageRouter.h"
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
        std::cerr << "Caught SIGINT, shutting down..." << std::endl;
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

std::shared_ptr<creatures::Logger> makeLogger(std::string name) {
    auto logger = std::make_shared<creatures::SpdlogLogger>();
    logger->init(name);
    return logger;
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
    std::shared_ptr<creatures::Logger> logger = makeLogger("main");

    // Print to the console as we start
    std::cout << fmt::format("Welcome to the Creature Controller! v{} 🦜", version) << std::endl << std::endl;

    // Leave some version info to be found
    logger->debug("spdlog version {}.{}.{}", SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH);
    logger->debug("fmt version {}", FMT_VERSION);


    // Parse out the command line options
    auto commandLine = std::make_unique<creatures::CommandLine>(logger);
    auto configResult = commandLine->parseCommandLine(argc, argv);

    if(!configResult.isSuccess()) {
        auto errorMessage = fmt::format("Unable to build configuration in memory: {}", configResult.getError().value().getMessage());
        std::cerr << errorMessage << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Yay, we have a valid config
    auto config = configResult.getValue().value();

    auto builder = creatures::config::CreatureBuilder(logger, config->getCreatureConfigFile());
    auto creatureResult = builder.build();
    if(!creatureResult.isSuccess()) {
        auto errorMessage = fmt::format("Unable to build the creature: {}", creatureResult.getError().value().getMessage());
        std::cerr << errorMessage << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Define it
    auto creature = creatureResult.getValue().value();

    // Make sure the creature believes it's ready to go
    auto preFlightCheckResult = creature->performPreFlightCheck();
    if(!preFlightCheckResult.isSuccess()) {
        auto errorMessage = fmt::format("Pre-flight check failed: {}", preFlightCheckResult.getError().value().getMessage());
        std::cerr << errorMessage << std::endl;
        std::exit(EXIT_FAILURE);
    }
    logger->info("Pre-flight check passed: {}", preFlightCheckResult.getValue().value());


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


    // Create the top level message queues
    auto topLevelOutgoingQueue = std::make_shared<creatures::MessageQueue<Message>>();
    auto topLevelIncomingQueue = std::make_shared<creatures::MessageQueue<Message>>();

    // Make the MessageRouter
    auto messageRouter = std::make_shared<creatures::io::MessageRouter>(logger);

    // Fire up the controller
    auto controller = std::make_shared<Controller>(logger, creature, messageRouter);
    controller->start();
    workerThreads.push_back(controller);



    /**
     * Create and start the ServoModuleHandler for the UART devices that were found in the config file
     */
    for( const auto& uart : config->getUARTDevices() ) {

        logger->debug("creating a ServoModuleHandler for module {} on {}",
                      UARTDevice::moduleNameToString(uart.getModule()),
                      uart.getDeviceNode());

        /*
         * ServoModuleHandler(std::shared_ptr<Logger> logger,
                           UARTDevice::module_name moduleId,
                           std::string deviceNode,
                           std::shared_ptr<creatures::io::MessageRouter> messageRouter);
         */


        std::string loggerName = fmt::format("uart-{}", UARTDevice::moduleNameToString(uart.getModule()));
        auto handler = std::make_shared<ServoModuleHandler>(makeLogger(loggerName),
                                                            uart.getModule(),
                                                            uart.getDeviceNode(),
                                                            messageRouter);

        logger->debug("init'ing the ServoModuleHandler for module {}", UARTDevice::moduleNameToString(uart.getModule()));
        handler->init();

        logger->debug("starting the ServoModuleHandler for module {}", UARTDevice::moduleNameToString(uart.getModule()));
        handler->start();

        workerThreads.push_back(handler);

    }

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

    logger->debug("waiting for a few seconds to let everyone clean up");
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout << "Bye! 🖖🏻" << std::endl;
    std::exit(EXIT_SUCCESS);

}

