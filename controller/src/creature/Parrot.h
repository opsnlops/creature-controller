
#pragma once

#include <memory>
#include <optional>

#include "controller-config.h"

#include "Creature.h"
#include "creature/DifferentialHead.h"

#include "creature/logging/Logger.h"
#include "device/Servo.h"
#include "util/Result.h"

class Parrot : public creatures::creature::Creature {

  public:
    explicit Parrot(const std::shared_ptr<creatures::Logger> &logger);
    ~Parrot() = default;

    creatures::Result<std::string> performPreFlightCheck() override;

    void applyConfig(const nlohmann::json &config) override;

    void mapInputsToServos(const std::unordered_map<std::string, creatures::Input> &inputs) override;

  private:
    std::optional<creatures::creature::DifferentialHead> head;
};
