
#pragma once

#include <string>
#include <thread>

#include "controller-config.h"
#include "namespace-stuffs.h"

#include "util/MessageQueue.h"

namespace creatures {

    class SerialOutput {

    public:
        /**
         * Creates a new SerialOutput
         *
         * @param deviceNode the device node to open up
         * @param outgoingQueue A `MessageQueue<std::string>` for outgoing messages TO the remote device
         * @param incomingQueue A `MessageQueue<std::string>` for incoming messages FROM the remote device
         */
        SerialOutput(std::string deviceNode,
                     const std::shared_ptr<MessageQueue<std::string>>& outgoingQueue,
                     const std::shared_ptr<MessageQueue<std::string>>& incomingQueue);

        ~SerialOutput() = default;

        void start();
        void stop();

    private:
        std::string deviceNode;

        // A pointer to our shared MessageQueues
        std::shared_ptr<MessageQueue<std::string>> outgoingQueue;
        std::shared_ptr<MessageQueue<std::string>> incomingQueue;

        // Our threads
        std::thread readerThread;
        std::thread writerThread;

        static bool isDeviceNodeAccessible(const std::string& deviceNode);

        // Our threads
        void reader();

        [[noreturn]] void writer();

    };

} // creatures
