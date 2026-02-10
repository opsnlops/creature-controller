# I2C EEPROM Programmer

A firmware target for programming "personality modules" — small I2C EEPROMs that give each creature its unique identity. The EEPROM stores the creature's USB VID/PID, serial number, product name, manufacturer name, and logging level. When the main controller firmware boots, it reads this EEPROM to configure itself.

## What's a Personality Module?

Each animatronic creature has an I2C EEPROM (e.g., 24C256) soldered to its controller board. This chip holds a small binary blob that tells the controller firmware who it is. Think of it as a name tag that also carries configuration.

The main controller reads this at startup to set its USB identity, logging behavior, and other per-creature settings. The programmer firmware is the tool used to write these EEPROMs.

## Hardware Setup

- **I2C bus**: `i2c1`
- **SDA pin**: GPIO 2
- **SCL pin**: GPIO 3
- **I2C speed**: 100 kHz
- **EEPROM address**: `0x50` (standard for 24-series EEPROMs)
- **EEPROM page size**: 64 bytes

### Status LEDs

| GPIO | Function |
|------|----------|
| 16 | CDC mounted (USB serial connected) |
| 17 | Data received (flashes on incoming data) |
| 18 | Data transmitted (flashes on outgoing data) |

### USB Identity

- **VID**: `0x0666`
- **PID**: `0x0010`
- **Product string**: "i2c EEPROM Programmer"

## Building and Flashing

```bash
cmake -B build .
cmake --build build --target programmer-rp2350-arm-s-hw3
```

Flash the resulting UF2 at `build/programmer-rp2350-arm-s-hw3.uf2`.

## Connecting

The programmer appears as a USB CDC serial device. Connect with any serial terminal:

```bash
screen /dev/tty.usbmodem* 115200
```

Type `H` and press Enter to see available commands.

## Commands

| Command | Description |
|---------|-------------|
| `H` | Help (lists commands, free heap, firmware version) |
| `I` | System info as JSON (`version`, `free_heap`, `uptime`) |
| `L<size>` | Load `<size>` bytes of binary data (no space between L and the number) |
| `B` | Burn loaded data to EEPROM |
| `V` | Verify EEPROM contents against loaded data |
| `P` | Print loaded data as hex dump |
| `R` | Reboot the programmer |
| `ESC` (0x1B) | Reset all buffers and return to idle |

## Programming Workflow

A typical session looks like this:

### 1. Load the binary data

Send the `L` command with the byte count, then send the raw binary data:

```
L256
```

The programmer responds with `GO_AHEAD`. Now send exactly 256 bytes of raw binary data (no encoding, no escaping). When all bytes are received, the programmer responds with `OK`.

### 2. Preview (optional)

```
P
```

Prints a hex dump of the loaded data so you can verify it looks right before burning.

### 3. Burn to EEPROM

```
B
```

Writes the loaded data to the EEPROM starting at address 0. Writes in 64-byte pages with 15ms delays between pages for the EEPROM's internal write cycle. Responds with `OK` when done.

### 4. Verify

```
V
```

Reads the EEPROM back and compares byte-by-byte against the loaded data. Responds with `OK Data verified successfully!` or reports the first mismatched byte.

### Automated example

For scripting, the full sequence is:

```
-> L256
<- GO_AHEAD
-> <256 raw bytes>
<- OK
-> B
<- OK
-> V
<- OK Data verified successfully!
```

## EEPROM Data Format

The personality module uses a compact binary format. All multi-byte integers are big-endian. Strings are length-prefixed (1 byte length, then ASCII characters, no null terminator).

```
Offset  Size   Field
------  ----   -----
0       4      Magic: "HOP!" (0x48 0x4F 0x50 0x21)
4       2      USB VID (big-endian)
6       2      USB PID (big-endian)
8       2      Version BCD (big-endian, currently unused — firmware version is substituted)
10      1      Logging level
11      1+n    Serial number (length byte + ASCII)
...     1+n    Product name (length byte + ASCII)
...     1+n    Manufacturer name (length byte + ASCII)
...     ...    Optional custom strings (length byte + ASCII each, 0xFF = empty/end)
```

### Example

A personality module for a creature called "Red Dragon" with serial "CC0001":

```
48 4F 50 21              HOP! magic
06 66                    VID = 0x0666
00 20                    PID = 0x0020
01 00                    Version 1.0 (BCD, unused)
03                       Logging level = 3 (INFO)
06 43 43 30 30 30 31     Serial: "CC0001" (length 6)
0A 52 65 64 20 44 72 61 67 6F 6E  Product: "Red Dragon" (length 10)
03 41 43 57              Manufacturer: "ACW" (length 3)
FF                       No custom strings
```

### Logging Levels

| Value | Level |
|-------|-------|
| 0 | TRACE |
| 1 | VERBOSE |
| 2 | DEBUG |
| 3 | INFO |
| 4 | WARNING |
| 5 | ERROR |

## Error Recovery

If something goes wrong during a transfer (garbled data, wrong byte count, etc.):

1. Send **ESC** (0x1B) to reset all buffers
2. Start over with `L<size>`

If the programmer gets into a bad state, use `R` to reboot or just unplug and replug the board.

## Write Speed

At 100 kHz I2C with 64-byte pages and 15ms write cycle delays:

- ~64 bytes every 15ms = ~4.3 KB/s
- 256 bytes: ~60ms
- 1 KB: ~240ms
- 32 KB (max): ~7.5 seconds

The 32 KB buffer limit is far larger than a typical personality module needs (usually under 100 bytes).

## Architecture

```
src/programmer/
  config.h              Pin assignments, buffer sizes
  main.c                Entry point, buffer allocation
  shell.h/c             Command handling, USB CDC callbacks, state machine
  i2c_programmer.h/c    I2C read/write/verify operations
  usb.h/c               TinyUSB CDC setup and timers
  usb_descriptors.c     USB device identity

src/device/
  eeprom.h/c            EEPROM reader used by the main controller firmware
                        (parses the format written by the programmer)
```

The programmer's I2C write code lives in `i2c_programmer.c`. The parsing/reading code that the main controller uses at boot lives separately in `src/device/eeprom.c` — both understand the same binary format.
