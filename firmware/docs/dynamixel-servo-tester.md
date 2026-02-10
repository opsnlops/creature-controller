# Dynamixel Servo Tester

A firmware target for testing and configuring Dynamixel XC430 servos using Protocol 2.0. Connect via USB serial to interactively ping, scan, configure, and move servos.

## Hardware Setup

The tester communicates with servos over a single GPIO pin using PIO-based half-duplex UART. The XC430 is 3.3V TTL (same as the RP2350), so no level shifter is needed.

- **Data pin**: GPIO 15 (configurable in `src/dynamixel_tester/config.h`)
- **Default baud rate**: 57,600 bps
- **PIO instance**: pio0 (uses 2 of 4 state machines)

### Wiring

Connect three wires between the RP2350 board and the Dynamixel servo:

| RP2350 | Dynamixel |
|--------|-----------|
| GPIO 15 | DATA |
| GND | GND |
| -- | VIN (external 12V supply) |

The servo needs external 12V power. The data line connects directly to the GPIO pin.

## Building and Flashing

```bash
cmake -B build .
cmake --build build --target dynamixel-tester-rp2350-arm-s-hw3
```

The resulting UF2 file is at `build/dynamixel-tester-rp2350-arm-s-hw3.uf2`. Flash it by holding BOOTSEL and plugging in the board, then copy the UF2 to the mounted drive.

## Connecting

The tester appears as a USB CDC serial device. Connect with any serial terminal (screen, minicom, PuTTY, etc.) at any host baud rate (USB CDC ignores the host baud setting).

```bash
screen /dev/tty.usbmodem* 115200
```

Type `H` and press Enter to see the help menu.

## Commands

### General

| Command | Description |
|---------|-------------|
| `H` | Show help menu (includes current bus baud rate) |
| `I` | System info as JSON (version, free heap, uptime, data pin, baud rate) |
| `R` | Reboot the tester |

### Servo Discovery

| Command | Description |
|---------|-------------|
| `P <id>` | Ping a servo. Returns model number and firmware version |
| `S [start] [end]` | Scan for servos in ID range (default 0-253) |

### Broadcast

Any command that takes an `<id>` accepts **254** as the broadcast ID. Broadcast commands are sent to all servos on the bus simultaneously. Since no servo responds to broadcast, these are transmit-only (no status reply). Useful for turning all LEDs on/off, enabling/disabling torque on all servos at once, etc.

### Servo Configuration

These write to EEPROM registers. **Torque must be off** (`T <id> 0`) before changing EEPROM settings.

| Command | Description |
|---------|-------------|
| `ID <old> <new>` | Change a servo's ID |
| `BR <id> <index>` | Set servo baud rate (see baud rate table below) |
| `FR <id> <option>` | Factory reset: 0=all, 1=keep ID, 2=keep ID+baud |
| `RB <id>` | Reboot servo |

### Motion Control

| Command | Description |
|---------|-------------|
| `T <id> <0\|1>` | Torque disable/enable (must enable before moving) |
| `M <id> <position>` | Move to position (0-4095, center is 2048) |
| `V <id> <velocity>` | Set profile velocity (0 = max speed) |
| `L <id> <0\|1>` | LED off/on |

### Status and Diagnostics

| Command | Description |
|---------|-------------|
| `ST <id>` | Read full status: position, temperature, voltage, load, moving, hardware errors |
| `RR <id> <addr> <len>` | Read any register (len: 1, 2, or 4 bytes) |
| `RW <id> <addr> <len> <val>` | Write any register |

### Register Shortcuts

These read the register when called without a value, or write it when a value is provided.

| Command | Register | Size |
|---------|----------|------|
| `OM <id> [mode]` | Operating mode | 1 byte |
| `PA <id> [accel]` | Profile acceleration | 4 bytes |
| `HO <id> [offset]` | Homing offset | 4 bytes |
| `MN <id> [min]` | Min position limit | 4 bytes |
| `MX <id> [max]` | Max position limit | 4 bytes |

### Bus Configuration

| Command | Description |
|---------|-------------|
| `CB <rate>` | Change the PIO bus baud rate (in bps) |

## Baud Rate Table

Use the index value with the `BR` command to change a servo's baud rate.

| Index | Baud Rate |
|-------|-----------|
| 0 | 9,600 |
| 1 | 57,600 (factory default) |
| 2 | 115,200 |
| 3 | 1,000,000 |
| 4 | 2,000,000 |
| 5 | 3,000,000 |
| 6 | 4,000,000 |
| 7 | 4,500,000 |

Index 3 (1 Mbps) is recommended for production use. Indices 4-7 work but are sensitive to wire length and signal integrity.

