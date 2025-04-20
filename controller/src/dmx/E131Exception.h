
#pragma once

#include <exception>
#include <string>

namespace creatures::dmx {

    class E131Exception : public std::exception {
    private:
        std::string message;
    public:
        explicit E131Exception(const std::string& msg) : message(msg) {}
        [[nodiscard]] const char* what() const noexcept override {
            return message.c_str();
        }
    };


} // creatures::dmx
