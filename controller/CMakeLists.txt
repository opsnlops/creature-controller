cmake_minimum_required(VERSION 3.18)

project(creature-controller
        VERSION "2.5.6"
        DESCRIPTION "April's Creature Workshop Controller"
        HOMEPAGE_URL https://github.com/opsnlops/creature-controller
        LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Overshadowed declarations keep biting me in the butt 😅
add_compile_options(-Wshadow)
add_compile_options(-Wall)
add_compile_options(-Wextra)
add_compile_options(-Wpedantic)

set(PACKAGE_AUTHOR "April White")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

# Make our version available to the project
configure_file(src/Version.h.in Version.h)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

find_package(PkgConfig REQUIRED)

# fmt
FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt
        GIT_TAG 9.1.0
)
FetchContent_MakeAvailable(fmt)
set(FMT_HEADER_ONLY ON)
set(FMT_LOCALE ON)


# spdlog
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog
        GIT_TAG v1.11.0
)
FetchContent_MakeAvailable(spdlog)

# argparse
FetchContent_Declare(
        argparse
        GIT_REPOSITORY https://github.com/p-ranav/argparse.git
)
FetchContent_MakeAvailable(argparse)


FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# Set up lib131 as an external library
add_library(libe131 STATIC
        lib/libe131/src/e131.c
)

# Not much I can do about warnings in this library
set_source_files_properties(lib/libe131/src/e131.c PROPERTIES COMPILE_FLAGS "-w")


target_include_directories(libe131
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libe131/src/
)

FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)


FetchContent_Declare(
        ixwebsocket
        GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
        GIT_TAG        v11.4.5
)
FetchContent_MakeAvailable(ixwebsocket)



include_directories(
        src/
        PRIVATE ${CMAKE_BINARY_DIR}
)

add_library(creature_lib

        # Configuration Sources
        src/config/CommandLine.cpp
        src/config/CommandLine.h
        src/config/Configuration.cpp
        src/config/Configuration.h
        src/config/CreatureBuilder.cpp
        src/config/CreatureBuilder.h


        # Controller Sources
        src/controller/Controller.cpp
        src/controller/StepperHandler.cpp
        src/controller/commands/ICommand.h
        src/controller/commands/SetServoPositions.cpp
        src/controller/commands/SetServoPositions.h
        src/controller/commands/CommandException.h
        src/controller/commands/Ping.cpp
        src/controller/commands/Ping.h
        src/controller/commands/tokens/ServoPosition.cpp
        src/controller/commands/tokens/ServoPosition.h
        src/controller/commands/tokens/ServoConfig.cpp
        src/controller/commands/tokens/ServoConfig.h
        src/controller/commands/ServoModuleConfiguration.cpp
        src/controller/commands/ServoModuleConfiguration.h
        src/controller/commands/FlushBuffer.cpp
        src/controller/commands/FlushBuffer.h
        src/controller/tasks/PingTask.cpp
        src/controller/tasks/PingTask.h
        src/controller/Input.cpp
        src/controller/Input.h
        src/controller/ControllerException.h
        src/controller/CommandSendException.h

        # Creature Sources
        src/creature/Creature.cpp
        src/creature/Parrot.cpp
        src/creature/CreatureException.h

        # Device Sources
        src/device/Servo.cpp
        src/device/Stepper.cpp
        src/device/GPIO.cpp
        src/device/GPIO.h
        src/device/ServoException.h
        src/device/GPIOException.h

        # DMX Sources
        src/dmx/E131Client.cpp
        src/dmx/E131Exception.h

        # IO Sources
        src/io/SerialHandler.cpp
        src/io/SerialHandler.h
        src/io/SerialException.h
        src/io/MessageProcessor.cpp
        src/io/MessageProcessor.h
        src/io/handlers/IMessageHandler.h
        src/io/handlers/StatsHandler.cpp
        src/io/handlers/StatsHandler.h
        src/io/handlers/LogHandler.cpp
        src/io/handlers/LogHandler.h
        src/io/handlers/StatsMessage.cpp
        src/io/handlers/StatsMessage.h
        src/io/handlers/PongHandler.cpp
        src/io/handlers/PongHandler.h
        src/io/handlers/InitHandler.cpp
        src/io/handlers/InitHandler.h
        src/io/handlers/ReadyHandler.cpp
        src/io/handlers/ReadyHandler.h

        # Logging Sources
        src/logging/Logger.h
        src/logging/SpdlogLogger.h
        src/logging/LoggingException.h

        # Utility Sources
        src/util/fast_hsv2rgb_32bit.cpp
        src/util/ranges.cpp
        src/util/MessageQueue.h
        src/util/thread_name.cpp
        src/util/thread_name.h
        src/util/string_utils.cpp
        src/util/string_utils.h
        src/util/StoppableThread.h
        src/io/SerialReader.h
        src/io/SerialReader.cpp
        src/io/SerialWriter.h
        src/io/SerialWriter.cpp
        src/util/CountingThread.h
        src/config/UARTDevice.h
        src/config/UARTDevice.cpp
        src/config/BaseBuilder.h
        src/config/BaseBuilder.cpp
        src/config/BuilderException.h
        src/config/ConfigurationBuilder.h
        src/config/ConfigurationBuilder.cpp
        src/io/Message.h
        src/io/MessageRouter.h
        src/io/UnknownMessageDestintationException.h
        src/io/MessageRouter.cpp
        src/controller/ServoModuleHandler.h
        src/controller/ServoModuleHandler.cpp
        src/device/ServoSpecifier.h
        src/util/Result.h

        # Creature Server Connection
        src/server/ServerConnection.h
        src/server/ServerConnection.cpp
        src/io/handlers/BoardSensorHandler.h
        src/io/handlers/BoardSensorHandler.cpp
        src/io/handlers/MotorSensorHandler.h
        src/io/handlers/MotorSensorHandler.cpp


        src/server/ServerConnection.h
        src/server/ServerConnection.cpp
        src/server/handlers/ServerMessageHandler.h
        src/server/ServerMessageProcessor.h
        src/server/ServerMessage.h
        src/server/ServerMessage.cpp
        src/server/BoardSensorReportMessage.h
        src/server/MotorSensorReportMessage.h
        src/server/WebsocketWriter.h
        src/server/WebsocketWriter.cpp
)

