
#pragma once

#include <memory>
#include <optional>

#include "controller-config.h"

#include "Creature.h"
#include "creature/DifferentialHead.h"

#include "device/Servo.h"
#include "logging/Logger.h"
#include "util/Result.h"

class Crow : public creatures::creature::Creature {

  public:
    explicit Crow(const std::shared_ptr<creatures::Logger> &logger);
    ~Crow() = default;

    creatures::Result<std::string> performPreFlightCheck() override;

    void applyConfig(const nlohmann::json &config) override;

    void mapInputsToServos(const std::unordered_map<std::string, creatures::Input> &inputs) override;

  private:
    std::optional<creatures::creature::DifferentialHead> head;
};
