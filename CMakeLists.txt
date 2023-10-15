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
        GIT_TAG 9.1.0
)
FetchContent_MakeAvailable(fmt)
set(FMT_HEADER_ONLY ON)
set(FMT_LOCALE ON)

find_package(BCM2835)

# Our source files
file(GLOB controllerFiles
        src/*
)


include_directories(${BCM2835_INCLUDE_DIRS})

add_executable(creature-controller
        ${controllerFiles}
)

target_link_libraries(creature-controller
        PUBLIC
        fmt::fmt
        ${BCM2835_LIBRARIES}
)

