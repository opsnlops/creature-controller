//
// Created by April White on 11/30/23.
//

#ifndef CREATURE_CONTROLLER_CREATUREBUILDEREXCEPTION_H
#define CREATURE_CONTROLLER_CREATUREBUILDEREXCEPTION_H

#include <exception>
#include <string>

namespace creatures {

    class CreatureBuilderException : public std::exception {
    private:
        std::string message;
    public:
        explicit CreatureBuilderException(const std::string& msg) : message(msg) {}
        virtual const char* what() const noexcept override {
            return message.c_str();
        }
    };


} // creatures

#endif //CREATURE_CONTROLLER_CREATUREBUILDEREXCEPTION_H
