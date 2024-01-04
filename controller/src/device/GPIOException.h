
#pragma once

#include <exception>
#include <string>
#include <utility>

namespace creatures::device {

    class GPIOException : public std::exception {
    private:
        std::string message;
    public:
        explicit GPIOException(std::string  msg) : message(std::move(msg)) {}
        virtual const char* what() const noexcept override {
            return message.c_str();
        }
    };


} // creatures::device
