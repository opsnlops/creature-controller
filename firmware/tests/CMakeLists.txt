cmake_minimum_required(VERSION 3.12)
project(creature-firmware-tests C)

set(CMAKE_C_STANDARD 11)

# Download Unity framework
include(FetchContent)
FetchContent_Declare(
        unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG v2.5.2
)
FetchContent_MakeAvailable(unity)

# Add include directories (order matters - mocks directory comes first to override real headers)
include_directories(
        BEFORE  # Add this keyword to ensure these directories come first
        ${CMAKE_CURRENT_SOURCE_DIR}/mocks
        ${unity_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Add mock library for hardware and FreeRTOS
add_library(mocks
        mocks/freertos_mocks.c
        mocks/hardware_mocks.c
        mocks/logging_mocks.c
        mocks/message_handlers_mock.c  # Add this to include our message handler mocks
)

# Define which source files to include in tests
set(FIRMWARE_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/util/string_utils.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/io/responsive_analog_read_filter.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/messaging/messaging.c
)

# Create test executable for string_utils
add_executable(test_string_utils
        tests/test_string_utils.c
        ${FIRMWARE_SOURCES}
)
target_link_libraries(test_string_utils unity mocks)

# Create test executable for responsive_analog_read_filter
add_executable(test_analog_filter
        tests/test_analog_filter.c
        ${FIRMWARE_SOURCES}
)
target_link_libraries(test_analog_filter unity mocks)

# Create test executable for message parsing
add_executable(test_message_parsing
        tests/test_message_parsing.c
        ${FIRMWARE_SOURCES}
)
target_link_libraries(test_message_parsing unity mocks)

# Create test executable for dynamixel protocol
add_executable(test_dynamixel_protocol
        tests/test_dynamixel_protocol.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/dynamixel/dynamixel_protocol.c
)
target_link_libraries(test_dynamixel_protocol unity)

# Create test executable for dynamixel servo layer
# HAL stubs are provided directly in the test file (no real hardware needed)
add_executable(test_dynamixel_servo
        tests/test_dynamixel_servo.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/dynamixel/dynamixel_servo.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/dynamixel/dynamixel_protocol.c
)
target_link_libraries(test_dynamixel_servo unity mocks)

# Add custom test target to run all tests
add_custom_target(run_tests
        COMMAND test_string_utils
        COMMAND test_analog_filter
        COMMAND test_message_parsing
        COMMAND test_dynamixel_protocol
        COMMAND test_dynamixel_servo
        DEPENDS test_string_utils test_analog_filter test_message_parsing test_dynamixel_protocol test_dynamixel_servo
)

# Add exported symbols for mocked functions that are used by the firmware code
add_compile_definitions(
        LOGGING_IMPLEMENTATION_FILE
)

# Suppress the auto-generation of stubs for our message handlers
target_compile_definitions(test_message_parsing PRIVATE
        NO_AUTOSTUB_MESSAGING_HANDLERS=1
)

# Add linker flags to handle undefined symbols for message handler functions
if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-undefined,dynamic_lookup")
else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")
endif()