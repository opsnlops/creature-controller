//
// Created by April White on 12/10/23.
//


#pragma once

#include <exception>
#include <string>

namespace creatures {

    class CreatureException : public std::exception {
    private:
        std::string message;
    public:
        explicit CreatureException(const std::string& msg) : message(msg) {}
        virtual const char* what() const noexcept override {
            return message.c_str();
        }
    };


} // creatures
