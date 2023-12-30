
#pragma once

#include <string>
#include <vector>

#include "device/Servo.h"
#include "logging/Logger.h"

#include "controller-config.h"

namespace creatures {

    class ServoConfig {

    public:

        ServoConfig(std::shared_ptr<Logger> logger, std::shared_ptr<Servo> servo);
        ~ServoConfig() = default;

        std::string getOutputPosition() const;

        std::string toString() const;

    private:
        std::shared_ptr<Logger> logger;
        std::shared_ptr<Servo> servo;
    };


} // creatures
