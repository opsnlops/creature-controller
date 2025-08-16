#pragma once

#include <atomic>
#include <chrono>
#include <memory>

#include "config/Configuration.h"
#include "controller/Controller.h"
#include "io/MessageRouter.h"
#include "logging/Logger.h"
#include "server/ServerMessage.h"
#include "util/MessageQueue.h"
#include "util/StoppableThread.h"

namespace creatures::watchdog {

/**
 * Watchdog thread that monitors temperature and power draw
 * Sends warnings and triggers ESTOP if limits are exceeded
 */
class WatchdogThread : public StoppableThread {
  public:
    WatchdogThread(std::shared_ptr<Logger> logger, std::shared_ptr<creatures::config::Configuration> config,
                   std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue,
                   std::shared_ptr<creatures::io::MessageRouter> messageRouter);

  protected:
    void run() override;

  private:
    std::shared_ptr<Logger> logger;
    std::shared_ptr<creatures::config::Configuration> config;
    std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue;
    std::shared_ptr<creatures::io::MessageRouter> messageRouter;

    // State tracking for exceeded thresholds
    std::chrono::steady_clock::time_point powerLimitExceededTime;
    std::chrono::steady_clock::time_point temperatureLimitExceededTime;
    bool powerLimitCurrentlyExceeded = false;
    bool temperatureLimitCurrentlyExceeded = false;
    bool powerWarningLogged = false;
    bool temperatureWarningLogged = false;

    void checkPowerDraw();
    void checkTemperature();
    void sendWarningToServer(const std::string &warningType, double currentValue, double threshold);
    void triggerEmergencyStop(const std::string &reason);
};

} // namespace creatures::watchdog