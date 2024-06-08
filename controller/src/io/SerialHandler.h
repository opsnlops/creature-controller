
#pragma once

#include <string>
#include <thread>
#include <vector>

#include "controller-config.h"

#include "config/UARTDevice.h"
#include "logging/Logger.h"
#include "io/Message.h"
#include "io/SerialReader.h"
#include "util/MessageQueue.h"
#include "util/Result.h"
#include "util/StoppableThread.h"

namespace creatures {

    using creatures::config::UARTDevice;
    using creatures::io::Message;

    class SerialHandler {

    public:
        /**
         * Creates a new SerialHandler
         *
         * @param logger our logger
         * @param deviceNode the device node to open up
         * @param moduleName the name of the module we are communicating with
         * @param outgoingQueue A `MessageQueue<Message>` for outgoing messages TO the remote device
         * @param incomingQueue A `MessageQueue<Message>` for incoming messages FROM the remote device
         */
        SerialHandler(const std::shared_ptr<Logger>& logger,
                      std::string deviceNode,
                      UARTDevice::module_name moduleName,
                      const std::shared_ptr<MessageQueue<Message>>& outgoingQueue,
                      const std::shared_ptr<MessageQueue<Message>>& incomingQueue);

        // Clean up the serial port
        ~SerialHandler();

        Result<bool> start();
        Result<bool> shutdown();

        std::shared_ptr<MessageQueue<Message>> getOutgoingQueue();
        std::shared_ptr<MessageQueue<Message>> getIncomingQueue();

        /**
         * Get the module name for this serial handler
         *
         * @return a `UARTDevice::module_name` for this serial handler
         */
        UARTDevice::module_name getModuleName();

        /*
         * This is public so that the threads can be registered with the servo module handler
         */
        std::vector<std::shared_ptr<creatures::StoppableThread>> threads;

    private:
        std::string deviceNode;
        UARTDevice::module_name moduleName;
        int fileDescriptor = -1;

        // A pointer to our shared MessageQueues
        std::shared_ptr<MessageQueue<Message>> outgoingQueue;
        std::shared_ptr<MessageQueue<Message>> incomingQueue;

        static bool isDeviceNodeAccessible(const std::shared_ptr<Logger>& logger, const std::string& deviceNode);

        Result<bool> setupSerialPort();
        Result<bool> closeSerialPort();

        std::shared_ptr<Logger> logger;

    };

} // creatures