## Operating Modes

Use the mode value with the `OM` command.

| Value | Mode | Description |
|-------|------|-------------|
| 0 | Current | Torque (current) control only |
| 1 | Velocity | Continuous rotation |
| 3 | Position | Standard position control (0-4095, default) |
| 4 | Extended Position | Multi-turn position control |
| 5 | Current-based Position | Position control with current limit |
| 16 | PWM | Direct PWM control |

## Common Workflows

### First-time servo setup

```
S                  # Scan to find the servo (factory default ID is 1)
P 1                # Ping to verify communication
T 1 0              # Torque off (required for EEPROM changes)
ID 1 10            # Change ID from 1 to 10
BR 10 3            # Set baud rate to 1Mbps
CB 1000000         # Match the bus baud rate
P 10               # Verify communication at new settings
```

### Moving a servo

```
T 1 1              # Enable torque
V 1 50             # Set profile velocity (lower = slower)
M 1 2048           # Move to center position
M 1 0              # Move to minimum
M 1 4095           # Move to maximum
T 1 0              # Disable torque when done
```

### Checking servo health

```
ST 1               # Read full status
OM 1               # Check operating mode
MN 1               # Check min position limit
MX 1               # Check max position limit
RR 1 144 2         # Read input voltage (register 144, 2 bytes)
```

### Broadcast to all servos

```
L 254 1            # Turn on LED on all servos
T 254 1            # Enable torque on all servos
M 254 2048         # Move all servos to center
T 254 0            # Disable torque on all servos
L 254 0            # Turn off all LEDs
```

Note: read commands (`P`, `ST`, `RR`, `OM`, etc.) won't return useful data with broadcast since no servo responds.

### Changing baud rate

The servo's baud rate persists in EEPROM. The bus baud rate resets to 57,600 on every firmware boot. To change baud rate:

1. `T 1 0` - Torque off
2. `BR 1 3` - Set servo to 1Mbps (index 3)
3. `CB 1000000` - Change bus to match

If you lose communication after step 2, the servo is at the new rate but the bus isn't. Just run `CB 1000000` to reconnect.

## Architecture

The tester is built on a shared Dynamixel library (`src/dynamixel/`) that can be reused by the main controller firmware.

```
src/dynamixel/                  # Shared library
  dynamixel_registers.h         # XC430 control table addresses
  dynamixel_protocol.h/c        # Protocol 2.0 packet layer (CRC16, byte stuffing)
  dynamixel_hal.h/c             # PIO half-duplex UART with DMA
  dynamixel_servo.h/c           # High-level servo operations

src/dynamixel_tester/           # Tester application
  config.h                      # Pin assignments, buffer sizes
  main.c                        # Entry point
  shell.h/c                     # Interactive command processor
  usb.h/c                       # TinyUSB CDC setup
  usb_descriptors.c             # USB device identity
```

### Protocol Implementation

- **PIO UART**: Two state machines on the same GPIO pin handle TX and RX. Only one is active at a time (half-duplex).
- **DMA receive**: Bytes transfer from the PIO RX FIFO directly into a memory buffer, freeing the CPU during receive.
- **Byte stuffing**: Protocol 2.0 requires stuffing `FF FF FD` sequences in packet parameters. Handled transparently by `dxl_build_packet()` and `dxl_parse_packet()`.
- **CRC16**: Each packet includes a CRC16 checksum verified on receive.

## Common Register Addresses

For use with the `RR` and `RW` commands. See the [XC430 e-Manual](https://emanual.robotis.com/docs/en/dxl/x/xc430-w240t/) for the complete control table.

| Address | Size | Name | Access |
|---------|------|------|--------|
| 7 | 1 | ID | RW |
| 8 | 1 | Baud Rate | RW |
| 10 | 1 | Drive Mode | RW |
| 11 | 1 | Operating Mode | RW |
| 20 | 4 | Homing Offset | RW |
| 48 | 4 | Max Position Limit | RW |
| 52 | 4 | Min Position Limit | RW |
| 64 | 1 | Torque Enable | RW |
| 65 | 1 | LED | RW |
| 80 | 2 | Position D Gain | RW |
| 82 | 2 | Position I Gain | RW |
| 84 | 2 | Position P Gain | RW |
| 108 | 4 | Profile Acceleration | RW |
| 112 | 4 | Profile Velocity | RW |
| 116 | 4 | Goal Position | RW |
| 126 | 2 | Present Load | RO |
| 128 | 4 | Present Velocity | RO |
| 132 | 4 | Present Position | RO |
| 144 | 2 | Present Input Voltage | RO |
| 146 | 1 | Present Temperature | RO |
