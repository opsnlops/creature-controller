# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Context

This is firmware for animatronic controllers used in April's Creature Workshop. This code runs directly on the animatronic (called a "creature") and serves as the low-level hardware interface. 

**This is April's passion project and she is very proud of this codebase.** Please treat it with appropriate care and respect. 

**Important**: This firmware does NOT make autonomous decisions. It receives commands from and reports status to a higher-level "creature-controller" system running on a Linux host (typically a Raspberry Pi). This firmware is purely reactive - it executes commands, monitors sensors, and reports data.

### System Relationship
```
[creature-controller (Linux/RPi)] <-- USB/UART --> [This Firmware (RP2350B)]
         ^                                                    |
         |                                                    v
   [Decision Making]                                    [Hardware Control]
   [Behavior Logic]                                     [Servo Motors]
   [Sequencing]                                         [Sensors]
   [External APIs]                                      [RGB Status LEDs]
```

The creature-controller sends position commands, configuration updates, and requests for sensor data. This firmware responds with servo position updates, sensor readings, and system status.

### Visual Status System (WS2812 RGB LEDs)
The system provides comprehensive visual feedback through RGB LEDs (WS2812):

**System Status Indicators**:
- **Event loop status**: Color-cycling LED indicates the main event loop is progressing normally
- **USB communication**: LED changes color when USB messages are received (primary communication method)
- **UART communication**: LED changes color when UART messages are received (legacy feature, minimal current use)

**Per-Servo Status**: Each servo has its own RGB status LED that indicates the current servo position through color changes

*Note: RGB LEDs are used extensively throughout the system because April enjoys covering everything possible in RGB lighting.*

## Hardware Target

The current version targets a **custom RP2350B-based board** and makes full use of the additional GPIO pins compared to the RP2040. Hardware version 3 (CC_VER3) represents the latest custom board design optimized for the RP2350B's capabilities.

### Hardware Compatibility
- **Custom board**: Primary target is a custom RP2350B design with specialized circuitry for animatronic control
- **Generic board compatibility**: The resulting UF2 files will run on off-the-shelf RP2040/RP2350 development boards but will be missing extended functionality (sensors, dedicated power control, etc.)
- **Generic board setup**: To run on generic boards, set `USE_SENSORS` to `0` in the configuration

## Development Philosophy

This code is designed to be **as close to real-time as possible** on the RP2350. Key principles:

- **Minimize latency**: Servo control timing is critical for smooth animatronic motion
- **Measured changes only**: Make small, highly considered modifications. The system's real-time characteristics depend on careful resource management
- **Think deeply**: It's encouraged to stop and analyze problems thoroughly before making changes
- **FreeRTOS best practices**: The codebase follows FreeRTOS design patterns for deterministic real-time behavior

## Build System

This is a CMake-based project for Raspberry Pi Pico (RP2350/RP2040) firmware development using the Pico SDK and FreeRTOS.

**Language**: This project is written in **pure C** (C11 standard). Do not use C++ features or syntax. Where C++ libraries are needed, custom C wrappers have been written to provide the necessary interfaces.

### Build Commands
- `cmake -B build .` - Configure build (requires PICO_SDK_PATH environment variable)
- `cmake --build build` - Build all firmware targets
- `cmake --build build -j$(nproc)` - Parallel build
- `cmake --build build --target <target-name>` - Build specific target

### Firmware Targets
Four firmware variants share common source files but serve different purposes:

- **`controller-<platform>-hw<version>`** - **Primary firmware** for animatronic servo control (most important)
- `dynamixel-tester-<platform>-hw<version>` - Interactive shell for testing and configuring Dynamixel servos on the bus (ping, read/write registers, sync read/write, baud rate changes)
- `programmer-<platform>-hw<version>` - I2C EEPROM programmer for creating "personality modules" containing creature name, serial number, USB ID, and configuration data
- `usbc_pd-<platform>-hw<version>` - USB-C PD testing application for hardware validation

