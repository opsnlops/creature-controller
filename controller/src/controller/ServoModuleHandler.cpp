

#include "config/UARTDevice.h"
#include "controller/ServoModuleHandler.h"

#include <utility>
#include "io/Message.h"
#include "io/MessageProcessor.h"
#include "io/MessageRouter.h"
#include "io/SerialHandler.h"
#include "logging/Logger.h"
#include "util/MessageQueue.h"
#include "util/StoppableThread.h"

namespace creatures {

    using creatures::MessageProcessor;
    using creatures::SerialHandler;
    using creatures::config::UARTDevice;
    using creatures::io::Message;
    using creatures::io::MessageRouter;

    ServoModuleHandler::ServoModuleHandler(std::shared_ptr<Logger> logger,
                                           UARTDevice::module_name moduleId, std::string deviceNode,
                                           std::shared_ptr<MessageRouter> messageRouter) :
                                           logger(logger), deviceNode(std::move(deviceNode)),
                                           moduleId(moduleId), messageRouter(messageRouter) {

        logger->info("creating a new ServoModuleHandler for module {} on node {}",
                     UARTDevice::moduleNameToString(moduleId), this->deviceNode);

        // Make our queues
        this->outgoingQueue = std::make_shared<MessageQueue<Message>>();
        this->incomingQueue = std::make_shared<MessageQueue<Message>>();



    }

    void ServoModuleHandler::init() {
        logger->info("initializing the ServoModuleHandler for module {} on node {}",
                     UARTDevice::moduleNameToString(this->moduleId), this->deviceNode);

        // Make our message processor
        this->messageProcessor = std::make_unique<MessageProcessor>(logger, moduleId, shared_from_this());

        // Create the SerialHandler
        this->serialHandler = std::make_shared<SerialHandler>(logger, this->deviceNode, this->moduleId,
                                                             this->outgoingQueue, this->incomingQueue);

    }

    void ServoModuleHandler::shutdown() {
        logger->info("shutting down the ServoModuleHandler for module {} on node {}",
                     UARTDevice::moduleNameToString(this->moduleId), this->deviceNode);

        // Gracefully shut down the SerialHandler
        logger->debug("shutting down the SerialHandler for module {} on node {}",
                      UARTDevice::moduleNameToString(this->moduleId), this->deviceNode);
        for(const auto& t : this->serialHandler->threads) {
            logger->debug("stopping thread {}", t->getName());
            t->shutdown();
        }

        this->messageProcessor->shutdown();
    }

    void ServoModuleHandler::start() {
        this->messageProcessor->start();
        this->serialHandler->start();
        creatures::StoppableThread::start();
    }

    void ServoModuleHandler::firmwareReadyToOperate() {
        this->ready = true;
    }

    std::shared_ptr<MessageQueue<Message>> ServoModuleHandler::getIncomingQueue() {
        return this->incomingQueue;
    }

    bool ServoModuleHandler::isReady() const {
        return this->ready;
    }

    bool ServoModuleHandler::isConfigured() const {
        return this->configured;
    }


    void ServoModuleHandler::run() {
        logger->info("ServoModuleHandler thread started");
    }

    creatures::config::UARTDevice::module_name ServoModuleHandler::getModuleName() const {
        return this->moduleId;
    }

} // creatures