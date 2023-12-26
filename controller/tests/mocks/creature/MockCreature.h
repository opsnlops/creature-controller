
#pragma once

#include <string>
#include <thread>

#include <gmock/gmock.h>



#include "creature/Creature.h"

#include "controller-config.h"

class MockCreature : public Creature {
public:
    // Mocking the constructor
    MockCreature(std::shared_ptr<creatures::Logger> logger) : Creature(logger) {}

    // Mock methods
    MOCK_METHOD(void, init, (std::shared_ptr<Controller>), (override));
    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(std::thread, getWorkerTaskHandle, (), (override));
    MOCK_METHOD(u16, convertInputValueToServoValue, (u8), (override));
    MOCK_METHOD(u8, getNumberOfJoints, (), (const, override));
    MOCK_METHOD(void, addServo, (std::string, std::shared_ptr<Servo>), (override));
    MOCK_METHOD(u8, getNumberOfServos, (), (const, override));
    MOCK_METHOD(void, addStepper, (std::string, std::shared_ptr<Stepper>), (override));
    MOCK_METHOD(u8, getNumberOfSteppers, (), (const, override));
    MOCK_METHOD(std::vector<creatures::ServoPosition>, getRequestedServoPositions, (), (override));
    MOCK_METHOD(void, calculateNextServoPositions, (), (override));
    MOCK_METHOD(const std::string&, getName, (), (const, override));
    MOCK_METHOD(const std::string&, getVersion, (), (const, override));
    MOCK_METHOD(const std::string&, getDescription, (), (const, override));
    MOCK_METHOD(creature_type, getType, (), (const, override));
    MOCK_METHOD(u8, getStartingDmxChannel, (), (const, override));
    MOCK_METHOD(u16, getDmxUniverse, (), (const, override));
    MOCK_METHOD(u16, getPositionMin, (), (const, override));
    MOCK_METHOD(u16, getPositionMax, (), (const, override));
    MOCK_METHOD(u16, getPositionDefault, (), (const, override));
    MOCK_METHOD(float, getHeadOffsetMax, (), (const, override));
    MOCK_METHOD(u16, getServoUpdateFrequencyHz, (), (const, override));
    MOCK_METHOD(std::shared_ptr<Servo>, getServo, (const std::string&), (override));
    MOCK_METHOD(std::shared_ptr<Stepper>, getStepper, (std::string), (override));

    // Mock setters
    MOCK_METHOD(void, setName, (const std::string&), (override));
    MOCK_METHOD(void, setVersion, (const std::string&), (override));
    MOCK_METHOD(void, setDescription, (const std::string&), (override));
    MOCK_METHOD(void, setType, (creature_type), (override));
    MOCK_METHOD(void, setStartingDmxChannel, (u8), (override));
    MOCK_METHOD(void, setDmxUniverse, (u16), (override));
    MOCK_METHOD(void, setPositionMin, (u16), (override));
    MOCK_METHOD(void, setPositionMax, (u16), (override));
    MOCK_METHOD(void, setPositionDefault, (u16), (override));
    MOCK_METHOD(void, setHeadOffsetMax, (float), (override));
    MOCK_METHOD(void, setServoUpdateFrequencyHz, (u16), (override));
};
