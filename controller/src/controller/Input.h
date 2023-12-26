
#pragma once

#include <string>

#include "controller-config.h"

namespace creatures {


    /**
     * This class represents one input off the wire that the Creature desires
     *
     *
     */
    class Input {

    public:
        Input() = default;
        Input(std::string name, u16 slot, u8 width, u32 incomingRequest);
        ~Input() = default;

        // Copy constructor
        Input(const Input& other);

        std::string getName() const;
        u16 getSlot() const;
        u8 getWidth() const;
        u32 getIncomingRequest() const;

        /**
         * Set the value of the incoming request
         *
         * @param incomingRequest
         */
        void setIncomingRequest(u32 incomingRequest);

        std::string toString() const;

    private:

        /*
         * The name of the input. Must be unique.
         */
        std::string name;

        /**
         * The DMX slot the input is on
         */
        u16 slot;

        /**
         * How wide the input is in bytes
         *
         * (This is currently unused)
         */
        u8 width;

        /**
         * The request from the server of where to move this input to
         */
        u32 incomingRequest;
    };

} // creatures
