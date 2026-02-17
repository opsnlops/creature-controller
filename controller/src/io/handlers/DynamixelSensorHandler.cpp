

#include <string>
#include <vector>

#include "controller-config.h"

#include "io/handlers/DynamixelSensorHandler.h"
#include "logging/Logger.h"
#include "server/DynamixelSensorReportMessage.h"
#include "server/ServerMessage.h"
#include "util/MessageQueue.h"
#include "util/string_utils.h"

/*
 * DSENSE message format from firmware:
 *   DSENSE\tD1 45 128 7400\tD2 43 -50 7350
 *
 * Each token after DSENSE: D<id> <temperature_F> <present_load> <voltage_mV>
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

    // Process each motor token (skip token[0] which is "DSENSE")
    for (size_t i = 1; i < tokens.size(); i++) {
        auto motorReport = tokens[i];
        auto split = splitString(motorReport);

        if (split.size() != 4) {
            handleLogger->warn("expected 4 fields in DSENSE motor token, got {}: {}", split.size(), motorReport);
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

        double voltageV = static_cast<double>(voltageMv) / 1000.0;

        json motorInfo = {
            {"dxl_id", motorId},       {"temperature_f", temperatureF}, {"present_load", presentLoad},
            {"voltage_mv", voltageMv}, {"voltage_v", voltageV},
        };

        payloadJson["dynamixel_motors"].push_back(motorInfo);

        handleLogger->info("Dynamixel {} temp: {:.1f}F, load: {}, voltage: {:.2f}V", motorId, temperatureF, presentLoad,
                           voltageV);
    }

    // Send to websocket
    auto message = creatures::server::DynamixelSensorReportMessage(logger, payloadJson);
    websocketOutgoingQueue->push(message);
}

} // namespace creatures
