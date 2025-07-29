cmake_minimum_required(VERSION 3.25)

set(FREERTOS_HEAP "4" CACHE STRING "" FORCE)
set(FREERTOS_PORT "TEMPLATE" CACHE STRING "" FORCE)

# New in SDK 2.1.1, use the fastest supported clock
set(PICO_USE_FASTEST_SUPPORTED_CLOCK 1)

# Which version our code is written for
if (NOT DEFINED HARDWARE_VERSION AND DEFINED ENV{HARDWARE_VERSION})
    set(HARDWARE_VERSION $ENV{HARDWARE_VERSION})
endif()

if (HARDWARE_VERSION STREQUAL "2")
    message("HARDWARE_VERSION is set to ${HARDWARE_VERSION}, setting CC_VER2=1")
    add_compile_definitions(CC_VER2=1)
else()
    message("HARDWARE_VERSION not set or is set to 3, defaulting to 3")
    set(HARDWARE_VERSION 3)
    add_compile_definitions(CC_VER3=1)
endif()




set(PICO_PLATFORM rp2350)
if (NOT PICO_PLATFORM AND NOT DEFINED ENV{PICO_PLATFORM})
    message("PICO_PLATFORM not defined, defaulting to rp2040")
    set(PICO_PLATFORM rp2040)
endif()
add_compile_definitions(PICO_PLATFORM=${PICO_PLATFORM})

# Pull in the Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Make sure we have the right version of the SDK
if (PICO_SDK_VERSION_STRING VERSION_LESS "2.1.1")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 2.1.1 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()


