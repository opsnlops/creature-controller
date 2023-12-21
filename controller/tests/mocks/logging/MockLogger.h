
#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "fmt/format.h"

#include "logging/Logger.h"

namespace creatures {


    class MockLogger : public creatures::Logger {
    public:
        // Mock the protected virtual methods
        MOCK_METHOD(void, logTrace,(const std::string &format, fmt::format_args args), (override)

        );

        MOCK_METHOD(void, logDebug,(const std::string &format, fmt::format_args args), (override)

        );

        MOCK_METHOD(void, logInfo,(const std::string &format, fmt::format_args args), (override)

        );

        MOCK_METHOD(void, logWarning,(const std::string &format, fmt::format_args args), (override)

        );

        MOCK_METHOD(void, logError,(const std::string &format, fmt::format_args args), (override)

        );

        MOCK_METHOD(void, logCritical,(const std::string &format, fmt::format_args args), (override)

        );

        // Override init() with an empty implementation if necessary
        void init(std::string loggerName) override {
            // No initialization required for the mock
        }
    };

    class NiceMockLogger : public ::testing::NiceMock<MockLogger> {
        // Inherits everything from MockLogger but ignores uninteresting calls
    };
}