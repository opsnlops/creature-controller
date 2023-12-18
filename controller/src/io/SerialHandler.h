
#pragma once

#include <string>
#include <thread>

#include "controller-config.h"
#include "logging/Logger.h"
#include "util/MessageQueue.h"

namespace creatures {

    class SerialHandler {

    public:
        /**
         * Creates a new SerialHandler
         *
         * @param logger our logger
         * @param deviceNode the device node to open up
         * @param outgoingQueue A `MessageQueue<std::string>` for outgoing messages TO the remote device
         * @param incomingQueue A `MessageQueue<std::string>` for incoming messages FROM the remote device
         */
        SerialHandler(const std::shared_ptr<Logger>& logger,
                      std::string deviceNode,
                      const std::shared_ptr<MessageQueue<std::string>>& outgoingQueue,
                      const std::shared_ptr<MessageQueue<std::string>>& incomingQueue);

        ~SerialHandler() = default;

        void start();
        void stop();

        std::shared_ptr<MessageQueue<std::string>> getOutgoingQueue();
        std::shared_ptr<MessageQueue<std::string>> getIncomingQueue();

    private:
        std::string deviceNode;
        int fileDescriptor;

        // A pointer to our shared MessageQueues
        std::shared_ptr<MessageQueue<std::string>> outgoingQueue;
        std::shared_ptr<MessageQueue<std::string>> incomingQueue;

        // Our threads
        std::thread readerThread;
        std::thread writerThread;

        static bool isDeviceNodeAccessible(std::shared_ptr<Logger> logger, const std::string& deviceNode);

        bool setupSerialPort();

        // Our threads
        void reader();

        [[noreturn]] void writer();

        std::shared_ptr<Logger> logger;

    };

} // creatures
