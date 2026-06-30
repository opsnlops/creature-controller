

#include <cmath>
#include <string>
#include <vector>

#include "controller-config.h"

#include "io/handlers/DynamixelSensorHandler.h"
#include "logging/Logger.h"
#include "server/DynamixelSensorReportMessage.h"
#include "server/ServerMessage.h"
#include "util/MessageQueue.h"
#include "util/string_utils.h"
#include "watchdog/WatchdogGlobals.h"

/*
 * DSENSE message format from firmware:
 *   DSENSE\tD1 45 128 7400 2048\tD2 43 -50 7350 1024
 *
 * Each token after DSENSE: D<id> <temperature_F> <present_load> <voltage_mV> <present_position>
 *
 * The trailing <present_position> field was added later. Tokens without it
 * (older firmware) are still accepted and simply omit the position.
 */

namespace creatures {

DynamixelSensorHandler::DynamixelSensorHandler(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue)
    : websocketOutgoingQueue(websocketOutgoingQueue), logger(logger) {
    logger->info("DynamixelSensorHandler created");
}

void DynamixelSensorHandler::handle(std::shared_ptr<Logger> handleLogger, const std::vector<std::string> &tokens) {

    handleLogger->debug("received Dynamixel sensor report");

    if (tokens.size() < 2) {
        handleLogger->warn("DSENSE message has no motor tokens");
        return;
    }

    json payloadJson;
    double maxTemp = 0.0;
    double maxAbsLoad = 0.0;

    // Process each motor token (skip token[0] which is "DSENSE")
    for (size_t i = 1; i < tokens.size(); i++) {
        auto motorReport = tokens[i];
        auto split = splitString(motorReport);

        // 5 fields is current firmware (with position); 4 is older firmware
        // that predates the present_position field.
        const bool hasPosition = (split.size() == 5);
        if (split.size() != 4 && split.size() != 5) {
            handleLogger->warn("expected 4 or 5 fields in DSENSE motor token, got {}: {}", split.size(), motorReport);
            continue;
        }

        // Parse D<id> prefix
        std::string idStr = split[0];
        if (idStr.empty() || idStr[0] != 'D') {
            handleLogger->warn("DSENSE motor token missing D prefix: {}", idStr);
            continue;
        }

        u32 motorId = stringToU32(idStr.substr(1));
        double temperatureF = stringToDouble(split[1]);
        // present_load can be negative, parse as signed via stringToU32 and cast
        int32_t presentLoad = static_cast<int32_t>(stringToU32(split[2]));
        u32 voltageMv = stringToU32(split[3]);
        // present_position is the raw encoder value (0-4095 for XC430-class servos)
        int32_t presentPosition = hasPosition ? static_cast<int32_t>(stringToU32(split[4])) : 0;

        double voltageV = static_cast<double>(voltageMv) / 1000.0;

        // Track max values for watchdog monitoring
        if (temperatureF > maxTemp) {
            maxTemp = temperatureF;
        }
        double absLoad = std::abs(static_cast<double>(presentLoad));
        if (absLoad > maxAbsLoad) {
            maxAbsLoad = absLoad;
        }

        json motorInfo = {
            {"dxl_id", motorId},       {"temperature_f", temperatureF}, {"present_load", presentLoad},
            {"voltage_mv", voltageMv}, {"voltage_v", voltageV},
        };

        // Only report position when the firmware actually sent it
        if (hasPosition) {
            motorInfo["present_position"] = presentPosition;
        }

        payloadJson["dynamixel_motors"].push_back(motorInfo);

        if (hasPosition) {
            handleLogger->debug("Dynamixel {} temp: {:.1f}F, load: {}, voltage: {:.2f}V, position: {}", motorId,
                                temperatureF, presentLoad, voltageV, presentPosition);
        } else {
            handleLogger->debug("Dynamixel {} temp: {:.1f}F, load: {}, voltage: {:.2f}V", motorId, temperatureF,
                                presentLoad, voltageV);
        }
    }

    // Update watchdog globals with max values across all Dynamixel servos
    creatures::watchdog::WatchdogGlobals::updateDynamixelTemperature(maxTemp);
    creatures::watchdog::WatchdogGlobals::updateDynamixelLoad(maxAbsLoad);

    // Send to websocket
    auto message = creatures::server::DynamixelSensorReportMessage(logger, payloadJson);
    websocketOutgoingQueue->push(message);
}

} // namespace creatures
