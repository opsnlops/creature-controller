cmake_minimum_required(VERSION 3.18)


project(creature-controller
        VERSION "1.0.0"
        DESCRIPTION "April's Creature Workshop Controller"
        HOMEPAGE_URL https://github.com/opsnlops/creature-controller
        LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")


include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

find_package(PkgConfig REQUIRED)


# fmt
FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt
        GIT_TAG 10.1.1
)
FetchContent_MakeAvailable(fmt)
set(FMT_HEADER_ONLY ON)
set(FMT_LOCALE ON)


# spdlog
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog
        GIT_TAG v1.12.0
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
        URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_MakeAvailable(json)


include_directories(
        src/
)

add_library(creature_lib
        src/config/command-line.cpp
        src/config/config.cpp
        src/config/creature_builder.cpp
        src/controller/controller.cpp
        src/controller/stepper_handler.cpp
        src/creature/creature.cpp
        src/creature/parrot.cpp
        src/device/i2c_servo/i2c_servo.cpp
        src/device/i2c_mock.cpp
        src/device/relay.cpp
        src/device/servo.cpp
        src/device/stepper.cpp
        src/dmx/e131_server.cpp
        src/util/fast_hsv2rgb_32bit.cpp
        src/util/ranges.cpp
        src/device/ServoException.h
        src/creature/CreatureException.h
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
        $<$<PLATFORM_ID:Linux>:i2c>
        fmt::fmt
        argparse
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
        nlohmann_json::nlohmann_json
)

#
# Set up testing
#

enable_testing()

add_executable(creature-controller-test
        tests/creature_builder_test.cpp
        tests/creature_test.cpp
        tests/servo_test.cpp
)

target_link_libraries(creature-controller-test
        creature_lib
        gtest_main
        gmock_main
        nlohmann_json::nlohmann_json
        fmt::fmt
        argparse
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
)

# This is to include Google Test and Google Mock headers
target_include_directories(creature-controller-test PRIVATE
        ${googletest_SOURCE_DIR}/include
        ${googlemock_SOURCE_DIR}/include
)

# Tell CMake where our tests are
set_property(TARGET creature-controller-test PROPERTY FOLDER "tests")

# Register the test with CTest
include(GoogleTest)
gtest_discover_tests(creature-controller-test)
