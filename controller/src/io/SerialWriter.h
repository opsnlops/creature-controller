
#pragma once

#include <string>
#include <thread>

#include "controller-config.h"
#include "logging/Logger.h"

#include "util/MessageQueue.h"
#include "util/StoppableThread.h"

namespace creatures::io {

    class SerialWriter : public StoppableThread {

    public:

        SerialWriter(const std::shared_ptr<Logger>& logger,
                     std::string deviceNode,
                     int fileDescriptor,
                     const std::shared_ptr<MessageQueue<std::string>>& outgoingQueue);

        ~SerialWriter() override {
            this->logger->info("SerialWriter destroyed");
        }

        void start() override;

    protected:
        void run() override;

    private:
        std::shared_ptr<Logger> logger;
        std::shared_ptr<MessageQueue<std::string>> outgoingQueue;
        std::string deviceNode;
        int fileDescriptor;
    };

} // creatures::io


