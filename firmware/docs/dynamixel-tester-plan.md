# Dynamixel Servo Tester Build Target

## Context

Adding a new build target for testing/programming Dynamixel servos (XC430 and similar) using Protocol 2.0. The protocol implementation lives in a shared `src/dynamixel/` library so it can be reused by the main controller target later. The tester application in `src/dynamixel_tester/` follows the programmer target's interactive shell pattern.

Hardware: PIO-based half-duplex UART on a single GPIO pin. The XC430 is 3.3V TTL, same as the RP2350, so no level shifter is needed. Uses 2 PIO state machines (one TX, one RX) on the same pin, leveraging the existing `uart_tx.pio` and `uart_rx.pio` programs already in the repo.

## New Files

### Shared Library: `src/dynamixel/` (4 files)

**`dynamixel_registers.h`** - XC430 control table register addresses, baud rate indices, operating modes, position limits, factory reset options. Pure `#define` constants.

**`dynamixel_protocol.h/c`** - Protocol 2.0 packet layer:
- `dxl_packet_t` struct (id, instruction, params[], param_count, error)
- `dxl_result_t` enum (OK, TIMEOUT, INVALID_PACKET, CRC_MISMATCH, SERVO_ERROR, TX_FAIL, BUFFER_OVERFLOW)
- `dxl_crc16()` - CRC16 using the Dynamixel 256-entry lookup table
- `dxl_build_packet()` - serialize packet struct to wire bytes (header + ID + length + instruction + params + CRC, with byte stuffing for `FF FF FD` sequences in params)
- `dxl_parse_packet()` - deserialize wire bytes back to packet struct (header validation, CRC check, byte unstuffing)
- `dxl_result_to_string()`, `dxl_error_to_string()` - human-readable error descriptions

**`dynamixel_hal.h/c`** - Hardware abstraction using PIO half-duplex UART with DMA:
- `dxl_hal_config_t` struct (data_pin, baud_rate, pio instance)
- `dxl_hal_init()`:
  - Load TX and RX PIO programs into the same PIO instance, claim 2 state machines, configure both on the same GPIO pin
  - TX SM starts disabled, RX SM starts disabled
  - Uses `uart_tx_program_init()` and `uart_rx_program_init()` helpers from the existing PIO headers
  - Claims a DMA channel via `dma_claim_unused_channel()`
  - Allocates a `DXL_MAX_PACKET_SIZE` receive buffer
- `dxl_hal_set_baud_rate()` - reconfigure both SM clock dividers
- `dxl_hal_txrx()` - full TX/RX cycle:
  1. Flush RX FIFO
  2. Enable TX SM (pin becomes output, driven idle-high)
  3. Write packet bytes via `pio_sm_put_blocking()`
  4. Wait for TX FIFO empty + final byte shifted out (poll `pio_sm_is_tx_fifo_empty()`, then wait one byte-time for shift register to drain)
  5. Disable TX SM
  6. Enable RX SM (pin becomes input with pull-up)
  7. Configure DMA: read from PIO RX FIFO (fixed addr), write to rx_buffer (incrementing), transfer size 1 byte, DREQ paced by PIO RX FIFO (`pio_get_dreq(pio, sm_rx, false)`), transfer count = `DXL_MAX_PACKET_SIZE`
  8. Start DMA transfer
  9. Poll: check `DXL_MAX_PACKET_SIZE - dma_channel_hw_addr(dma_chan)->transfer_count` to get bytes received so far. Once >= 7 bytes, parse length field from buffer to determine expected total. Once total bytes received or timeout, abort DMA.
  10. Disable RX SM
  11. Parse the rx_buffer into the response `dxl_packet_t`
- `dxl_hal_tx()` - transmit-only for broadcast commands (no RX/DMA phase)
- `dxl_hal_flush_rx()` - drain RX FIFO and abort any in-progress DMA

