

#include <string>
#include <vector>

#include "controller-config.h"

#include "logging/Logger.h"
#include "io/handlers/SensorHandler.h"
#include "server/SensorReportMessage.h"
#include "server/ServerMessage.h"
#include "util/MessageQueue.h"
#include "util/string_utils.h"

/*
 * This is the message from the firmware:
 *    0       1          2                       3                      4                        5
 * "SENSOR\tTEMP %.2f\tM0 %u %.2f %.2f %.2f\tM1 %u %.2f %.2f %.2f\tM2 %u %.2f %.2f %.2f\tM3 %u %.2f %.2f %.2f"
 *
 * voltage, current, power
 */


namespace creatures {

    SensorHandler::SensorHandler(std::shared_ptr<Logger> logger,
                                 std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue) :
            websocketOutgoingQueue(websocketOutgoingQueue), logger(logger) {
        logger->info("SensorHandler created!");
    }


    void SensorHandler::handle(std::shared_ptr<Logger> handleLogger, const std::vector<std::string> &tokens) {

        handleLogger->debug("received sensor report");

        if (tokens.size() < 5) {
            handleLogger->error("Invalid number of tokens in a sensor message message: {}", tokens.size());
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


        // Surely this will not bite me in the butt
        for (int i = 2; i < 6; i++) {
            auto motorReport = tokens[i];
            split = splitString(motorReport);
            if (split.size() != 5) {
                handleLogger->warn("expected five tokens in a motor report, got: {}", motorReport);
                continue;
            }

            auto motorPosition = stringToU32(split[1]);
            auto motorVoltage = stringToDouble(split[2]);
            auto motorCurrent = stringToDouble(split[3]);
            auto motorPower = stringToDouble(split[4]);


            json motorInfo = {
                    {"number", i - 2},
                    {"position", motorPosition},
                    {"voltage", motorVoltage},
                    {"current", motorCurrent},
                    {"power", motorPower},
            };

            payloadJson["motors"].push_back(motorInfo);

            handleLogger->info("Motor {} position: {}, voltage: {:.2f}V, current: {:.2f}A, power: {:.2f}W",
                         i - 2, motorPosition,
                         motorVoltage, motorCurrent, motorPower);

        }

        // Send the message to the websocket
        auto message = creatures::server::SensorReportMessage(logger, payloadJson);
        websocketOutgoingQueue->push(message);
    }

} // creatures