
#pragma once

#include <string>

#include "device/ServoSpecifier.h"

namespace creatures {

    /**
     *
     * A simple class for the controller to accept a list of requested moves from a creature
     *
     * @class ServoPosition
     */
    class ServoPosition {

    public:

        /**
         * Constructor!
         *
         * @param servoId the servo to move
         * @param requestedTicks the number of ticks to move the servo to
         */
        ServoPosition(ServoSpecifier servoId, u32 requestedTicks);
        ~ServoPosition() = default;

        [[nodiscard]] ServoSpecifier getServoId() const;
        [[nodiscard]] u32 getRequestedTicks() const;

        [[nodiscard]] std::string toString() const;

    private:
        ServoSpecifier servoId;
        u32 requestedTicks;
    };

} // creatures
