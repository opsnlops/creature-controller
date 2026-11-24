#pragma once

#include <memory>

#include "controller-config.h"

#include "Creature.h"

#include "creature/logging/Logger.h"
#include "util/Result.h"

/**
 * A crow. Every joint is its own Dynamixel, so each input goes straight to the motor with the same name. There's no
 * differential head, so no math is needed.
 */
class Crow : public creatures::creature::Creature {

  public:
    explicit Crow(const std::shared_ptr<creatures::Logger> &logger);
    ~Crow() = default;

    creatures::Result<std::string> performPreFlightCheck() override;

    void mapInputsToServos(const std::unordered_map<std::string, creatures::Input> &inputs) override;
};
