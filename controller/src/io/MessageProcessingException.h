
#pragma once

#include <exception>
#include <string>

namespace creatures {

    class MessageProcessingException : public std::exception {
    private:
        std::string message;
    public:
        explicit MessageProcessingException(const std::string& msg) : message(msg) {}
        virtual const char* what() const noexcept override {
            return message.c_str();
        }
    };

} // creatures


