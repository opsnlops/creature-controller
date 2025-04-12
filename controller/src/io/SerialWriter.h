#pragma once

#include <atomic>
#include <string>
#include <thread>

#include "controller-config.h"
#include "config/UARTDevice.h"
#include "logging/Logger.h"
#include "io/Message.h"
#include "util/MessageQueue.h"
#include "util/StoppableThread.h"

namespace creatures::io {

    using creatures::config::UARTDevice;
    using creatures::io::Message;

    class SerialWriter : public StoppableThread {

    public:

        SerialWriter(const std::shared_ptr<Logger>& logger,
                     std::string deviceNode,
                     UARTDevice::module_name moduleName,
                     int fileDescriptor,
                     const std::shared_ptr<MessageQueue<Message>>& outgoingQueue,
        std::atomic<bool>& resources_valid,
                std::atomic<bool>& port_connected);

        ~SerialWriter() override {
            this->logger->info("SerialWriter destroyed");
        }

        void start() override;

    protected:
        void run() override;

    private:
        std::shared_ptr<Logger> logger;
        std::shared_ptr<MessageQueue<Message>> outgoingQueue;
        std::string deviceNode;
        UARTDevice::module_name moduleName;
        int fileDescriptor;
        std::atomic<bool>& resources_valid; // Reference to parent's resources_valid flag
        std::atomic<bool>& port_connected;  // Reference to parent's port_connected flag
    };

} // creatures::io