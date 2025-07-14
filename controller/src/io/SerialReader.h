//
// SerialReader.h
//

#pragma once

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
     *
     * This class follows a fail-fast philosophy: if anything goes wrong with the
     * serial port, it cleanly shuts down rather than trying to recover. Sometimes
     * the best thing a rabbit can do is know when to hop away!
     */
    class SerialReader : public StoppableThread {

    public:

        SerialReader(const std::shared_ptr<Logger>& logger,
                     std::string deviceNode,
                     UARTDevice::module_name moduleName,
                     int fileDescriptor,
                     const std::shared_ptr<MessageQueue<Message>>& incomingQueue);

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
    };

} // creatures::io