target_include_directories(creature_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${argparse_SOURCE_DIR}/include
        ${fmt_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libe131/src/
        ${nlohmann_json_SOURCE_DIR}/include
        ${spdlog_SOURCE_DIR}/include
        ${ixwebsocket_SOURCE_DIR}
)

add_executable(creature-controller
        src/main.cpp
)

target_link_libraries(creature-controller
        PUBLIC
        creature_lib
        libe131
        fmt::fmt
        argparse
        ixwebsocket
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
        nlohmann_json::nlohmann_json
)

install(TARGETS creature-controller
        COMPONENT creature-controller
        RUNTIME DESTINATION "/bin"
        LIBRARY DESTINATION "/lib"
        DESTINATION "/bin"
)


#
# Set up testing
#

#enable_testing()

#add_executable(creature-controller-test
#        tests/config/CreatureBuilder_test.cpp
#        tests/creature_test.cpp
#        tests/servo_test.cpp
#        tests/MessageQueue_test.cpp
#        tests/SerialHandler_test.cpp
#        tests/MessageProcessor_test.cpp
#        tests/LogHandler_test.cpp
#        tests/mocks/logging/MockLogger.h
#        tests/mocks/io/handlers/MockMessageHandler.cpp
#        tests/mocks/io/handlers/MockMessageHandler.h
#        tests/controller/commands/tokens/ServoPosition_test.cpp
#        tests/controller/Controller_test.cpp
#        tests/mocks/controller/commands/MockCommand.h
#        tests/controller/commands/ICommand_test.cpp
#        tests/controller/commands/SetServoPositions_test.cpp
#        tests/io/handlers/StatsMessage_test.cpp
#        tests/string_utils_test.cpp
#        tests/io/handlers/StatsHandler_test.cpp
#        tests/dmx/E131Server_test.cpp
#        tests/mocks/creature/MockCreature.h
#        tests/creature/Input_test.cpp
#        tests/util/StoppableThread_test.cpp
#        tests/config/UARTDevice_test.cpp
#        tests/config/Configuation_test.cpp
#        tests/io/Message_test.cpp
#)

#target_link_libraries(creature-controller-test
#        creature_lib
#        gtest_main
#        gmock_main
#        libe131
#        nlohmann_json::nlohmann_json
#        fmt::fmt
#        argparse
#        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
#)

# This is to include Google Test and Google Mock headers
#target_include_directories(creature-controller-test PRIVATE
#        ${googletest_SOURCE_DIR}/include
#        ${googlemock_SOURCE_DIR}/include
#        ${CMAKE_CURRENT_SOURCE_DIR}/tests/
#)

# Tell CMake where our tests are
#set_property(TARGET creature-controller-test PROPERTY FOLDER "tests")

# Register the test with CTest
#include(GoogleTest)
#gtest_discover_tests(creature-controller-test)

# where to find our CMake modules
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Package)
