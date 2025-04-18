
#pragma once

#include <string>
#include <vector>

#include "device/Servo.h"
#include "logging/Logger.h"

#include "controller-config.h"

namespace creatures {

    class ServoConfig {

    public:

        ServoConfig(const std::shared_ptr<Logger> &logger, const std::shared_ptr<Servo> &servo);
        ~ServoConfig() = default;

        u16 getOutputHeader() const;

        std::string toString() const;

    private:
        std::shared_ptr<Logger> logger;
        std::shared_ptr<Servo> servo;
    };


} // creatures