**`dynamixel_servo.h/c`** - High-level servo operations (each composes a `dxl_packet_t` and calls `dxl_hal_txrx()`):
- `dxl_ping()` - ping servo, return model number + firmware version
- `dxl_scan()` - iterate IDs with short timeout, report found servos via callback
- `dxl_read_register()` / `dxl_write_register()` - generic register access (1/2/4 byte)
- `dxl_set_id()`, `dxl_set_baud_rate()` - EEPROM configuration helpers
- `dxl_factory_reset()`, `dxl_reboot()`
- `dxl_set_torque()`, `dxl_set_position()`, `dxl_set_led()`, `dxl_set_profile_velocity()`
- `dxl_read_status()` - reads position, temperature, voltage, load, moving state, hardware error into a `dxl_servo_status_t` struct
- `dxl_baud_index_to_rate()` - convert index (0-7) to actual baud value

### Tester Application: `src/dynamixel_tester/` (6 files)

**`config.h`** - Pin assignments and buffer sizes:
- `DXL_DATA_PIN` - single GPIO pin for the Dynamixel data line (placeholder value, any GPIO works with PIO)
- `DXL_PIO_INSTANCE` - which PIO to use (pio0 recommended, all 4 SMs free)
- `DXL_DEFAULT_BAUD_RATE` (57600)
- `INCOMING_REQUEST_BUFFER_SIZE` (128), `OUTGOING_RESPONSE_BUFFER_SIZE` (256)
- Status LED pins, watchdog settings (same pattern as programmer)

**`usb_descriptors.c`** - Based on programmer's version:
- USB PID: `0x0011` (programmer is `0x0010`)
- Product string: `"Dynamixel Servo Tester"`
- Everything else identical

**`usb.h/c`** - Based on programmer's version:
- Same TinyUSB CDC setup, timer architecture, LED control, `cdc_send()`
- Removes the unnecessary `controller/controller.h` and `usb/usb.h` includes from the programmer's version

**`shell.h/c`** - Interactive command processor:
- Simpler state machine than programmer (no FILLING_BUFFER state needed)
- `tud_cdc_rx_cb()` accumulates chars until newline, then calls `handle_shell_command()`
- Shell commands (space-separated args):

| Command | Format | Description |
|---------|--------|-------------|
| `H` | `H` | Help |
| `I` | `I` | System info (JSON) |
| `P` | `P <id>` | Ping servo |
| `S` | `S [start] [end]` | Scan for servos (default 0-253) |
| `RR` | `RR <id> <addr> <len>` | Read register |
| `RW` | `RW <id> <addr> <len> <val>` | Write register |
| `ID` | `ID <old> <new>` | Change servo ID |
| `BR` | `BR <id> <baud_index>` | Set baud rate |
| `FR` | `FR <id> <option>` | Factory reset (0=all, 1=except ID, 2=except ID+baud) |
| `RB` | `RB <id>` | Reboot servo |
| `M` | `M <id> <position>` | Move to position (0-4095) |
| `T` | `T <id> <0\|1>` | Torque enable/disable |
| `ST` | `ST <id>` | Read full status |
| `L` | `L <id> <0\|1>` | LED on/off |
| `V` | `V <id> <velocity>` | Set profile velocity |
| `CB` | `CB <rate>` | Change PIO baud rate |
| `R` | `R` | Reboot tester |

**`main.c`** - Following programmer's startup pattern:
- `bi_decl` blocks for picotool info (data pin, LED pins)
- `stdio_init_all()`, `logger_init()`, `board_init()`
- Allocate request buffer
- `dxl_hal_init()` with config from `config.h`
- `start_debug_blinker()`
- Startup task: `usb_init()`, `usb_start()`
- `vTaskStartScheduler()`
- `acw_post_logging_hook()` implementation (printf to debug UART0)

### Unit Test: `tests/tests/test_dynamixel_protocol.c`

Tests for the pure-computation protocol layer (no hardware required):
- CRC16 against known vectors from Dynamixel documentation
- Packet build round-trip (build then parse)
- CRC mismatch detection
- Invalid header detection
- Multi-byte parameter handling
- Zero-parameter edge case

## Modified Files

### `CMakeLists.txt`

After the programmer target definition (~line 232), add:

