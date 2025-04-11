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

# Create include directory for mocked headers
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/logging)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/messaging)

# Copy our fixed headers into the build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mocks/stubs.h ${CMAKE_BINARY_DIR}/include/stubs.h COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mocks/logging.h ${CMAKE_BINARY_DIR}/include/logging/logging.h COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mocks/messaging_patched.h ${CMAKE_BINARY_DIR}/include/messaging/messaging.h COPYONLY)

# Add include directories (order matters - build directory first, then mocks, then real code)
include_directories(
        ${CMAKE_BINARY_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/mocks
        ${unity_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Add mock library for hardware and FreeRTOS
add_library(mocks
        mocks/freertos_mocks.c
        mocks/hardware_mocks.c
        mocks/hardware_mocks_globals.c
        mocks/logging_mocks.c
        mocks/message_handlers_mock.c
)

# Define which source files to include in tests
set(FIRMWARE_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/util/string_utils.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/io/responsive_analog_read_filter.c
)

# Create test executable for string_utils
add_executable(test_string_utils
        tests/test_string_utils.c
        ${FIRMWARE_SOURCES}
)
target_link_libraries(test_string_utils unity mocks)
target_compile_definitions(test_string_utils PRIVATE TEST_BUILD)

# Create test executable for responsive_analog_read_filter
add_executable(test_analog_filter
        tests/test_analog_filter.c
        ${FIRMWARE_SOURCES}
)
target_link_libraries(test_analog_filter unity mocks)
target_compile_definitions(test_analog_filter PRIVATE TEST_BUILD)

# Create test executable for message parsing
# Note: we include the messaging.c source directly in this test
add_executable(test_message_parsing
        tests/test_message_parsing.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/messaging/messaging.c
)
target_link_libraries(test_message_parsing unity mocks)
target_compile_definitions(test_message_parsing PRIVATE TEST_BUILD)

# Add custom test target to run all tests
add_custom_target(run_tests
        COMMAND test_string_utils
        COMMAND test_analog_filter
        COMMAND test_message_parsing
        DEPENDS test_string_utils test_analog_filter test_message_parsing
)

# Add linker flags to handle undefined symbols for message handler functions
if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-undefined,dynamic_lookup")
else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")
endif()