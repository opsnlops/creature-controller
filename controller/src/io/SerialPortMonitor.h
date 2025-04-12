#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include "logging/Logger.h"
#include "io/SerialHandler.h"

namespace creatures::io {

    /**
     * @brief Monitor for serial port connections
     *
     * This class monitors the connection status of a serial port and attempts to
     * reconnect if the connection is lost.
     */
    class SerialPortMonitor {
    public:
        /**
         * @brief Construct a new Serial Port Monitor
         *
         * @param logger Logger instance
         * @param serialHandler The serial handler to monitor
         * @param checkIntervalMs How often to check the connection status in milliseconds
         */
        SerialPortMonitor(std::shared_ptr<Logger> logger,
                          std::shared_ptr<SerialHandler> serialHandler,
                          unsigned int checkIntervalMs = 1000);

        /**
         * @brief Destroy the Serial Port Monitor
         */
        ~SerialPortMonitor();

        /**
         * @brief Start the monitoring thread
         */
        void start();

        /**
         * @brief Stop the monitoring thread
         */
        void stop();

    private:
        /**
         * @brief Main monitoring loop
         *
         * This runs in a separate thread and periodically checks the connection
         * status of the serial port.
         */
        void monitorLoop();

        std::shared_ptr<Logger> logger;
        std::shared_ptr<SerialHandler> serialHandler;
        unsigned int checkIntervalMs;

        std::atomic<bool> isRunning;
        std::thread monitorThread;
    };

} // namespace creatures::io