```cmake
# Dynamixel Protocol 2.0 library (shared, reusable by controller later)
set(DYNAMIXEL_SOURCES
        src/dynamixel/dynamixel_protocol.h
        src/dynamixel/dynamixel_protocol.c
        src/dynamixel/dynamixel_registers.h
        src/dynamixel/dynamixel_hal.h
        src/dynamixel/dynamixel_hal.c
        src/dynamixel/dynamixel_servo.h
        src/dynamixel/dynamixel_servo.c
)

set(DYNAMIXEL_TESTER_ADDITIONAL_SOURCES
        ${DYNAMIXEL_SOURCES}
        src/dynamixel_tester/main.c
        src/dynamixel_tester/config.h
        src/dynamixel_tester/shell.h
        src/dynamixel_tester/shell.c
        src/dynamixel_tester/usb.h
        src/dynamixel_tester/usb.c
        src/dynamixel_tester/usb_descriptors.c
)

set(DYNAMIXEL_TESTER_ADDITIONAL_LIBS
        tinyusb_device
        tinyusb_board
)

set(DYNAMIXEL_TESTER_ADDITIONAL_INCLUDES
)
```

And at the bottom with the other `create_firmware` calls:

```cmake
create_firmware("dynamixel-tester-${PICO_PLATFORM}-hw${HARDWARE_VERSION}"
        "${DYNAMIXEL_TESTER_ADDITIONAL_SOURCES}"
        "${DYNAMIXEL_TESTER_ADDITIONAL_LIBS}"
        "${DYNAMIXEL_TESTER_ADDITIONAL_INCLUDES}"
)
```

### `tests/CMakeLists.txt`

Add `test_dynamixel_protocol` executable and include it in `run_tests`.

## Implementation Order

1. `src/dynamixel/dynamixel_registers.h` - pure constants, no deps
2. `src/dynamixel/dynamixel_protocol.h/c` - packet layer + CRC (testable standalone)
3. `tests/tests/test_dynamixel_protocol.c` + test CMake changes - verify CRC and packets
4. `src/dynamixel/dynamixel_hal.h/c` - PIO half-duplex UART with DMA
5. `src/dynamixel/dynamixel_servo.h/c` - high-level operations
6. `src/dynamixel_tester/config.h` - pin/buffer defines
7. `src/dynamixel_tester/usb_descriptors.c` - USB identity
8. `src/dynamixel_tester/usb.h/c` - CDC setup
9. `src/dynamixel_tester/shell.h/c` - command processor
10. `src/dynamixel_tester/main.c` - entry point
11. `CMakeLists.txt` changes - build target

## Key Design Decisions

- **PIO over UART1**: Single-pin half-duplex via PIO is the natural fit for Dynamixel's bus topology. No direction control pin or external transceiver needed. Tighter TX-to-RX turnaround (nanoseconds vs microseconds). Uses 2 state machines on pio0 (all 4 are currently free). Leaves UART1 available for future use. The existing `uart_tx.pio` and `uart_rx.pio` programs are already compiled for all targets.
- **DMA for RX**: DMA transfers bytes from the PIO RX FIFO directly into a memory buffer, paced by the PIO's DREQ signal. This frees the CPU during receive and is robust at any baud rate (up to 4.5Mbps). We handle variable-length packets by starting a max-size DMA transfer, monitoring the transfer count to detect when the length field arrives (7 bytes in), then waiting for the full packet or timeout before aborting. One DMA channel is claimed at init and reused for every transaction.
- **Byte stuffing**: Protocol 2.0 requires stuffing `FF FF FD` sequences in parameters. Handled in `dxl_build_packet` / `dxl_parse_packet`.
- **TX completion detection**: After TX FIFO empties, the TX PIO program stalls on `pull side 1` holding the line idle-high. We wait for FIFO empty + one byte-time to ensure the shift register has drained before switching to RX.
- **COMMON_SOURCES bloat**: The new target inherits all common sources (sensor drivers etc) that it doesn't use. This is consistent with how programmer and usbc_pd work today. Linker strips unused code.
- **Placeholder pin number**: config.h uses a placeholder GPIO value for the data pin. Any GPIO works with PIO.

## Verification

1. **Unit tests**: `cd tests && cmake -B build . && cmake --build build && ./build/test_dynamixel_protocol`
2. **Build**: `cmake -B build . && cmake --build build --target dynamixel-tester-rp2350-arm-s-hw3`
3. **Flash + connect**: Flash the UF2 to the RP2350, connect via USB serial terminal
4. **Hardware test sequence**: `H` (help), `P 1` (ping servo at ID 1), `S` (scan), `ST 1` (read status), `T 1 1` / `M 1 2048` / `T 1 0` (move test)
