cmake_minimum_required(VERSION 3.18)


project(creature-controller
        VERSION "1.0.0"
        DESCRIPTION "April's Creature Workshop Controller"
        HOMEPAGE_URL https://github.com/opsnlops/creature-controller
        LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)


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

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_package(BCM2835)
endif()

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


# Our source files on all platforms
file(GLOB controllerFiles
        src/*
        src/config/*
        src/controller/*
        src/creature/*
        src/device/i2c_mock.cpp
        src/device/i2c_servo/*
        src/device/relay.cpp
        src/device/servo.cpp
        src/device/stepper.cpp
        src/dmx/*
        src/pca9685/*
        src/util/*
)

# Include the non-mock i2c files on Linux only
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    file(GLOB realI2CDeviceFiles
            src/device/i2c_bcm2835.cpp
            src/device/i2c_smbus.cpp
    )
    list(APPEND controllerFiles ${realI2CDeviceFiles})
endif()


include_directories(
        src/
        ${BCM2835_INCLUDE_DIRS}
)

add_executable(creature-controller
        ${controllerFiles}
)

target_link_libraries(creature-controller
        PUBLIC
        libe131
        $<$<PLATFORM_ID:Linux>:i2c>
        fmt::fmt
        argparse
        spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
        ${BCM2835_LIBRARIES}
        nlohmann_json::nlohmann_json
)