Platform defaults to `rp2350-arm-s` and hardware version defaults to `3`.

### Environment Variables
- `PICO_SDK_PATH` - Path to Pico SDK (required)
- `HARDWARE_VERSION` - Hardware version (2 or 3, defaults to 3)
- `PICO_PLATFORM` - Target platform (rp2040, rp2350, rp2350-riscv)

## Testing

Unit tests use Unity framework and are located in the `tests/` directory.

### Test Commands
- `cd tests && cmake -B build .` - Configure test build
- `cd tests && cmake --build build` - Build tests
- `cd tests/build && make run_tests` - Run all unit tests

Individual tests can be run directly from `tests/build/`:
- `./test_string_utils`
- `./test_analog_filter`
- `./test_message_parsing`
- `./test_dynamixel_protocol`
- `./test_dynamixel_servo`

## Architecture Overview

This is a **deterministic real-time embedded system** built on FreeRTOS for controlling servo motors and monitoring sensors in animatronic controllers. The system architecture prioritizes predictable timing and minimal latency for servo control.

### Hardware Abstraction
- **Hardware Versions**: Code supports both v2 and v3 hardware via conditional compilation (`CC_VER2`/`CC_VER3`)
- **Platform Support**: RP2040 and RP2350 microcontrollers via Pico SDK abstraction

### Core Subsystems
- **Controller** (`src/controller/`): Main servo/motor control logic and system initialization
- **Dynamixel** (`src/dynamixel/`): Dynamixel Protocol 2.0 servo communication (see below)
- **Communication** (`src/io/`, `src/messaging/`): USB/UART serial, message processing with custom protocol
- **Device Drivers** (`src/device/`): Hardware-specific drivers for sensors (MCP9808, PAC1954, MCP3304), EEPROM, status lights (WS2812)
- **Sensor Management** (`src/sensor/`): I2C/SPI sensor coordination and data collection
- **Logging** (`src/logging/`): Structured logging system with configurable levels

### Dynamixel Servo Subsystem (`src/dynamixel/`)
Three-layer architecture for communicating with Dynamixel XC430-series servos over a half-duplex UART bus:

- **Protocol** (`dynamixel_protocol.c`): Packet construction/parsing, CRC16 calculation, and error code translation. Pure logic with no hardware dependencies — fully unit-testable on the host.
- **HAL** (`dynamixel_hal.c`): Hardware abstraction using PIO-based half-duplex UART with DMA. Handles TX/RX timing, direction switching, and multi-response collection. The multi-response RX path (`dxl_hal_txrx_multi`) uses a hardware alarm + FreeRTOS binary semaphore to yield during the receive window instead of busy-waiting.
- **Servo** (`dynamixel_servo.c`): High-level operations (ping, read/write registers, sync read status, sync write positions). Uses pre-allocated workspace packets from the HAL to avoid heap allocation in hot paths.

Key bus operations for the controller's 50Hz loop:
- **Sync Read** (`dxl_sync_read_status`): Reads position, load, temperature, voltage, and error state from all servos in a single bus transaction
- **Sync Write** (`dxl_sync_write_position`): Sets goal positions for all servos atomically in a single broadcast packet

### Key Design Patterns
- **RTOS Task Architecture**: Each major subsystem runs as FreeRTOS tasks with message queues for inter-task communication
- **Hardware Abstraction**: Configuration files (`config.h`) provide hardware version-specific pin mappings and feature toggles
- **Device Management**: Centralized initialization sequence in `main.c` ensures proper system startup order
- **Message-Based Communication**: Custom protocol for external communication with message processors for different command types

### Configuration System
- Hardware-specific settings in `src/controller/config.h`, `src/programmer/config.h`, and `src/dynamixel_tester/config.h`
- Feature toggles: `USE_EEPROM`, `USE_SENSORS`, `USE_STEPPERS`
- Build-time configuration via CMake definitions and conditional compilation

## Coding Standards

