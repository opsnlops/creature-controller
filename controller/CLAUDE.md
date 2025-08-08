# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System and Commands

This is a CMake-based C++ project (C++20 standard) with the following common commands:

### Building
```bash
# Debug build
mkdir -p cmake-build-debug && cd cmake-build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Release build  
mkdir -p cmake-build-release && cd cmake-build-release
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Testing
Tests are currently commented out in CMakeLists.txt but the test framework (GoogleTest) is available. To enable tests, uncomment lines 316-369 in CMakeLists.txt.

### Packaging
```bash
./make_package.sh  # Creates Debian package using Docker
```

### Code Formatting
```bash
clang-format -i <file>  # Format a single file
clang-format -i src/**/*.cpp src/**/*.h  # Format all source files
```
The project includes a `.clang-format` file (LLVM style, 4-space indent, 120 column limit). All changed files should be processed with clang-format before committing.

### Code Standards
- Maintain professional comments and log messages - avoid casual language, emoji, or overly playful references
- Use clear, descriptive variable and function names
- Follow existing naming conventions (camelCase for variables, PascalCase for classes)
- Write self-documenting code with minimal but meaningful comments

## Project Architecture

This is April's Creature Workshop Controller - a multi-threaded C++ application that controls animatronic creatures. The system receives DMX frames (via RS-485 or UDP e1.31), processes them, and sends motor control commands to Pi Pico 2040 microcontrollers over USB serial.

### Core Architecture Flow
```
Creature Server → DMX → Creature Controller (Linux host) → Pi Pico 2040 → Motors
```

### Key Components

**Configuration System** (`src/config/`):
- `Configuration` - Main config container
- `CreatureBuilder` - Builds creature objects from JSON config
- `CommandLine` - Argument parsing using argparse
- JSON-based creature configuration files in `config/` directory

**Controller Layer** (`src/controller/`):
- `Controller` - Main control loop, inherits from `StoppableThread`
- `ServoModuleHandler` - Handles servo module communication
- Commands pattern implementation for Pi Pico communication (G-code-like protocol)

**Creature Abstraction** (`src/creature/`):
- `Creature` - Base class for all creature types (parrot, wled_light, skunk, test)
- `Parrot` - Specific creature implementation
- Supports servo and stepper motor types

**I/O System** (`src/io/`):
- `MessageRouter` - Routes messages between components
- `SerialHandler`/`SerialReader`/`SerialWriter` - USB serial communication with Pi Pico
- Message handlers for different protocol messages (ping, stats, sensors, etc.)

**Server Integration** (`src/server/`):
- `ServerConnection` - WebSocket connection to Creature Server
- Handles sensor reporting and server communication

**Device Abstraction** (`src/device/`):
- `Servo` - Servo motor control
- `Stepper` - Stepper motor control  
- `GPIO` - General purpose I/O

### Communication Protocol

The Linux host communicates with Pi Pico 2040 using a G-code-like protocol over USB CDC:
- `POS xxx yyy zzz ...` - Set motor positions
- `LIMIT # <lower> <upper>` - Set motor safety limits

### Dependencies

Key external libraries (fetched via CMake FetchContent):
- **spdlog** (v1.11.0) - Logging
- **fmt** (9.1.0) - String formatting  
- **nlohmann/json** (v3.11.3) - JSON parsing
- **argparse** - Command line parsing
- **IXWebSocket** (v11.4.5) - WebSocket client
- **uvgRTP** (v3.1.6) - RTP audio streaming
- **Opus** (v1.5.2) - Audio codec
- **GoogleTest** (v1.14.0) - Unit testing framework
- **SDL2/SDL2_mixer** - Audio system
- **libe131** (local lib) - E1.31 DMX over Ethernet

### Threading Model

The application uses a multi-threaded design with `StoppableThread` base class:
- Main controller thread
- Serial I/O threads (reader/writer)
- Audio subsystem threads
- Server connection thread
- Message processing threads

### Configuration

Creature configurations are JSON files defining:
- UART device nodes for Pi Pico communication
- Motor configurations and limits  
- DMX universe and IP settings
- Creature-specific parameters

Examples in `config/`:
- `controller-config.json` - Main controller config
- `kenny-config.json` - Specific creature config
- `dev-board-config.json` - Development board config

### Error Handling

The codebase uses custom exceptions extensively:
- `CreatureException`, `ControllerException`, `SerialException`, etc.
- `Result<T>` utility class for error handling without exceptions
- Comprehensive logging throughout the application

### Recent Threading Improvements

The application has been recently updated with comprehensive threading fixes:

**Main Shutdown Process:**
- Graceful shutdown via SIGINT signal handling
- Proper thread cleanup in reverse order of creation
- All threads use `StoppableThread` base class with `stop_requested` atomic flag

**Serial Port Threading:**
- Enhanced `MessageQueue` with shutdown signaling (`request_shutdown()`)
- Serial reader/writer threads use timeout-based operations instead of blocking
- Removed all `std::exit()` calls in favor of graceful error handling
- Serial port errors now return `Result<bool>` instead of crashing

**Message Passing System:**
- `MessageProcessor` and `MessageRouter` use timeout-based `pop_timeout(100ms)`
- Proper shutdown coordination between all message handling threads
- Exception handling without application termination

**RTP Audio Subsystem:**
- Multi-threaded audio processing with proper shutdown coordination
- Non-blocking socket operations with 1ms timeouts
- Responsive monitoring thread with 100ms shutdown responsiveness
- Clean resource cleanup for SDL, Opus decoders, and network sockets

All subsystems now shutdown gracefully within 100-500ms of receiving SIGINT.