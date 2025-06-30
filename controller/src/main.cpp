/**
 * @file main.cpp
 * @brief Main entry point for the creature controller application
 *
 * Initializes the application, processes command line arguments,
 * builds the creature configuration, and runs the main controller.
 *
 * This version follows a simple philosophy: start things up cleanly,
 * and when it's time to shut down, just call shutdown() and trust
 * that everything will hop away gracefully!
 */

// Standard library includes
#include <cstdlib>
#include <csignal>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// Project includes
#include "controller-config.h"
#include "audio/AudioSubsystem.h"
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
#include "server/ServerConnection.h"
#include "server/ServerMessage.h"
#include "util/thread_name.h"
#include "util/StoppableThread.h"
#include "Version.h"

// Default to not shutting down
std::atomic<bool> shutdown_requested(false);

/**
 * @brief Signal handler to IMMEDIATELY shutdown the application - no more Mr. Nice Rabbit!
 * @param signal The received signal
 */
void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::cerr << "Caught SIGINT, shutting down IMMEDIATELY! 💥" << std::endl;

        // No more waiting around - just exit NOW!
        std::cout << "Bye! 🖖🏻" << std::endl;
        std::exit(EXIT_SUCCESS);
    }
}

/**
 * @brief Create a new logger with the specified name
 * @param name The name to assign to the logger
 * @return A shared pointer to the created logger
 */
std::shared_ptr<creatures::Logger> makeLogger(std::string name) {
    auto logger = std::make_shared<creatures::SpdlogLogger>();
    logger->init(std::move(name));
    return logger;
}

/**
 * @brief The audio subsystem for handling RTP audio reception
 */
std::shared_ptr<creatures::audio::AudioSubsystem> audioSubsystem;

/**
 * @brief Main entry point for the creature controller application
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return EXIT_SUCCESS on normal termination, EXIT_FAILURE otherwise
 */
int main(int argc, char **argv) {
    using creatures::io::Message;
    using creatures::server::ServerConnection;
    using creatures::config::UARTDevice;

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
        auto errorMessage = fmt::format("Unable to build configuration in memory: {}",
                                        configResult.getError().value().getMessage());
        std::cerr << errorMessage << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Yay, we have a valid config
    auto config = configResult.getValue().value();

    auto builder = creatures::config::CreatureBuilder(logger, config->getCreatureConfigFile());
    auto creatureResult = builder.build();
    if(!creatureResult.isSuccess()) {
        auto errorMessage = fmt::format("Unable to build the creature: {}",
                                       creatureResult.getError().value().getMessage());
        std::cerr << errorMessage << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Define it
    auto creature = creatureResult.getValue().value();

    // Make sure the creature believes it's ready to go
    auto preFlightCheckResult = creature->performPreFlightCheck();
    if(!preFlightCheckResult.isSuccess()) {
        auto errorMessage = fmt::format("Pre-flight check failed: {}",
                                       preFlightCheckResult.getError().value().getMessage());
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

    // Keep track of our threads - but keep it simple!
    std::vector<std::shared_ptr<creatures::StoppableThread>> workerThreads;

    // Start talking to the server, if we're told to
    auto websocketOutgoingQueue = std::make_shared<creatures::MessageQueue<creatures::server::ServerMessage>>();
    auto serverConnection = std::make_shared<ServerConnection>(
        makeLogger("server"),
        creature,
        config->isUsingServer(),
        config->getServerAddress(),
        config->getServerPort(),
        websocketOutgoingQueue
    );


    if (config->getUseAudioSubsystem()) {
        logger->info("🐰 Setting up audio subsystem...");

        audioSubsystem = std::make_shared<creatures::audio::AudioSubsystem>(logger);

        if (audioSubsystem->initialize(
            audio::DEFAULT_MULTICAST_GROUP,
            static_cast<uint16_t>(audio::DEFAULT_RTP_PORT),
            config->getSoundDeviceNumber(),
            config->getNetworkDeviceIPAddress())) {

            logger->info("🎵 Audio subsystem ready to go!");
            } else {
                logger->error("Failed to initialize audio subsystem");
                audioSubsystem.reset();
            }
    }

    // Start up if we should
    if(config->isUsingServer()) {
        serverConnection->start();
        workerThreads.push_back(serverConnection);
    }

    // Bring up the GPIO pins if enabled on the command line
    auto gpio = std::make_shared<creatures::device::GPIO>(makeLogger("gpio"), config->getUseGPIO());
    gpio->init();
    gpio->toggleFirmwareReset();

    // Make the MessageRouter (it will be started later in the boot process)
    auto messageRouter = std::make_shared<creatures::io::MessageRouter>(makeLogger("message-router"));

    // Fire up the controller
    auto controller = std::make_shared<Controller>(makeLogger("controller"), creature, messageRouter);
    controller->start();
    workerThreads.push_back(controller);

    /**
     * Create and start the ServoModuleHandler for the UART devices that were found in the config file
     */
    for(const auto& uart : config->getUARTDevices()) {
        logger->debug("creating a ServoModuleHandler for module {} on {}",
                      UARTDevice::moduleNameToString(uart.getModule()),
                      uart.getDeviceNode());

        std::string loggerName = fmt::format("uart-{}", UARTDevice::moduleNameToString(uart.getModule()));
        auto handler = std::make_shared<ServoModuleHandler>(
            makeLogger(loggerName),
            controller,
            uart.getModule(),
            uart.getDeviceNode(),
            messageRouter,
            websocketOutgoingQueue
        );

        // Register the handler with the message router
        messageRouter->registerServoModuleHandler(
            uart.getModule(),
            handler->getIncomingQueue(),
            handler->getOutgoingQueue()
        );

        logger->debug("init'ing the ServoModuleHandler for module {}",
                     UARTDevice::moduleNameToString(uart.getModule()));
        handler->init();

        logger->debug("starting the ServoModuleHandler for module {}",
                     UARTDevice::moduleNameToString(uart.getModule()));
        handler->start();

        workerThreads.push_back(handler);
    }

    // Now that the controller is running, we can start the creature
    creature->init(controller);
    creature->start();

    // Create and start the e1.31 client
    logger->debug("starting the e1.31 client");
    auto e131Client = std::make_unique<creatures::dmx::E131Client>(makeLogger("e131-client"));
    e131Client->init(creature, controller, config->getNetworkDeviceIPAddress());
    e131Client->start();
    workerThreads.push_back(std::move(e131Client));

    // Start the audio subsystem if it was initialized
    if (audioSubsystem) {
        audioSubsystem->start();
        workerThreads.push_back(audioSubsystem);
    }

    // Fire up the MessageRouter
    messageRouter->start();
    workerThreads.push_back(messageRouter);

    // Before we start sending pings, ask the controller to flush its buffer
    controller->sendFlushBufferRequest();

    // Fire up the ping task
    auto pingTask = std::make_unique<creatures::tasks::PingTask>(makeLogger("ping-task"), messageRouter);
    pingTask->start();
    workerThreads.push_back(std::move(pingTask));

    // Main loop - just run until the signal handler terminates us
    logger->info("All systems running! Press Ctrl+C to exit immediately.");
    while(true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        // The signal handler will terminate the process - no graceful shutdown needed!
    }

    // We should never reach here due to the signal handler, but just in case...
    return EXIT_SUCCESS;
}