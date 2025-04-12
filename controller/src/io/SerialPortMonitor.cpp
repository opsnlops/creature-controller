#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include "config/UARTDevice.h"
#include "io/SerialHandler.h"
#include "io/SerialPortMonitor.h"
#include "logging/Logger.h"
#include "util/thread_name.h"

namespace creatures::io {

    SerialPortMonitor::SerialPortMonitor(std::shared_ptr<Logger> logger,
                                         std::shared_ptr<SerialHandler> serialHandler,
                                         unsigned int checkIntervalMs)
            : logger(logger),
              serialHandler(serialHandler),
              checkIntervalMs(checkIntervalMs),
              isRunning(false) {

        logger->info("Creating SerialPortMonitor for device {}",
                     config::UARTDevice::moduleNameToString(serialHandler->getModuleName()));
    }

    SerialPortMonitor::~SerialPortMonitor() {
        // Make sure we stop the monitoring thread cleanly
        stop();
        logger->debug("SerialPortMonitor destroyed");
    }

    void SerialPortMonitor::start() {
        logger->info("Starting SerialPortMonitor");

        // Don't start if we're already running
        if (isRunning.load()) {
            logger->warn("SerialPortMonitor already running, not starting again");
            return;
        }

        // Set the running flag
        isRunning.store(true);

        // Start the monitoring thread
        monitorThread = std::thread(&SerialPortMonitor::monitorLoop, this);
    }

    void SerialPortMonitor::stop() {
        logger->info("Stopping SerialPortMonitor");

        // Signal the thread to stop
        isRunning.store(false);

        // Wait for the thread to finish if it's running
        if (monitorThread.joinable()) {
            monitorThread.join();
        }

        logger->debug("SerialPortMonitor stopped");
    }

    void SerialPortMonitor::monitorLoop() {
        // Set a thread name for debugging
        setThreadName("SerialPortMonitor");

        logger->info("SerialPortMonitor thread started");

        // Track the connection state
        bool isConnected = false;

        // Main monitoring loop
        while (isRunning.load()) {
            try {
                // Check the current connection state
                if (serialHandler) {
                    isConnected = serialHandler->isPortConnected();

                    // If we lost the connection, try to reconnect
                    if (!isConnected) {
                        logger->warn("Serial port disconnected, attempting to reconnect");

                        // Try to reconnect
                        if (serialHandler) {
                            auto result = serialHandler->reconnect();
                            if (result.isSuccess()) {
                                logger->info("Successfully reconnected to serial port");
                            } else {
                                logger->error("Failed to reconnect to serial port: {}",
                                              result.getError()->getMessage());
                            }
                        }
                    }
                } else {
                    logger->error("SerialHandler is null in SerialPortMonitor");
                }
            } catch (const std::exception& e) {
                // Catch any exceptions to prevent thread termination
                logger->error("Exception in SerialPortMonitor: {}", e.what());
            }

            // Sleep for the check interval
            std::this_thread::sleep_for(std::chrono::milliseconds(checkIntervalMs));
        }

        logger->info("SerialPortMonitor thread exiting");
    }

} // namespace creatures::io