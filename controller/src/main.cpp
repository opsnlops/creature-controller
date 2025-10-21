/**
 * @file main.cpp
 * @brief Main entry point for the creature controller application
 *
 * Initializes the application, processes command line arguments,
 * builds the creature configuration, and runs the main controller.
 *
 * This version follows a simple philosophy: start things up cleanly,
 * and when it's time to shut down, just call shutdown() and trust
 * that everything will shut down gracefully!
 */

// Standard library includes
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <opus.h>

// Third-party includes
#include <ixwebsocket/IXHttpClient.h>
#include <nlohmann/json.hpp>

// Project includes
#include "Version.h"
#include "audio/AudioSubsystem.h"
#include "config/BaseBuilder.h"
#include "config/CommandLine.h"
#include "config/CreatureBuilder.h"
#include "controller-config.h"
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
#include "util/StoppableThread.h"
#include "util/thread_name.h"
#include "watchdog/WatchdogThread.h"

// Default to not shutting down
std::atomic<bool> shutdown_requested(false);

// Track SIGINT signals for fail-safe mechanism
std::atomic<int> sigint_count(0);

/**
 * @brief Signal handler to gracefully shutdown the application
 * @param signal The received signal
 */
void signal_handler(int signal) {
    if (signal == SIGINT) {
        int current_count = sigint_count.fetch_add(1) + 1;

        if (current_count == 1) {
            // First SIGINT - start graceful shutdown
            std::cerr << "Caught SIGINT, requesting graceful shutdown..." << std::endl;
            std::cerr << "(Press Ctrl+C again for immediate hard shutdown)" << std::endl;
            shutdown_requested.store(true);
        } else {
            // Second or subsequent SIGINT - perform immediate hard shutdown
            std::cerr << "Second SIGINT received, performing immediate hard shutdown..." << std::endl;
            std::_Exit(EXIT_FAILURE);
        }
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
 * @brief Register the creature configuration with the server
 *
 * This function attempts to register the creature's configuration with the server
 * by sending the JSON config file content and universe assignment to the server's
 * /api/v1/creature/register endpoint. This provides a backup of the creature's
 * critical configuration.
 *
 * Errors are logged but non-fatal - the controller will continue running even if
 * registration fails (e.g., if the server is down).
 *
 * @param logger Shared pointer to a logger instance
 * @param serverAddress The server's address
 * @param serverPort The server's port
 * @param creatureConfigFile Path to the creature's JSON config file
 * @param universe The universe this creature is assigned to
 * @return true if registration succeeded, false otherwise
 */
bool registerCreatureWithServer(std::shared_ptr<creatures::Logger> logger,
                                const std::string& serverAddress,
                                u16 serverPort,
                                const std::string& creatureConfigFile,
                                u16 universe) {

    logger->info("Registering creature with server at {}:{}...", serverAddress, serverPort);

    // Load the creature config file content
    auto configFileResult = creatures::config::BaseBuilder::loadFile(logger, creatureConfigFile);
    if (!configFileResult.isSuccess()) {
        logger->error("Failed to load creature config file for registration: {}",
                     configFileResult.getError().value().getMessage());
        return false;
    }

    std::string creatureConfigContent = configFileResult.getValue().value();

    // Build the JSON request body matching RegisterCreatureRequestDto
    nlohmann::json requestBody;
    requestBody["creature_config"] = creatureConfigContent;  // String: raw JSON content
    requestBody["universe"] = universe;                       // UInt32: universe number

    std::string requestBodyStr = requestBody.dump();

    // Build the registration URL
    std::string url = fmt::format("http://{}:{}/api/v1/creature/register",
                                 serverAddress, serverPort);

    logger->debug("Registration URL: {}", url);
    logger->debug("Universe: {}", universe);

    // Create HTTP client and configure request
    ix::HttpClient httpClient;
    auto args = std::make_shared<ix::HttpRequestArgs>();
    args->extraHeaders["Content-Type"] = "application/json";
    args->connectTimeout = 10;  // 10 second connection timeout
    args->transferTimeout = 30; // 30 second transfer timeout

    // Make the POST request
    auto response = httpClient.post(url, requestBodyStr, args);

    // Handle HTTP errors
    if (response->errorCode != ix::HttpErrorCode::Ok) {
        logger->error("HTTP error during creature registration: {} - {}",
                     static_cast<int>(response->errorCode),
                     response->errorMsg);
        return false;
    }

    // Check server response status
    if (response->statusCode == 200) {
        logger->info("Successfully registered creature with server");
        logger->debug("Server response: {}", response->body);
        return true;
    } else {
        logger->warn("Server registration returned status {}: {}",
                    response->statusCode,
                    response->body);
        return false;
    }
}

/**
 * @brief Main entry point for the creature controller application
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return EXIT_SUCCESS on normal termination, EXIT_FAILURE otherwise
 */
int main(int argc, char **argv) {
    using creatures::config::UARTDevice;
    using creatures::io::Message;
    using creatures::server::ServerConnection;

    // Fire up the signal handlers
    std::signal(SIGINT, signal_handler);

    std::string version = fmt::format("{}.{}.{}", CREATURE_CONTROLLER_VERSION_MAJOR, CREATURE_CONTROLLER_VERSION_MINOR,
                                      CREATURE_CONTROLLER_VERSION_PATCH);

    // Get the logger up and running ASAP
    std::shared_ptr<creatures::Logger> logger = makeLogger("main");

    // Print to the console as we start
    std::cout << fmt::format("Welcome to the Creature Controller! v{}", version) << std::endl << std::endl;

    // Leave some version info to be found
    logger->debug("spdlog version {}.{}.{}", SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH);
    logger->debug("fmt version {}", FMT_VERSION);
    logger->debug("opus version {}", opus_get_version_string());

    // Parse out the command line options
    auto commandLine = std::make_unique<creatures::CommandLine>(logger);
    auto configResult = commandLine->parseCommandLine(argc, argv);

    if (!configResult.isSuccess()) {
        auto errorMessage =
            fmt::format("Unable to build configuration in memory: {}", configResult.getError().value().getMessage());
        std::cerr << errorMessage << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Yay, we have a valid config
    auto config = configResult.getValue().value();

    auto builder = creatures::config::CreatureBuilder(logger, config->getCreatureConfigFile());
    auto creatureResult = builder.build();
    if (!creatureResult.isSuccess()) {
        auto errorMessage =
            fmt::format("Unable to build the creature: {}", creatureResult.getError().value().getMessage());
        std::cerr << errorMessage << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Define it
    auto creature = creatureResult.getValue().value();

    // Make sure the creature believes it's ready to go
    auto preFlightCheckResult = creature->performPreFlightCheck();
    if (!preFlightCheckResult.isSuccess()) {
        auto errorMessage =
            fmt::format("Pre-flight check failed: {}", preFlightCheckResult.getError().value().getMessage());
        std::cerr << errorMessage << std::endl;
        std::exit(EXIT_FAILURE);
    }
    logger->info("Pre-flight check passed: {}", preFlightCheckResult.getValue().value());

    // Hooray, we did it!
    logger->info("working with {}! ({})", creature->getName(), creature->getDescription());
    logger->debug("{} has {} servos and {} steppers", creature->getName(), creature->getNumberOfServos(),
                  creature->getNumberOfSteppers());

    // Label the thread so it shows up in ps
    setThreadName("main for " + creature->getName());

    // Keep track of our threads - but keep it simple!
    std::vector<std::shared_ptr<creatures::StoppableThread>> workerThreads;

    // Start talking to the server if we're told to
    auto websocketOutgoingQueue = std::make_shared<creatures::MessageQueue<creatures::server::ServerMessage>>();
    auto serverConnection =
        std::make_shared<ServerConnection>(makeLogger("server"), creature, config->isUsingServer(),
                                           config->getServerAddress(), config->getServerPort(), websocketOutgoingQueue);

    if (config->getUseAudioSubsystem()) {
        logger->info("Setting up audio subsystem...");

        audioSubsystem = std::make_shared<creatures::audio::AudioSubsystem>(makeLogger("audio"));

        // Use the creature's audio channel for dialog stream
        uint8_t creatureAudioChannel = creature->getAudioChannel();

        // Validate that the creature has a valid audio channel
        if (creatureAudioChannel < 1 || creatureAudioChannel > 16) {
            logger->warn("Creature {} has invalid audio channel {}, using channel 1", creature->getName(),
                         creatureAudioChannel);
            creatureAudioChannel = 1;
        }

        if (audioSubsystem->initialize(creatureAudioChannel,                // Dialog channel (1-16)
                                       config->getNetworkDeviceIPAddress(), // Network interface
                                       config->getSoundDeviceNumber())) {   // SDL audio device index

            logger->info("Audio subsystem initialized: dialog channel {}, BGM channel 17", creatureAudioChannel);
        } else {
            logger->error("Failed to initialize audio subsystem");
            audioSubsystem.reset();
        }
    }

    // Start up if we should
    if (config->isUsingServer()) {
        serverConnection->start();
        workerThreads.push_back(serverConnection);

        // Register the creature with the server (non-fatal if it fails)
        registerCreatureWithServer(logger, config->getServerAddress(), config->getServerPort(),
                                  config->getCreatureConfigFile(), config->getUniverse());
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
    for (const auto &uart : config->getUARTDevices()) {
        logger->debug("creating a ServoModuleHandler for module {} on {}",
                      UARTDevice::moduleNameToString(uart.getModule()), uart.getDeviceNode());

        std::string loggerName = fmt::format("uart-{}", UARTDevice::moduleNameToString(uart.getModule()));
        auto handler =
            std::make_shared<ServoModuleHandler>(makeLogger(loggerName), controller, uart.getModule(),
                                                 uart.getDeviceNode(), messageRouter, websocketOutgoingQueue);

        // Register the handler with the message router
        messageRouter->registerServoModuleHandler(uart.getModule(), handler->getIncomingQueue(),
                                                  handler->getOutgoingQueue());

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
    e131Client->init(creature, controller, config->getUniverse(), config->getNetworkDeviceName(),
                     config->getNetworkDeviceIndex(), config->getNetworkDeviceIPAddress());
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

    // Start the watchdog thread
    logger->debug("starting the watchdog thread");
    auto watchdogThread = std::make_shared<creatures::watchdog::WatchdogThread>(makeLogger("watchdog"), config,
                                                                                websocketOutgoingQueue, messageRouter);
    watchdogThread->start();
    workerThreads.push_back(watchdogThread);

    // Before we start sending pings, ask the controller to flush its buffer
    controller->sendFlushBufferRequest();

    // Fire up the ping task
    auto pingTask = std::make_unique<creatures::tasks::PingTask>(makeLogger("ping-task"), messageRouter);
    pingTask->start();
    workerThreads.push_back(std::move(pingTask));

    // Main loop - run until shutdown is requested
    logger->info("All systems running! Press Ctrl+C to shutdown gracefully.");
    while (!shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Graceful shutdown sequence
    logger->info("Shutdown requested, stopping all threads...");

    // Stop all threads in reverse order of creation
    for (auto it = workerThreads.rbegin(); it != workerThreads.rend(); ++it) {
        if (*it) {
            logger->info("Stopping thread: {}", (*it)->getName());
            (*it)->shutdown();
            logger->debug("Thread {} shutdown complete", (*it)->getName());
        }
    }

    // Stop the creature
    if (creature) {
        logger->debug("Stopping creature: {}", creature->getName());
        creature->shutdown();
    }

    // Clean up audio subsystem
    if (audioSubsystem) {
        logger->debug("Stopping audio subsystem");
        audioSubsystem->shutdown();
        audioSubsystem.reset();
    }

    logger->info("Graceful shutdown complete.");
    return EXIT_SUCCESS;
}