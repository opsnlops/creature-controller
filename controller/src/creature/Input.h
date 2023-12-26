
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
        Input(std::string name, u16 slot, u8 width);
        ~Input() = default;


        std::string getName() const;
        u16 getSlot() const;
        u8 getWidth() const;

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
    };

} // creatures