# Pull in FreeRTOS from RPI directly so we get the RP2350 support
if(PICO_PLATFORM STREQUAL "rp2040")
    add_compile_definitions(PICO_PLATFORM_IS_RP2350=0)
    include($ENV{PICO_SDK_PATH}/../FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
else()
    if (PICO_PLATFORM STREQUAL "rp2350-riscv")
        add_compile_definitions(PICO_PLATFORM_IS_RP2350=1)
        include($ENV{PICO_SDK_PATH}/../FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_RISC-V/FreeRTOS_Kernel_import.cmake)
    else()
        add_compile_definitions(PICO_PLATFORM_IS_RP2350=1)
        include($ENV{PICO_SDK_PATH}/../FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake)
    endif()
endif()

# Log which version we're building
message("Building for platform ${PICO_PLATFORM}")

project(creature-firmware
        VERSION "3.3.2"
        DESCRIPTION "Pi Pico Firmwares for April's Creatures"
        HOMEPAGE_URL https://github.com/opsnlops/creature-controller/firmware
        LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)


# Initialize the SDK
pico_sdk_init()

# Add FreeRTOS debugging support for CLion
add_compile_definitions(
        FREERTOS_ENABLE_TRACE=1
        configUSE_TRACE_FACILITY=1
        configRECORD_STACK_HIGH_ADDRESS=1
        configGENERATE_RUN_TIME_STATS=1
)

# Ensure we're using the hardware floating point
set(PICO_DEFAULT_FLOAT_IMPL pico_float_pico)
set(PICO_DEFAULT_DOUBLE_IMPL pico_double_pico)

# Set up common includes and libraries
set(COMMON_LIBS
        hardware_dma
        hardware_i2c
        hardware_gpio
        hardware_pio
        hardware_pwm
        hardware_spi
        hardware_uart
        pico_stdlib
        pico_rand
        pico_unique_id
        pico_multicore
        pico_async_context_freertos
        FreeRTOS-Kernel-Heap4
)

set(COMMON_INCLUDES
        src/
        generated/
        ${CMAKE_BINARY_DIR}/generated/
)

set(COMMON_SOURCES
        src/types.h
        src/debug/blinker.h
        src/debug/blinker.c
        src/debug/remote_logging.h
        src/debug/remote_logging.c
        src/debug/sensor_reporter.h
        src/debug/sensor_reporter.c
        src/debug/stats_reporter.h
        src/debug/stats_reporter.c
        src/device/colors.h
        src/device/colors.c
        src/device/eeprom.h
        src/device/eeprom.c
        src/device/mcp3304.h
        src/device/mcp3304.c
        src/device/mcp9808.h
        src/device/mcp9808.c
        src/device/pac1954.h
        src/device/pac1954.c
        src/device/power_control.h
        src/device/power_control.c
        src/device/status_lights.h
        src/device/status_lights.c
        src/freertos_hooks.c
        src/io/i2c.h
        src/io/i2c.c
        src/logging/logging.h
        src/logging/logging.c
        src/sensor/sensor.h
        src/sensor/sensor.c
        src/util/ranges.h
        src/util/ranges.c
        src/util/string_utils.h
        src/util/string_utils.c
        src/watchdog/watchdog.h
        src/watchdog/watchdog.c
)


# Configure version.h from version.h.in
configure_file(src/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h)

#
# Per-binary configuration
#

# Controller
set(CONTROLLER_ADDITIONAL_SOURCES

        # The star of the show
        src/controller/main.c
        src/controller/config.h
        src/controller/controller.h
        src/controller/controller.c
        src/controller/logger_hook.c
        src/io/message_processor.h
        src/io/message_processor.c
        src/io/responsive_analog_read_filter.h
        src/io/responsive_analog_read_filter.c
        src/io/spi.h
        src/io/spi.c
        src/io/uart_serial.h
        src/io/uart_serial.c
        src/io/usb_serial.h
        src/io/usb_serial.c
        src/io/usb_workers.c
        src/messaging/messaging.h
        src/messaging/messaging.c
        src/messaging/processors/config_message.h
        src/messaging/processors/config_message.c
        src/messaging/processors/ping_message.h
        src/messaging/processors/ping_message.c
        src/messaging/processors/position_message.h
        src/messaging/processors/position_message.c
        src/usb/usb.h
        src/usb/usb.c
        src/usb/usb_descriptors.c
)

set(CONTROLLER_ADDITIONAL_LIBS
        tinyusb_device
        tinyusb_board
)

set(CONTROLLER_ADDITIONAL_INCLUDES
)

# USBC-PD Test (usbc_pd)
set(USBC_PD_ADDITIONAL_SOURCES
        src/usbc_pd/main.c

        # We need the controller for the status lights only
        src/controller/controller.h
        src/controller/controller.c
        src/usbc_pd/sensortest.h
        src/usbc_pd/sensortest.c
)

set(USBC_PD_ADDITIONAL_LIBS
)

set(USBC_PD_ADDITIONAL_INCLUDES
)


# i2c EEPROM Programmer
set(PROGRAMMER_ADDITIONAL_SOURCES
        src/programmer/main.c
        src/programmer/i2c_programmer.h
        src/programmer/i2c_programmer.c
        src/programmer/shell.h
        src/programmer/shell.c
        src/programmer/usb.h
        src/programmer/usb.c
        src/programmer/usb_descriptors.c
)

set(PROGRAMMER_ADDITIONAL_LIBS
        tinyusb_device
        tinyusb_board
)

set(PROGRAMMER_ADDITIONAL_INCLUDES
)






# Function to create firmware targets
function(create_firmware TARGET_NAME ADDITIONAL_SOURCES ADDITIONAL_LIBS ADDITIONAL_INCLUDES)
    add_executable(${TARGET_NAME}
            ${COMMON_SOURCES}
            ${ADDITIONAL_SOURCES}
            FreeRTOSConfig.h
    )

    pico_generate_pio_header(${TARGET_NAME} ${CMAKE_CURRENT_LIST_DIR}/src/io/uart_rx.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)
    pico_generate_pio_header(${TARGET_NAME} ${CMAKE_CURRENT_LIST_DIR}/src/io/uart_tx.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)
    pico_generate_pio_header(${TARGET_NAME} ${CMAKE_CURRENT_LIST_DIR}/src/device/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

    target_include_directories(${TARGET_NAME} PRIVATE
            ${COMMON_INCLUDES}
            ${ADDITIONAL_INCLUDES}
            ${CMAKE_CURRENT_LIST_DIR}
    )

    target_link_libraries(${TARGET_NAME}
            ${COMMON_LIBS}
            ${ADDITIONAL_LIBS}
    )

    pico_enable_stdio_usb(${TARGET_NAME} 0)
    pico_enable_stdio_uart(${TARGET_NAME} 1)
    pico_add_extra_outputs(${TARGET_NAME})
endfunction()






# Create firmware targets
create_firmware("controller-${PICO_PLATFORM}-hw${HARDWARE_VERSION}"
        "${CONTROLLER_ADDITIONAL_SOURCES}"
        "${CONTROLLER_ADDITIONAL_LIBS}"
        "${CONTROLLER_ADDITIONAL_INCLUDES}"
)

create_firmware("usbc_pd-${PICO_PLATFORM}-hw${HARDWARE_VERSION}"
        "${USBC_PD_ADDITIONAL_SOURCES}"
        "${USBC_PD_ADDITIONAL_LIBS}"
        "${USBC_PD_ADDITIONAL_INCLUDES}"
)

create_firmware("programmer-${PICO_PLATFORM}-hw${HARDWARE_VERSION}"
        "${PROGRAMMER_ADDITIONAL_SOURCES}"
        "${PROGRAMMER_ADDITIONAL_LIBS}"
        "${PROGRAMMER_ADDITIONAL_INCLUDES}"
)
