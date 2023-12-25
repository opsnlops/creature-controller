
#pragma once

#include <string>

#include "controller-config.h"

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
         * @param outputPosition which pin the servo is connected to (A0, A1, B2, B3, etc)
         * @param requestedTicks the number of ticks to move the servo to
         */
        ServoPosition(std::string outputPosition, u32 requestedTicks);
        ~ServoPosition() = default;

        std::string getOutputPosition() const;
        u32 getRequestedTicks() const;

        std::string toString() const;

    private:
        std::string outputPosition;
        u32 requestedTicks;
    };

} // creatures
