//
// SerialWriter.h
//

#pragma once

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

    /**
     * A thread that writes messages to a serial port
     *
     * This class follows a fail-fast philosophy: if anything goes wrong with the
     * serial port, it cleanly shuts down rather than trying to recover. Sometimes
     * the best thing a rabbit can do is know when to hop away!
     */
    class SerialWriter : public StoppableThread {

    public:

        SerialWriter(const std::shared_ptr<Logger>& logger,
                     std::string deviceNode,
                     UARTDevice::module_name moduleName,
                     int fileDescriptor,
                     const std::shared_ptr<MessageQueue<Message>>& outgoingQueue);

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
    };

} // creatures::io