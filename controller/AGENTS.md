# AGENTS.md

This guide orients any coding agent (including ChatGPT-based assistants) working in this repository.

## Build System and Commands

This is a CMake-based C++20 project with the following common workflows.

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
GoogleTest support is wired in but currently disabled. To enable tests, uncomment lines 316-369 in `CMakeLists.txt`.

### Packaging
```bash
./make_packages.sh  # Builds both Debian packages inside Docker
```
The controller package ships every creature config from the repository's top-level `config/`, and each creature's controller config from `config/controllers/`. A udev rule in `packaging/controller/` starts `creature-controller@NAME.service` from the board's USB product name, and the lowercase name selects both `NAME.json` files.

### Code Formatting
```bash
clang-format -i <file>                # Format a single file
clang-format -i src/**/*.cpp src/**/*.h  # Format the codebase
```
The repository ships with `.clang-format` (LLVM style, 4-space indent, 120-column width). **Always run `clang-format -i` on every changed `.cpp` and `.h` file before committing.** Do not skip this step.

### Code Standards
- Keep comments and logs professional; avoid casual tone or emoji.
- Prefer clear, descriptive names; stay consistent with camelCase variables and PascalCase types.
- Favor self-documenting code, adding brief comments only when the intent is non-obvious.

## Project Architecture

April's Creature Workshop Controller is a multi-threaded C++ service that ingests DMX frames (RS-485 or UDP e1.31), processes them, and streams motor commands to Pi Pico 2040 microcontrollers over USB serial.

### Core Flow
```
Creature Server → DMX → Creature Controller (Linux host) → Pi Pico 2040 → Motors
```

### Key Components

**Configuration System** (`src/config/`)
- `Configuration`: Central configuration container.
- `CreatureBuilder`: Instantiates creatures from JSON.
- `CommandLine`: Command-line parsing via argparse.
- Creature JSON files live in `config/`.

**Controller Layer** (`src/controller/`)
- `Controller`: Main control loop, derives from `StoppableThread`.
- `ServoModuleHandler`: Manages servo module I/O.
- Command pattern drives the Pi Pico G-code-like protocol.

**Creature Abstraction** (`src/creature/`)
- `Creature`: Base class for creature implementations.
- `Parrot`: Example concrete creature.
- Supports servo and stepper motion.

**I/O System** (`src/io/`)
- `MessageRouter`: Dispatch hub between subsystems.
- `SerialHandler`/`SerialReader`/`SerialWriter`: USB serial to Pi Pico.
- Message handlers cover ping, stats, sensors, and more.

**mDNS** (`src/network/`)
- `MdnsRecords`: Builds the creature's `_creature-controller._udp` advertisement (portable, unit tested).
- `AvahiPublisher`: Publishes it through `avahi-daemon`; Linux only, behind `CREATURE_HAS_AVAHI`.

**Server Integration** (`src/server/`)
- `ServerConnection`: WebSocket link to the Creature Server.
- Publishes sensors; consumes remote commands.

**Device Abstraction** (`src/device/`)
- `Servo`: Servo motor interface.
- `Stepper`: Stepper motor control.

## Communication Protocol

The Linux controller talks to Pi Pico 2040 boards using a G-code-inspired USB CDC protocol:
- `POS xxx yyy zzz ...` → set motor positions.
- `LIMIT # <lower> <upper>` → enforce motion limits.

## Dependencies

Fetched via CMake `FetchContent` unless noted otherwise:
- `spdlog` (1.11.0) — logging
- `fmt` (9.1.0) — formatting
- `nlohmann/json` (3.11.3) — JSON parsing
- `argparse` — CLI parsing
- `IXWebSocket` (11.4.5) — WebSocket client
- `uvgRTP` (3.1.6) — RTP audio
- `Opus` (1.5.2) — audio codec
- `GoogleTest` (1.14.0) — unit tests
- `SDL2` / `SDL2_mixer` — audio playback
- `libe131` (local) — E1.31 DMX

## Threading Model

Systems inherit from the `StoppableThread` base class for coordinated shutdown:
- Main controller loop
- Serial reader/writer threads
- Audio subsystem threads
- Server connection
- Message processing workers

`StoppableThread::shutdown()` sets `stop_requested` and then waits for `run()` to return, joining the thread. It waits at most the shutdown timeout (2000 ms by default, `setShutdownTimeout()` to change it). A thread still running after that is logged and detached so a stuck thread can't hang the process; that is the only place a thread is detached, and it means that thread's `run()` loop has a bug. `didShutdownTimeOut()` reports it, and `main` logs it as critical.

Rules for `run()` loops:
- Check `stop_requested` at least every few hundred milliseconds. Never block without a timeout (use `poll()`, `pop_timeout()`, `SO_RCVTIMEO`, and so on).
- For long waits between iterations, use `waitForStop(duration)` instead of `sleep_for()`; it wakes as soon as a stop is requested.
- Every subclass calls `stopThread()` from its destructor. The base destructor also stops and joins as a last resort, but by then the subclass's members are gone and the object is no longer of the subclass's type, so a thread still running then races with destruction (ThreadSanitizer reports it).

## Configuration

Creature definitions are JSON files specifying:
- UART devices for Pi Pico links
- Motor assignments and safety limits
- DMX universe/IP settings
- Creature-specific parameters, including its required power limits

Examples live in `config/`:
- `controller-config.json` — primary controller setup
- `kenny-config.json` — example creature
- `dev-board-config.json` — development harness

## Error Handling

The project favors expressive error handling and logging:
- Custom exceptions (`CreatureException`, `ControllerException`, `SerialException`, etc.)
- `Result<T>` for exception-free error propagation
- Extensive structured logging

## Recent Threading Updates

The shutdown path has been tightened recently:

**Main Shutdown**
- SIGINT triggers graceful teardown.
- Threads stop in reverse creation order.
- `StoppableThread` uses an atomic `stop_requested`.

**Serial Port Threads**
- `MessageQueue` supports `request_shutdown()`.
- Serial threads rely on timeout-based loops.
- `std::exit()` replaced with recoverable errors.
- Serial failures return `Result<bool>` instead of aborting.
- A failed serial port, stalled write, or pong timeout makes `main` shut down and exit non-zero so the service manager restarts it.
- After an emergency stop the firmware goes silent by design, so these failures are ignored and frames stop. Never restart past an ESTOP.

**Message Passing**
- `MessageProcessor` / `MessageRouter` use `pop_timeout(100ms)`.
- Coordinated shutdown across message handlers.
- Exceptions are caught without killing the process.

**RTP Audio**
- Multi-threaded audio with coordinated shutdown.
- Non-blocking sockets (1 ms timeouts).
- Monitoring thread responds within ~100 ms.
- Cleans up SDL, Opus, and network resources.

**Shutdown Timeout Tweaks**
- `StoppableThread::shutdown()` joins its thread, waiting up to 2000 ms before detaching it and logging.
- `E131Client` uses a 200 ms receive timeout so it notices shutdown when no DMX is arriving.
- `SerialHandler` leaves the port open if a serial thread had to be abandoned, so its descriptor can't be reused under it.
- `ServoModuleHandler` now shuts down serial paths before message processors.
- Enhanced logging for shutdown progress.
- Fixed race conditions where processing continued post-shutdown request.

## Future Work / Known TODOs

**TLS Support for Server Communication**
- Add TLS/SSL support for WebSocket connections (`ServerConnection`)
- Add TLS/SSL support for RESTful API calls (creature registration endpoint)
- Requires adding a configuration option for enabling TLS
- Currently both use unencrypted HTTP/WS protocols
- This is a larger architectural change that should be done holistically

