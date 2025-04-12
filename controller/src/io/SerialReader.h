#pragma once

#include <atomic>
#include <string>
#include <thread>

#include "controller-config.h"
#include "logging/Logger.h"
#include "io/Message.h"
#include "config/UARTDevice.h"
#include "util/MessageQueue.h"
#include "util/StoppableThread.h"

namespace creatures::io {

    using creatures::config::UARTDevice;
    using creatures::io::Message;

    /**
     * A thread that reads from a serial port and places the messages into a queue
     */
    class SerialReader : public StoppableThread {

    public:

        SerialReader(const std::shared_ptr<Logger>& logger,
                     std::string deviceNode,
                     UARTDevice::module_name moduleName,
                     int fileDescriptor,
                     const std::shared_ptr<MessageQueue<Message>>& incomingQueue,
        std::atomic<bool>& resources_valid,
                std::atomic<bool>& port_connected);

        ~SerialReader() override {
            this->logger->info("SerialReader destroyed");
        }

        void start() override;

    protected:
        void run() override;

    private:
        std::shared_ptr<Logger> logger;
        std::shared_ptr<MessageQueue<Message>> incomingQueue;
        std::string deviceNode;
        UARTDevice::module_name moduleName;
        int fileDescriptor;
        std::atomic<bool>& resources_valid; // Reference to parent's resources_valid flag
        std::atomic<bool>& port_connected;  // Reference to parent's port_connected flag
    };

} // creatures::io