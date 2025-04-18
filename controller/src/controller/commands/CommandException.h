
#pragma once

#include <exception>
#include <string>

namespace creatures {

    class CommandException final : public std::exception {
        std::string message;
    public:
        explicit CommandException(const std::string& msg) : message(msg) {}

        const char* what() const noexcept override {
            return message.c_str();
        }
    };


} // creatures