### Data Types
- **Always use types from `src/types.h`**: Use `u8`, `u16`, `u32`, `u64` instead of raw integer types
- **Add new types to `types.h`**: If additional type aliases are needed, add them to `types.h` and reference them consistently
- **Type consistency**: Maintain consistent type usage across all source files

### Constants and Configuration
- **No magic numbers**: All numeric constants must be defined in appropriate `config.h` files
- **Configuration-driven**: Use `#define` macros in config files rather than hardcoding values
- **Shared constants**: Common values used across multiple files should be in the main `config.h`

### Code Organization
- **Shared codebase**: Multiple firmware targets share common source files in `src/`
- **Target-specific code**: Application-specific code is separated into subdirectories (`controller/`, `dynamixel_tester/`, `programmer/`, `usbc_pd/`)
- **Modular design**: Each subsystem is self-contained with clear interfaces

### Comments and Messages
- **Professional comments**: Keep all comments clean and professional
- **Appropriate humor**: Light humor in comments is acceptable, occasional rabbit-related silliness is fine
- **Professional debug messages**: Debug/log messages must always remain professional since they are visible upstream to the creature-controller system
- **No inappropriate content**: Avoid any content that would be unprofessional in a production environment

### Code Formatting
- **Run clang-format**: Always run `clang-format` after making code changes to maintain consistent formatting
- **Exception for main.c**: Do NOT run clang-format on any `main.c` files - clang-format does not handle Pico SDK macros properly (especially picotool binary info macros)
- **Format command**: `clang-format -i <file>` to format in-place

### Communication Protocol
The system implements a custom messaging protocol for servo control and sensor data reporting, handled by message processors in `src/messaging/processors/`.

## FreeRTOS Implementation

This codebase follows FreeRTOS best practices for real-time embedded systems:

### Task Design
- **Single responsibility**: Each task handles one primary function (servo control, sensor monitoring, communication)
- **Appropriate priorities**: Critical real-time tasks (servo control) have higher priorities than monitoring tasks
- **Stack size optimization**: Task stack sizes are carefully tuned to minimize memory usage while preventing overflow

### Inter-Task Communication
- **Message queues**: Used extensively for safe data passing between tasks (see queue configurations in `config.h`)
- **No shared variables**: Tasks communicate through queues and semaphores, avoiding race conditions
- **Blocking operations**: Tasks use blocking queue operations rather than polling to reduce CPU usage

### Memory Management
- **Static allocation preferred**: Where possible, use statically allocated buffers rather than dynamic allocation
- **Heap monitoring**: The system tracks free heap space (`xFreeHeapSpace`) for debugging
- **Buffer sizing**: Communication buffers are sized based on protocol requirements and available memory

### Timing and Synchronization
- **Watchdog integration**: System uses hardware watchdog with FreeRTOS task coordination
- **Deterministic timing**: Critical control loops use consistent timing intervals
- **Resource protection**: Shared resources (I2C, SPI buses) are protected with appropriate synchronization

## Critical Real-Time Code

### Servo Control ISR
**EXTREME CAUTION REQUIRED**: There is a **critical ISR** (Interrupt Service Routine) that services servo control. This ISR:

- **Triggers on PWM counter wrap**: Executes every time the PWM counter wraps (at servo frequency, typically 50Hz)
- **Memory lookup**: Reads the next requested servo states from memory
- **PWM adjustment**: Updates PWM duty cycles for all servo channels
- **Timing critical**: Any delays in this ISR directly impact servo control smoothness and accuracy

**ISR Development Guidelines**:
- **Minimal processing**: Keep ISR code as short and fast as possible
- **No blocking operations**: Never call blocking functions, use queues, or access slow peripherals
- **Atomic operations**: Ensure all memory accesses are atomic or properly protected
- **Testing essential**: Any changes to ISR code must be thoroughly tested for timing impact
- **Profile carefully**: Measure ISR execution time to ensure it completes well before the next PWM wrap
- **Memory access patterns**: Optimize data structures for fastest possible lookup times