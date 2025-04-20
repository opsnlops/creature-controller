#pragma once

#include <string>

#include "device/ServoSpecifier.h"

namespace creatures {

    /**
     * @brief Represents a command to move a specific servo to a target position.
     *
     * This class encapsulates the identification of a servo and the requested position (in ticks)
     * that the controller should move it to. It is used by the creature controller to store
     * and process movement requests.
     */
    class ServoPosition {

    public:

        /**
         * @brief Constructs a ServoPosition object.
         * @param servoId The identifier of the servo to be moved.
         * @param requestedTicks The target position for the servo in ticks.
         */
        ServoPosition(ServoSpecifier servoId, u32 requestedTicks);

        /**
         * @brief Destructor.
         */
        ~ServoPosition() = default;

        /**
         * @brief Retrieves the ID of the servo to move.
         * @return The ServoSpecifier for the requested servo.
         */
        [[nodiscard]] ServoSpecifier getServoId() const;

        /**
         * @brief Gets the target position for the servo, in ticks.
         * @return The requested number of ticks for the servo's position.
         */
        [[nodiscard]] u32 getRequestedTicks() const;

        /**
         * @brief Generates a human-readable string describing this servo move request.
         * @return A string representation of the servo position object.
         */
        [[nodiscard]] std::string toString() const;

    private:
        /**
         * @brief The identifier of the servo to move.
         */
        ServoSpecifier servoId;

        /**
         * @brief The requested position for the servo, in encoder ticks.
         */
        u32 requestedTicks;
    };

} // creatures