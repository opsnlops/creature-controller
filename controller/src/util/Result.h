#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-pass-by-value"
#pragma once

#include <variant>
#include <optional>
#include <string>
#include <iostream>

namespace creatures {

// Define the ControllerError struct
    class ControllerError {
    public:
        enum ErrorType {
            InvalidData,
            InternalError,
            InvalidConfiguration
        };

        ControllerError(ErrorType errorType, const std::string &message);

        [[nodiscard]] ErrorType getErrorType() const;

        [[nodiscard]] std::string getMessage() const;

    private:
        ErrorType errorType;
        std::string message;
    };

// Define a generic Result type
    template<typename T>
    class Result {
    public:
        // Constructors for success and error
        explicit Result(const T &value);

        explicit Result(const ControllerError &error);

        // Check if the result is a success
        [[nodiscard]] bool isSuccess() const;

        // Get the value (if success)
        std::optional<T> getValue() const;

        // Get the error (if failure)
        [[nodiscard]] std::optional<ControllerError> getError() const;

    private:
        std::variant<T, ControllerError> m_result;
    };

// Implement ControllerError methods
    inline ControllerError::ControllerError(ErrorType errorType, const std::string &message) : errorType(errorType), message(message) {}

    inline ControllerError::ErrorType ControllerError::getErrorType() const {
        return errorType;
    }

    inline std::string ControllerError::getMessage() const {
        return message;
    }


// Implement Result methods
    template<typename T>
    Result<T>::Result(const T &value) : m_result(value) {}

    template<typename T>
    Result<T>::Result(const ControllerError &error) : m_result(error) {}

    template<typename T>
    bool Result<T>::isSuccess() const {
        return std::holds_alternative<T>(m_result);
    }

    template<typename T>
    std::optional<T> Result<T>::getValue() const {
        if (isSuccess()) {
            return std::get<T>(m_result);
        }
        return std::nullopt;
    }

    template<typename T>
    std::optional<ControllerError> Result<T>::getError() const {
        if (!isSuccess()) {
            return std::get<ControllerError>(m_result);
        }
        return std::nullopt;
    }

}
#pragma clang diagnostic pop