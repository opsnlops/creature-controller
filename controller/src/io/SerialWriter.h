//
// SerialWriter.h
//

#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include "config/UARTDevice.h"
#include "controller-config.h"
#include "creature/logging/Logger.h"
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
    SerialWriter(const std::shared_ptr<Logger> &logger, std::string deviceNode, UARTDevice::module_name moduleName,
                 int fileDescriptor, const std::shared_ptr<MessageQueue<Message>> &outgoingQueue,
                 std::chrono::milliseconds writeTimeout = std::chrono::milliseconds(SERIAL_WRITE_TIMEOUT_MS));

    ~SerialWriter() override { this->logger->info("SerialWriter destroyed"); }

    void start() override;

    /**
     * Did this thread stop because the serial port failed?
     *
     * @return true if the port failed, false if it's running or was asked to stop
     */
    [[nodiscard]] bool hasFailed() const { return failed.load(); }

  protected:
    void run() override;

  private:
    /**
     * Write all of a message, waiting for room in the serial buffer as needed
     *
     * @param payload the bytes to write
     * @return true if every byte was written
     */
    bool writeFully(const std::string &payload);

    /**
     * Record a serial failure, unless we're already stopping
     */
    void markFailed(const std::string &errorMessage);

    std::shared_ptr<Logger> logger;
    std::shared_ptr<MessageQueue<Message>> outgoingQueue;
    std::string deviceNode;
    [[maybe_unused]] UARTDevice::module_name moduleName;
    int fileDescriptor;
    std::chrono::milliseconds writeTimeout;
    std::atomic<bool> failed{false};
};

} // namespace creatures::io