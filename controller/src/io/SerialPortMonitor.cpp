#include <chrono>
#include <thread>

#include "config/UARTDevice.h"
#include "io/SerialPortMonitor.h"
#include "io/SerialHandler.h" // Explicitly include SerialHandler
#include "util/thread_name.h"

namespace creatures::io {

    using creatures::config::UARTDevice;

    SerialPortMonitor::SerialPortMonitor(const std::shared_ptr<Logger>& logger,
                                         std::string deviceNode,
                                         UARTDevice::module_name moduleName,
                                         creatures::SerialHandler& serialHandler) :
            logger(logger),
            deviceNode(deviceNode),
            moduleName(moduleName),
            serialHandler(serialHandler) {

        this->logger->info("creating a new SerialPortMonitor for module {} on {}",
                           UARTDevice::moduleNameToString(moduleName), deviceNode);
    }

    void SerialPortMonitor::start() {
        this->logger->info("starting the port monitor thread");
        creatures::StoppableThread::start();
    }

    void SerialPortMonitor::run() {
        // Store a local copy of logger for safety
        auto localLogger = this->logger;

        localLogger->info("hello from the port monitor thread for {} 🔍", this->deviceNode);

        this->threadName = fmt::format("SerialPortMonitor::run for {}", this->deviceNode);
        setThreadName(threadName);

        int reconnectAttempts = 0;

        while(!stop_requested.load()) {
            // Sleep first to give initial connection time to establish properly
            std::this_thread::sleep_for(std::chrono::milliseconds(SERIAL_PORT_CHECK_INTERVAL_MS));

            // Check if we should stop before doing any work
            if (stop_requested.load()) {
                break;
            }

            // Check if we should stop
            if (stop_requested.load()) {
                break;
            }

            // Check port connection status with safety
            bool isConnected = false;
            try {
                isConnected = serialHandler.isPortConnected();
            } catch (const std::exception& e) {
                localLogger->error("Exception checking port connection: {}", e.what());
                // Something went wrong with the handler, exit the thread
                break;
            } catch (...) {
                localLogger->error("Unknown exception checking port connection");
                // Something went wrong with the handler, exit the thread
                break;
            }

            if (!isConnected) {
                localLogger->warn("Serial port {} appears to be disconnected", this->deviceNode);

                // Only try reconnecting a limited number of times
                if (reconnectAttempts < SERIAL_PORT_MAX_RECONNECT_ATTEMPTS) {
                    reconnectAttempts++;
                    localLogger->info("Reconnect attempt {} of {}", reconnectAttempts, SERIAL_PORT_MAX_RECONNECT_ATTEMPTS);

                    // Try to reconnect with safety checks
                    try {
                        auto result = serialHandler.reconnect();
                        if (result.isSuccess()) {
                            localLogger->info("Successfully reconnected to {}", this->deviceNode);
                            reconnectAttempts = 0; // Reset counter on successful reconnect
                        } else {
                            // Wait before attempting to reconnect again
                            std::this_thread::sleep_for(std::chrono::milliseconds(SERIAL_PORT_RECONNECT_DELAY_MS));
                        }
                    } catch (const std::exception& e) {
                        localLogger->error("Exception during reconnect: {}", e.what());
                        break; // Exit if the handler is broken
                    } catch (...) {
                        localLogger->error("Unknown exception during reconnect");
                        break; // Exit for unknown exceptions too
                    }
                } else {
                    localLogger->error("Maximum reconnect attempts ({}) reached for {}",
                                       SERIAL_PORT_MAX_RECONNECT_ATTEMPTS, this->deviceNode);
                    // After max reconnect attempts, slow down checking to avoid log spam
                    std::this_thread::sleep_for(std::chrono::milliseconds(SERIAL_PORT_CHECK_INTERVAL_MS * 3));
                }
            } else {
                // Reset reconnect attempts counter if port is connected
                if (reconnectAttempts > 0) {
                    localLogger->info("Port {} is now stable", this->deviceNode);
                    reconnectAttempts = 0;
                }
            }
        }

        localLogger->info("port monitor thread stopping");
    }
}