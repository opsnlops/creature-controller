

#include <string>
#include <vector>

#include "controller-config.h"

#include "logging/Logger.h"
#include "io/handlers/BoardSensorHandler.h"
#include "server/BoardSensorReportMessage.h"
#include "server/ServerMessage.h"
#include "util/MessageQueue.h"
#include "util/string_utils.h"

/*
 * This is the message from the firmware:
 *    0       1         2                   3                       4                   5
 * BSENSE	TEMP %.2f	VBUS %.3f %.3f %.3f	MP_IN %.3f %.3f %.3f	3V3 %.3f %.3f %.3f	5V %.3f %.3f %.3f
 *
 * board_temperature,
                sensor_power_data[VBUS_SENSOR_SLOT].voltage,
                sensor_power_data[VBUS_SENSOR_SLOT].current,
                sensor_power_data[VBUS_SENSOR_SLOT].power,
 */


namespace creatures {

    BoardSensorHandler::BoardSensorHandler(std::shared_ptr<Logger> logger,
                                           std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue) :
            websocketOutgoingQueue(websocketOutgoingQueue), logger(logger) {
        logger->info("BoardSensorHandler created!");
    }


    void BoardSensorHandler::handle(std::shared_ptr<Logger> handleLogger, const std::vector<std::string> &tokens) {

        handleLogger->debug("received board sensor report");

        if (tokens.size() < 6) {
            handleLogger->error("Invalid number of tokens in a board sensor message message: {}", tokens.size());
            return;
        }

        auto temperature = tokens[1];
        double boardTemperature = 0.0;

        std::vector<std::string> split = splitString(temperature);
        if (split.size() != 2) {
            handleLogger->warn("expected two tokens in a temperature sensor report, got: {}", temperature);
            return;
        }
        boardTemperature = stringToDouble(split[1]);
        handleLogger->info("Chassis temperature: {:.2f}F", boardTemperature);

        json payloadJson = {
                {"board_temperature", boardTemperature},
        };

        // This is fine
        for (int i = 2; i < 6; i++) {
            auto sensorReport = tokens[i];
            split = splitString(sensorReport);
            if (split.size() != 4) {
                handleLogger->warn("expected five tokens in a motor report, got: {}", sensorReport);
                continue;
            }

            std::string sensorName;

            if (split[0] == "VBUS") {
                sensorName = "vbus";
            } else if (split[0] == "MP_IN") {
                sensorName = "motor_power_in";
            } else if (split[0] == "3V3") {
                sensorName = "3v3";
            } else if (split[0] == "5V") {
                sensorName = "5v";
            } else {
                handleLogger->warn("Unknown sensor name: {}", split[0]);
                continue;
            }

            auto voltage = stringToDouble(split[1]);
            auto current = stringToDouble(split[2]);
            auto power = stringToDouble(split[3]);

            json sensorInfo = {
                    {"name", sensorName},
                    {"voltage", voltage},
                    {"current", current},
                    {"power", power},
            };

            payloadJson["power_reports"].push_back(sensorInfo);

            handleLogger->info("Sensor {}: voltage: {:.2f}V, current: {:.2f}A, power: {:.2f}W",
                         sensorName, voltage, current, power);
        }

        // Send the message to the websocket
        auto message = creatures::server::BoardSensorReportMessage(logger, payloadJson);
        websocketOutgoingQueue->push(message);
    }

} // creatures