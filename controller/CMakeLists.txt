cmake_minimum_required(VERSION 3.18)

project(creature-controller
        VERSION "2.1.0"
        DESCRIPTION "April's Creature Workshop Controller"
        HOMEPAGE_URL https://github.com/opsnlops/creature-controller
        LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

target_include_directories(libe131
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libe131/src/
)

FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)


include_directories(
        src/
        PRIVATE ${CMAKE_BINARY_DIR}
)

add_library(creature_lib
        src/config/CommandLine.cpp
        src/config/Configuration.cpp
        src/config/CreatureBuilder.cpp
        src/controller/Controller.cpp
        src/controller/StepperHandler.cpp
        src/creature/Creature.cpp
        src/creature/Parrot.cpp
        src/device/Servo.cpp
        src/device/Stepper.cpp
        src/dmx/E131Client.cpp
        src/util/fast_hsv2rgb_32bit.cpp
        src/util/ranges.cpp
        src/device/ServoException.h
        src/creature/CreatureException.h
        src/io/SerialHandler.cpp
        src/io/SerialHandler.h
        src/util/MessageQueue.h
        src/io/SerialException.h
        src/util/thread_name.h
        src/util/thread_name.cpp
        src/io/MessageProcessor.cpp
        src/io/MessageProcessor.h
        src/io/handlers/IMessageHandler.h
        src/io/handlers/StatsHandler.cpp
        src/io/handlers/StatsHandler.h
        src/io/handlers/LogHandler.h
        src/io/handlers/LogHandler.cpp
        src/logging/Logger.h
        src/logging/SpdlogLogger.h
        src/logging/LoggingException.h
        src/controller/commands/tokens/ServoPosition.cpp
        src/controller/commands/tokens/ServoPosition.h
        src/controller/commands/ICommand.h
        src/controller/commands/SetServoPositions.cpp
        src/controller/commands/SetServoPositions.h
        src/controller/commands/CommandException.h
        src/controller/CommandSendException.h
        src/io/handlers/StatsMessage.cpp
        src/io/handlers/StatsMessage.h
        src/util/string_utils.h
        src/util/string_utils.cpp
        src/controller/commands/Ping.h
        src/controller/commands/Ping.cpp
        src/controller/tasks/PingTask.cpp
        src/controller/tasks/PingTask.h
        src/io/handlers/PongHandler.h
        src/io/handlers/PongHandler.cpp
        src/dmx/E131Exception.h
        src/controller/Input.cpp
        src/controller/Input.h
        src/io/handlers/InitHandler.cpp
        src/io/handlers/InitHandler.h
        src/controller/ControllerException.h
        src/controller/commands/tokens/ServoConfig.cpp
        src/controller/commands/tokens/ServoConfig.h
        src/controller/commands/CreatureConfiguration.cpp
        src/controller/commands/CreatureConfiguration.h
        src/io/handlers/ReadyHandler.cpp
        src/io/handlers/ReadyHandler.h
        src/device/GPIO.cpp
        src/device/GPIO.h
        src/device/GPIOException.h
        src/controller/commands/FlushBuffer.h
        src/controller/commands/FlushBuffer.cpp
)

target_include_directories(creature_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${argparse_SOURCE_DIR}/include
        ${fmt_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libe131/src/
        ${nlohmann_json_SOURCE_DIR}/include
        ${spdlog_SOURCE_DIR}/include
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

enable_testing()

add_executable(creature-controller-test
        tests/config/CreatureBuilder_test.cpp
        tests/creature_test.cpp
        tests/servo_test.cpp
        tests/MessageQueue_test.cpp
        tests/SerialHandler_test.cpp
        tests/MessageProcessor_test.cpp
        tests/LogHandler_test.cpp
        tests/mocks/logging/MockLogger.h
        tests/mocks/io/handlers/MockMessageHandler.cpp
        tests/mocks/io/handlers/MockMessageHandler.h
        tests/controller/commands/tokens/ServoPosition_test.cpp
        tests/controller/Controller_test.cpp
        tests/mocks/controller/commands/MockCommand.h
        tests/controller/commands/ICommand_test.cpp
        tests/controller/commands/SetServoPositions_test.cpp
        tests/io/handlers/StatsMessage_test.cpp
        tests/string_utils_test.cpp
        tests/io/handlers/StatsHandler_test.cpp
        tests/dmx/E131Server_test.cpp
        tests/mocks/creature/MockCreature.h
        tests/creature/Input_test.cpp
)

target_link_libraries(creature-controller-test
        creature_lib
        gtest_main
        gmock_main
        libe131
        nlohmann_json::nlohmann_json
        fmt::fmt
        argparse
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
)

# This is to include Google Test and Google Mock headers
target_include_directories(creature-controller-test PRIVATE
        ${googletest_SOURCE_DIR}/include
        ${googlemock_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/
)

# Tell CMake where our tests are
set_property(TARGET creature-controller-test PROPERTY FOLDER "tests")

# Register the test with CTest
include(GoogleTest)
gtest_discover_tests(creature-controller-test)

# where to find our CMake modules
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Package)
