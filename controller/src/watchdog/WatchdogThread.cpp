#include "watchdog/WatchdogThread.h"
#include "controller/commands/EmergencyStop.h"
#include "io/Message.h"
#include "server/EstopMessage.h"
#include "server/WatchdogWarningMessage.h"
#include "watchdog/WatchdogGlobals.h"
#include <chrono>
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::json;

namespace creatures::watchdog {

WatchdogThread::WatchdogThread(std::shared_ptr<Logger> logger, std::shared_ptr<creatures::config::Configuration> config,
                               std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue,
                               std::shared_ptr<creatures::io::MessageRouter> messageRouter)
    : logger(logger), config(config), websocketOutgoingQueue(websocketOutgoingQueue), messageRouter(messageRouter) {
    threadName = "WatchdogThread";
    logger->info("WatchdogThread created");
}

void WatchdogThread::run() {
    logger->info("WatchdogThread starting monitoring loop");
    logger->info("Configuration: PowerLimit={:.2f}W, PowerWarning={:.2f}W, PowerResponse={:.2f}s",
                 config->getPowerDrawLimitWatts(), config->getPowerDrawWarningWatts(),
                 config->getPowerDrawResponseSeconds());
    logger->info("Configuration: TempLimit={:.2f}F, TempWarning={:.2f}F, TempResponse={:.2f}s",
                 config->getTemperatureLimitDegrees(), config->getTemperatureWarningDegrees(),
                 config->getTemperatureLimitSeconds());
    logger->info("Configuration: DxlTempLimit={:.2f}F, DxlTempWarning={:.2f}F, DxlTempResponse={:.2f}s",
                 config->getDynamixelTemperatureLimitDegrees(), config->getDynamixelTemperatureWarningDegrees(),
                 config->getDynamixelTemperatureLimitSeconds());
    logger->info("Configuration: DxlLoadLimit={:.2f}%, DxlLoadWarning={:.2f}%, DxlLoadResponse={:.2f}s",
                 config->getDynamixelLoadLimitPercent(), config->getDynamixelLoadWarningPercent(),
                 config->getDynamixelLoadLimitSeconds());

    while (!stop_requested.load()) {
        checkPowerDraw();
        checkTemperature();
        checkDynamixelTemperature();
        checkDynamixelLoad();

        // Sleep for 100ms before next check
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    logger->info("WatchdogThread stopping");
}

void WatchdogThread::checkPowerDraw() {
    double currentPower = WatchdogGlobals::getPowerDraw();
    double powerLimit = config->getPowerDrawLimitWatts();
    double powerWarning = config->getPowerDrawWarningWatts();
    double responseTimeSeconds = config->getPowerDrawResponseSeconds();

    auto now = std::chrono::steady_clock::now();

    if (currentPower >= powerLimit) {
        if (!powerLimitCurrentlyExceeded) {
            // Just exceeded the limit
            powerLimitCurrentlyExceeded = true;
            powerLimitExceededTime = now;
            logger->warn("Power draw limit exceeded: {:.2f}W >= {:.2f}W limit", currentPower, powerLimit);
        } else {
            // Check if we've been over the limit long enough to trigger ESTOP
            auto timeOverLimit = std::chrono::duration_cast<std::chrono::seconds>(now - powerLimitExceededTime).count();
            if (timeOverLimit >= responseTimeSeconds) {
                triggerEmergencyStop("Power draw limit exceeded for too long");
                return;
            }
        }
    } else {
        // Reset limit exceeded state
        if (powerLimitCurrentlyExceeded) {
            logger->info("Power draw returned to safe levels: {:.2f}W", currentPower);
            powerLimitCurrentlyExceeded = false;
            powerWarningLogged = false;
        }
    }

    // Check warning threshold
    if (currentPower >= powerWarning && !powerWarningLogged) {
        logger->warn("Power draw warning: {:.2f}W >= {:.2f}W warning threshold", currentPower, powerWarning);
        sendWarningToServer("power_draw_warning", currentPower, powerWarning);
        powerWarningLogged = true;
    } else if (currentPower < powerWarning && powerWarningLogged) {
        powerWarningLogged = false;
    }
}

void WatchdogThread::checkTemperature() {
    double currentTemp = WatchdogGlobals::getTemperature();
    double tempLimit = config->getTemperatureLimitDegrees();
    double tempWarning = config->getTemperatureWarningDegrees();
    double responseTimeSeconds = config->getTemperatureLimitSeconds();

    auto now = std::chrono::steady_clock::now();

    if (currentTemp >= tempLimit) {
        if (!temperatureLimitCurrentlyExceeded) {
            // Just exceeded the limit
            temperatureLimitCurrentlyExceeded = true;
            temperatureLimitExceededTime = now;
            logger->warn("Temperature limit exceeded: {:.2f}F >= {:.2f}F limit", currentTemp, tempLimit);
            logger->warn("Emergency stop will trigger in {:.2f} seconds if temperature remains high",
                         responseTimeSeconds);
        } else {
            // Check if we've been over the limit long enough to trigger ESTOP
            auto timeOverLimit =
                std::chrono::duration_cast<std::chrono::seconds>(now - temperatureLimitExceededTime).count();
            logger->warn("Temperature still over limit: {:.2f}F, time over limit: {}s/{:.2f}s", currentTemp,
                         timeOverLimit, responseTimeSeconds);
            if (timeOverLimit >= responseTimeSeconds) {
                triggerEmergencyStop("Temperature limit exceeded for too long");
                return;
            }
        }
    } else {
        // Reset limit exceeded state
        if (temperatureLimitCurrentlyExceeded) {
            logger->info("Temperature returned to safe levels: {:.2f}F", currentTemp);
            temperatureLimitCurrentlyExceeded = false;
            temperatureWarningLogged = false;
        }
    }

    // Check warning threshold
    if (currentTemp >= tempWarning && !temperatureWarningLogged) {
        logger->warn("Temperature warning: {:.2f}F >= {:.2f}F warning threshold", currentTemp, tempWarning);
        sendWarningToServer("temperature_warning", currentTemp, tempWarning);
        temperatureWarningLogged = true;
    } else if (currentTemp < tempWarning && temperatureWarningLogged) {
        temperatureWarningLogged = false;
    }
}

void WatchdogThread::checkDynamixelTemperature() {
    double currentTemp = WatchdogGlobals::getDynamixelTemperature();
    double tempLimit = config->getDynamixelTemperatureLimitDegrees();
    double tempWarning = config->getDynamixelTemperatureWarningDegrees();
    double responseTimeSeconds = config->getDynamixelTemperatureLimitSeconds();

    auto now = std::chrono::steady_clock::now();

    if (currentTemp >= tempLimit) {
        if (!dynamixelTemperatureLimitCurrentlyExceeded) {
            dynamixelTemperatureLimitCurrentlyExceeded = true;
            dynamixelTemperatureLimitExceededTime = now;
            logger->warn("Dynamixel temperature limit exceeded: {:.2f}F >= {:.2f}F limit", currentTemp, tempLimit);
            logger->warn("Emergency stop will trigger in {:.2f} seconds if Dynamixel temperature remains high",
                         responseTimeSeconds);
        } else {
            auto timeOverLimit =
                std::chrono::duration_cast<std::chrono::seconds>(now - dynamixelTemperatureLimitExceededTime).count();
            logger->warn("Dynamixel temperature still over limit: {:.2f}F, time over limit: {}s/{:.2f}s", currentTemp,
                         timeOverLimit, responseTimeSeconds);
            if (timeOverLimit >= responseTimeSeconds) {
                triggerEmergencyStop("Dynamixel temperature limit exceeded for too long");
                return;
            }
        }
    } else {
        if (dynamixelTemperatureLimitCurrentlyExceeded) {
            logger->info("Dynamixel temperature returned to safe levels: {:.2f}F", currentTemp);
            dynamixelTemperatureLimitCurrentlyExceeded = false;
            dynamixelTemperatureWarningLogged = false;
        }
    }

    if (currentTemp >= tempWarning && !dynamixelTemperatureWarningLogged) {
        logger->warn("Dynamixel temperature warning: {:.2f}F >= {:.2f}F warning threshold", currentTemp, tempWarning);
        sendWarningToServer("dynamixel_temperature_warning", currentTemp, tempWarning);
        dynamixelTemperatureWarningLogged = true;
    } else if (currentTemp < tempWarning && dynamixelTemperatureWarningLogged) {
        dynamixelTemperatureWarningLogged = false;
    }
}

void WatchdogThread::checkDynamixelLoad() {
    double currentLoad = WatchdogGlobals::getDynamixelLoad();
    // Convert from 0.1% units to percent (1000 = 100%)
    double currentLoadPercent = currentLoad / 10.0;
    double loadLimit = config->getDynamixelLoadLimitPercent();
    double loadWarning = config->getDynamixelLoadWarningPercent();
    double responseTimeSeconds = config->getDynamixelLoadLimitSeconds();

    auto now = std::chrono::steady_clock::now();

    if (currentLoadPercent >= loadLimit) {
        if (!dynamixelLoadLimitCurrentlyExceeded) {
            dynamixelLoadLimitCurrentlyExceeded = true;
            dynamixelLoadLimitExceededTime = now;
            logger->warn("Dynamixel load limit exceeded: {:.1f}% >= {:.1f}% limit", currentLoadPercent, loadLimit);
            logger->warn("Emergency stop will trigger in {:.2f} seconds if Dynamixel load remains high",
                         responseTimeSeconds);
        } else {
            auto timeOverLimit =
                std::chrono::duration_cast<std::chrono::seconds>(now - dynamixelLoadLimitExceededTime).count();
            logger->warn("Dynamixel load still over limit: {:.1f}%, time over limit: {}s/{:.2f}s", currentLoadPercent,
                         timeOverLimit, responseTimeSeconds);
            if (timeOverLimit >= responseTimeSeconds) {
                triggerEmergencyStop("Dynamixel load limit exceeded for too long");
                return;
            }
        }
    } else {
        if (dynamixelLoadLimitCurrentlyExceeded) {
            logger->info("Dynamixel load returned to safe levels: {:.1f}%", currentLoadPercent);
            dynamixelLoadLimitCurrentlyExceeded = false;
            dynamixelLoadWarningLogged = false;
        }
    }

    if (currentLoadPercent >= loadWarning && !dynamixelLoadWarningLogged) {
        logger->warn("Dynamixel load warning: {:.1f}% >= {:.1f}% warning threshold", currentLoadPercent, loadWarning);
        sendWarningToServer("dynamixel_load_warning", currentLoadPercent, loadWarning);
        dynamixelLoadWarningLogged = true;
    } else if (currentLoadPercent < loadWarning && dynamixelLoadWarningLogged) {
        dynamixelLoadWarningLogged = false;
    }
}

void WatchdogThread::sendWarningToServer(const std::string &warningType, double currentValue, double threshold) {
    if (!websocketOutgoingQueue) {
        return;
    }

    json warningJson = {{"warning_type", warningType},
                        {"current_value", currentValue},
                        {"threshold", threshold},
                        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count()}};

    // Create a server message for the warning
    auto warningMessage = creatures::server::WatchdogWarningMessage(logger, warningJson);

    websocketOutgoingQueue->push(warningMessage);
    logger->debug("Sent watchdog warning to server: {}", warningType);
}

void WatchdogThread::triggerEmergencyStop(const std::string &reason) {
    logger->critical("EMERGENCY STOP TRIGGERED: {}", reason);

    // Send ESTOP to server
    if (websocketOutgoingQueue) {
        json estopJson = {{"reason", reason},
                          {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                                            std::chrono::system_clock::now().time_since_epoch())
                                            .count()}};

        auto estopMessage = creatures::server::EstopMessage(logger, estopJson);
        websocketOutgoingQueue->push(estopMessage);
    }

    // Send ESTOP command to all firmware modules
    if (messageRouter) {
        auto estopCommand = std::make_shared<creatures::commands::EmergencyStop>(logger);
        auto moduleIds = messageRouter->getHandleIds();

        logger->critical("Sending ESTOP command to {} firmware modules", moduleIds.size());

        for (const auto &moduleId : moduleIds) {
            creatures::io::Message estopMessage(moduleId, estopCommand->toMessageWithChecksum());
            auto result = messageRouter->sendMessageToCreature(estopMessage);
            if (!result.isSuccess()) {
                auto error = result.getError();
                if (error.has_value()) {
                    logger->error("Failed to send ESTOP to module {}: {}",
                                  creatures::config::UARTDevice::moduleNameToString(moduleId), error->getMessage());
                } else {
                    logger->error("Failed to send ESTOP to module {} (unknown error)",
                                  creatures::config::UARTDevice::moduleNameToString(moduleId));
                }
            } else {
                logger->critical("ESTOP sent to module {}",
                                 creatures::config::UARTDevice::moduleNameToString(moduleId));
            }
        }
    }

    // Stop the watchdog thread
    stop_requested.store(true);
}

} // namespace creatures::watchdog