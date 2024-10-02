cmake_minimum_required(VERSION 3.25)

# Select the heap port.  values between 1-4 will pick a heap.
set(FREERTOS_HEAP "4" CACHE STRING "" FORCE)

# Select the native compile PORT
set(FREERTOS_PORT "TEMPLATE" CACHE STRING "" FORCE)

# Pull in the Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Make sure we have the right version of the SDK
if (PICO_SDK_VERSION_STRING VERSION_LESS "2.0.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 2.0.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()


if (NOT PICO_PLATFORM AND NOT DEFINED ENV{PICO_PLATFORM})
    message("PICO_PLATFORM not defined, defaulting to rp2040")
    set(PICO_PLATFORM rp2040)
endif()

# Pull in FreeRTOS from RPI directly so we get the RP2350 support
if(PICO_PLATFORM STREQUAL "rp2040")
    include($ENV{PICO_SDK_PATH}/../FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
else()
    if (PICO_PLATFORM STREQUAL "rp2350-riscv")
        include($ENV{PICO_SDK_PATH}/../FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_RISC-V/FreeRTOS_Kernel_import.cmake)
    else()
        include($ENV{PICO_SDK_PATH}/../FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake)
    endif()
endif()

project(firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)


# Initialize the SDK
pico_sdk_init()

# Ensure we're using the hardware floating point
set(PICO_DEFAULT_FLOAT_IMPL pico_float_pico)
set(PICO_DEFAULT_DOUBLE_IMPL pico_double_pico)


# Set up our includes
include_directories(
        src/
        generated/
)

set(COMMON_LIBS
        hardware_dma
        hardware_i2c
        hardware_gpio
        hardware_pio
        hardware_pwm
        hardware_spi
        hardware_uart
        tinyusb_device
        tinyusb_board
        pico_stdlib
        pico_rand
        pico_unique_id
        pico_multicore
        pico_async_context_freertos
        #FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4
)


set(COMMON_SOURCES
        src/tasks.h
        src/tasks.c
        src/controller/controller.h
        src/controller/controller.c
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
        src/device/mcp3304.h
        src/device/mcp3304.c
        src/device/mcp9808.h
        src/device/mcp9808.c
        src/device/pac1954.h
        src/device/pac1954.c
        src/device/power_relay.h
        src/device/power_relay.c
        src/device/status_lights.h
        src/device/status_lights.c
        src/freertos_hooks.h
        src/freertos_hooks.c
        src/io/i2c.h
        src/io/i2c.c
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
        src/logging/logging.h
        src/logging/logging.c
        src/messaging/messaging.h
        src/messaging/messaging.c
        src/messaging/processors/config_message.h
        src/messaging/processors/config_message.c
        src/messaging/processors/ping_message.h
        src/messaging/processors/ping_message.c
        src/messaging/processors/position_message.h
        src/messaging/processors/position_message.c
        src/sensor/sensor.h
        src/sensor/sensor.c
        src/usb/usb.h
        src/usb/usb.c
        src/usb/usb_descriptors.c
        src/util/ranges.h
        src/util/ranges.c
        src/util/string_utils.h
        src/util/string_utils.c
)


add_executable(firmware
        ${COMMON_SOURCES}
        FreeRTOSConfig.h
        src/main.c
        src/tasks.h
        src/tasks.c
)



# PIO-based UART from the Pico examples
pico_generate_pio_header(firmware ${CMAKE_CURRENT_LIST_DIR}/src/io/uart_rx.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)
pico_generate_pio_header(firmware ${CMAKE_CURRENT_LIST_DIR}/src/io/uart_tx.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

# PIO-base NeoPixel control 😍
pico_generate_pio_header(firmware ${CMAKE_CURRENT_LIST_DIR}/src/device/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

target_include_directories(firmware PRIVATE
        .
        ${CMAKE_CURRENT_LIST_DIR}
)

# pull in common dependencies
target_link_libraries(firmware
        ${COMMON_LIBS}
)

# Enable the shell over UART (which is the picoprobe!)
pico_enable_stdio_usb(firmware 0)
pico_enable_stdio_uart(firmware 1)

# Create all of the files
pico_add_extra_outputs(firmware)






add_executable(usbc_pd
        ${COMMON_SOURCES}
        FreeRTOSConfig.h
        src/usbc_pd/main.c
)


# PIO-base NeoPixel control 😍
pico_generate_pio_header(usbc_pd ${CMAKE_CURRENT_LIST_DIR}/src/device/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

target_include_directories(usbc_pd PRIVATE
        .
        ${CMAKE_CURRENT_LIST_DIR}
)

# pull in common dependencies
target_link_libraries(usbc_pd
        ${COMMON_LIBS}
)

pico_enable_stdio_usb(usbc_pd 0)
pico_enable_stdio_uart(usbc_pd 1)

# Create all of the files
pico_add_extra_outputs(usbc_pd)
