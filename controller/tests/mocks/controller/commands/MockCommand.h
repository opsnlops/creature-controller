
#pragma once

#include <gmock/gmock.h>

#include "controller-config.h"
#include "controller/commands/ICommand.h"

class MockCommand : public creatures::ICommand {
public:
    MOCK_METHOD(std::string, toMessage, (), (override));
    MOCK_METHOD(std::string, toMessageWithChecksum, (), (override));
};
