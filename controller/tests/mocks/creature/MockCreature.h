
#pragma once

#include <string>
#include <unordered_map>

#include <gmock/gmock.h>

#include "controller-config.h"
#include "controller/Input.h"
#include "creature/Creature.h"
#include "util/Result.h"

class MockCreature : public creatures::creature::Creature {
  public:
    using InputMap = std::unordered_map<std::string, creatures::Input>;

    MockCreature(std::shared_ptr<creatures::Logger> logger) : Creature(logger) {}

    MOCK_METHOD(creatures::Result<std::string>, performPreFlightCheck, (), (override));
    MOCK_METHOD(void, applyConfig, (const nlohmann::json &), (override));
    MOCK_METHOD(void, mapInputsToServos, (const InputMap &), (override));
};
