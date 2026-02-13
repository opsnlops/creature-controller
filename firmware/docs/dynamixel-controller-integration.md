# Dynamixel Controller Integration

## Context

The Dynamixel servo library (protocol, HAL, servo API) is complete and tested via the `dynamixel-tester` firmware target. This plan integrates it into the main **controller firmware** (RP2350, C) and the **host controller** (Linux/RPi, C++), enabling creatures with mixed PWM + Dynamixel servos.

This is a **CC_VER4** hardware version. VER4 defines imply VER3 defines (all VER3 features carry forward).

### Problem

Currently only PWM servos are supported. The host sends `CONFIG\tSERVO <pin> <min_us> <max_us>` and `POS\t<pin> <microseconds>`. Dynamixel servos use a completely different control model (serial bus protocol, position range 0-4095, no pulse width concept) and need a dedicated FreeRTOS task instead of the PWM ISR.

### Approach

- Extend CONFIG and POS message protocols with new token formats (no new message types)
- New `DSENSE` message for per-motor Dynamixel telemetry (temperature, load, voltage)
- Dedicated FreeRTOS task: Sync Write at 50Hz for positions, Sync Read every ~200ms for telemetry
- Profile Velocity set during config for smooth servo-side motion interpolation at 4kHz
- Torque lifecycle tied to power relay (enable on first frame, disable on disconnect/e-stop)

---

## Wire Protocol Changes

### CONFIG message

Existing tokens unchanged. New Dynamixel token:

```
CONFIG\tSERVO 0 1300 2200\tDYNAMIXEL 1 0 4095 134\tDYNAMIXEL 2 1024 3072 27
```

- `DYNAMIXEL <dxl_id> <min_position> <max_position> <profile_velocity>`
- `dxl_id`: bus address (1-253)
- `min_position` / `max_position`: safe motion range in Dynamixel position units (0-4095)
- `profile_velocity`: max speed for motion profiling (computed from smoothing_value by host)

### POS message

PWM tokens unchanged. Dynamixel tokens use `D` prefix:

```
POS\t0 1500\tD1 2048\tD2 1024
```

- `D<dxl_id> <position>`: position in native Dynamixel units
- No prefix = PWM servo (backwards-compatible)

### DSENSE message (firmware -> host, new)

Per-motor Dynamixel telemetry, sent every ~200ms. Follows existing `MSENSE`/`BSENSE` pattern:

```
DSENSE\tD1 45 128 7400\tD2 43 -50 7350
```

Each token: `D<dxl_id> <temperature_C> <present_load> <voltage_mV>`

- `temperature_C`: degrees Celsius (from present_temperature register)
- `present_load`: -1000 to 1000 (proportional to max torque, signed -- indicates direction)
- `voltage_mV`: input voltage in millivolts

The host monitors these for:
- Sustained high load -> stall detection -> safe mode
- Temperature trends -> proactive thermal management
- Voltage drops -> power supply issues

Note: Dynamixel servos have built-in overload and thermal protection (set hardware error flag and disable torque), so firmware telemetry is for proactive host-side monitoring, not real-time safety.

---

## Firmware Changes

### 1. CC_VER4 build support -- `CMakeLists.txt`

Add VER4 handling. VER4 implies VER3:

```cmake
elseif (HARDWARE_VERSION STREQUAL "4")
    message("HARDWARE_VERSION is set to 4, setting CC_VER4=1 and CC_VER3=1")
    add_compile_definitions(CC_VER4=1 CC_VER3=1)
```

### 2. VER4 config -- `src/controller/config.h`

Add `#ifdef CC_VER4` section:

```c
#ifdef CC_VER4

// Dynamixel bus configuration
#define DXL_DATA_PIN         22
#define DXL_PIO              pio1
#define DXL_BAUD_RATE        1000000
#define MAX_DYNAMIXEL_SERVOS 16

// Move status lights to pio0 (pio1 now used by Dynamixel)
#undef STATUS_LIGHTS_PIO
#define STATUS_LIGHTS_PIO    pio0

// GPIO 22 was CONTROLLER_RESET_PIN in VER3 -- move it
#undef CONTROLLER_RESET_PIN
#define CONTROLLER_RESET_PIN 23

// Dynamixel sensor reporting interval
#define DXL_SENSOR_REPORT_INTERVAL_FRAMES 10  // every 10 frames = 200ms at 50Hz

#endif
```

### 3. Dynamixel motor map -- `src/controller/controller.h`

```c
#ifdef CC_VER4

typedef struct {
    u8 dxl_id;
    u32 min_position;
    u32 max_position;
    u32 requested_position;
    bool is_configured;
} DynamixelMotorEntry;

extern DynamixelMotorEntry dxl_motors[MAX_DYNAMIXEL_SERVOS];
extern u8 dxl_motor_count;

bool configureDynamixelServo(u8 dxl_id, u32 min_pos, u32 max_pos, u32 profile_velocity);
bool requestDynamixelPosition(u8 dxl_id, u32 position);
void dynamixel_set_torque_all(bool enable);

#endif
```

### 4. Dynamixel controller -- `src/controller/controller.c`

**Init (`controller_init`):**
- `dxl_hal_init()` with `DXL_DATA_PIN`, `DXL_PIO`, `DXL_BAUD_RATE`
- Create `dxl_motors_mutex`

**Configuration (`configureDynamixelServo(dxl_id, min_pos, max_pos, profile_velocity)`):**
- Add entry to `dxl_motors[]`, store min/max, `is_configured = true`
- Set Profile Velocity via `dxl_set_profile_velocity(ctx, dxl_id, profile_velocity)`
- Do NOT enable torque here (torque tracks power relay state)

**Torque lifecycle (`dynamixel_set_torque_all`):**
- Called from `first_frame_received(true)` -> enable torque on all configured Dynamixel servos
- Called from `controller_disconnected()` / emergency stop -> disable torque
- Mirrors the existing power relay behavior for PWM servos

**Position request (`requestDynamixelPosition`):**
- Look up dxl_id in `dxl_motors[]`
- Bounds-check against min/max
- Store `requested_position` (mutex-protected)

**Dynamixel task (`dynamixel_controller_task`):**

```
loop (50Hz via vTaskDelay):
    if controller_safe_to_run and dxl_motor_count > 0:
        // Position update -- every frame
        build dxl_sync_position_t array from dxl_motors[]
        dxl_sync_write_position(ctx, entries, count)  // ~0.5ms broadcast, no response

        // Telemetry -- every DXL_SENSOR_REPORT_INTERVAL_FRAMES frames
        if frame_counter % DXL_SENSOR_REPORT_INTERVAL_FRAMES == 0:
            dxl_sync_read_status(ctx, ids, count, results, &result_count)  // ~6-7ms, yields via semaphore
            send DSENSE message to host
            check for hardware_error flags -> log warnings
```

### 5. CONFIG handler -- `src/messaging/processors/config_message.c`

Change the motor type check from hard rejection to dispatch:

```c
if (strcmp(motor_type, "SERVO") == 0) {
    // existing PWM path unchanged
    configureServoMinMax(output_position, min_us, max_us);
}
#ifdef CC_VER4
else if (strcmp(motor_type, "DYNAMIXEL") == 0) {
    // parse dxl_id, min_pos, max_pos, profile_velocity
    configureDynamixelServo(dxl_id, min_pos, max_pos, profile_velocity);
}
#endif
else {
    error("unknown motor type: %s", motor_type);
    return false;
}
```

### 6. POS handler -- `src/messaging/processors/position_message.c`

```c
if (position[0] == 'D') {
    // Dynamixel: D<id> <position>
    u8 dxl_id = (u8)stringToU16(&position[1]);
    u32 dxl_pos = (u32)stringToU16(value);
    requestDynamixelPosition(dxl_id, dxl_pos);
} else {
    // PWM: <pin> <microseconds> (existing path)
    requestServoPosition(position, stringToU16(value));
}
```

### 7. Torque integration -- `src/controller/controller.c`

In `first_frame_received(true)`:
```c
// Existing: enable power relay for PWM servos
gpio_put(POWER_PIN, 1);
// New: enable torque on all Dynamixel servos
#ifdef CC_VER4
dynamixel_set_torque_all(true);
#endif
```

In `controller_disconnected()` and emergency stop handler:
```c
#ifdef CC_VER4
dynamixel_set_torque_all(false);
#endif
```

### 8. Stats reporter -- `src/debug/stats_reporter.c`

Append Dynamixel bus metrics (only if VER4):

```
...\tDXL_TX <n>\tDXL_RX <n>\tDXL_ERR <n>\tDXL_CRC <n>\tDXL_TO <n>
```

Read from `dxl_hal_metrics(ctx)`.

### 9. Start task -- `src/controller/main.c`

```c
#ifdef CC_VER4
xTaskCreate(dynamixel_controller_task, "dxl_ctrl", 1024, NULL, CONTROL_TASK_PRIORITY, NULL);
#endif
```

---

## Host Controller Changes

All files in `~/code/creature-controller/controller/src/`.

### 1. Motor type enum -- `creature/Creature.h`

```cpp
enum motor_type { servo, dynamixel, stepper, invalid_motor };
```

Update `stringToMotorType()` in `Creature.cpp` to recognize `"dynamixel"`.

### 2. Servo class -- `device/Servo.h`

Add motor type field and accessor:
- `creatures::creature::Creature::motor_type motorType;`
- `getMotorType()` accessor
- Constructor parameter (default `servo` for backwards compatibility)

For Dynamixel motors, `min_pulse_us`/`max_pulse_us` store Dynamixel position range. The `positionToMicroseconds()` linear mapping math is identical -- just different units.

`calculateNextTick()` should skip host-side EMA smoothing for Dynamixel motors (check `motorType`). The servo handles smoothing internally via Profile Velocity at 4kHz, so the host sends raw target positions each frame.

### 3. ServoSpecifier -- `device/ServoSpecifier.h`

Add type field:
```cpp
struct ServoSpecifier {
    creatures::config::UARTDevice::module_name module;
    u16 pin;  // GPIO pin for PWM, Dynamixel bus ID for dynamixel
    creatures::creature::Creature::motor_type type = creatures::creature::Creature::servo;
};
```

Update hash and equality operators to include type.

### 4. CreatureBuilder -- `config/CreatureBuilder.cpp`

Add `"dynamixel"` case in motor type switch. Different JSON fields:

```json
{
  "type": "dynamixel",
  "id": "head_pan",
  "name": "Head Pan",
  "output_module": "A",
  "dxl_id": 1,
  "min_position": 0,
  "max_position": 4095,
  "smoothing_value": 0.85,
  "inverted": false,
  "default_position": "center"
}
```

Required fields for dynamixel: `type`, `id`, `name`, `output_module`, `dxl_id`, `min_position`, `max_position`, `smoothing_value`, `inverted`, `default_position`.

Create Servo with `ServoSpecifier{module, dxl_id, dynamixel}`, min/max position in pulse fields.

### 5. ServoConfig -- `commands/tokens/ServoConfig.cpp`

Format based on motor type:
```cpp
if (servo->getMotorType() == Creature::dynamixel) {
    u32 velocity = (1.0f - servo->getSmoothingValue()) * DXL_MAX_PROFILE_VELOCITY;
    return fmt::format("DYNAMIXEL {} {} {} {}", servo->getOutputHeader(),
                       servo->getMinPulseUs(), servo->getMaxPulseUs(), velocity);
}
return fmt::format("SERVO {} {} {}", ...);  // existing
```

### 6. ServoPosition -- `commands/tokens/ServoPosition.cpp`

Format with D prefix for Dynamixel:
```cpp
if (servoId.type == Creature::dynamixel) {
    return fmt::format("D{} {}", servoId.pin, requestedTicks);
}
return fmt::format("{} {}", servoId.pin, requestedTicks);  // existing
```

### 7. DSENSE handler (new) -- `io/handlers/DynamixelSensorHandler.cpp`

New handler registered in `MessageProcessor` for `"DSENSE"` messages. Parses per-motor telemetry tokens, builds JSON payload, sends to websocket queue. Follows existing `MotorSensorHandler` pattern.

The host monitors:
- `present_load` magnitude > threshold for sustained period -> stall detected -> trigger safe mode
- Temperature approaching limits -> reduce motion aggressiveness
- Voltage drops -> alert

---

## Profile Velocity from Smoothing Value

Dynamixel Profile Velocity (register 112) controls how fast the servo moves to goal positions. The servo interpolates internally at ~4kHz using a trapezoidal velocity profile (accelerate -> cruise -> decelerate), which is dramatically smoother than host-side 50Hz interpolation.

The existing `smoothing_value` (0.0-1.0) maps directly to Profile Velocity. The intent is identical -- "how responsive vs. smooth should this joint be" -- just the mechanism differs. High smoothing = slow motion = low velocity:

```
velocity = (1.0 - smoothing_value) * DXL_MAX_PROFILE_VELOCITY
```

| smoothing | velocity | ~RPM | feel |
|-----------|----------|------|------|
| 0.95 | 13 | 3 | very smooth, slow |
| 0.90 | 27 | 6 | smooth |
| 0.80 | 53 | 12 | moderate |
| 0.50 | 134 | 31 | responsive |
| 0.10 | 240 | 55 | snappy |

`DXL_MAX_PROFILE_VELOCITY` defined in config.h (~267 for XL-330, each unit = 0.229 RPM).

**Host-side smoothing is skipped for Dynamixel motors.** Since the servo handles interpolation at 4kHz via Profile Velocity, `calculateNextTick()` should pass through the raw target position for Dynamixel motors instead of applying the EMA filter. The host sends the target position directly each frame; the servo does the smoothing.

**Firmware implementation:** The CONFIG message doesn't carry the smoothing value today (only min/max). Two options:
1. Add smoothing to the CONFIG token: `DYNAMIXEL <dxl_id> <min> <max> <smoothing_x100>` (smoothing * 100 as integer, e.g., 90 for 0.90)
2. Add a separate velocity field to the CONFIG token: `DYNAMIXEL <dxl_id> <min> <max> <profile_velocity>`

Option 2 is cleaner -- the firmware doesn't need to know about the smoothing abstraction, it just receives the computed velocity value. The host does the mapping:

```
CONFIG\tSERVO 0 1300 2200\tDYNAMIXEL 1 1024 3072 27
```

The firmware calls `dxl_set_profile_velocity(ctx, dxl_id, profile_velocity)` during configuration.

---

## Example: Mixed Creature Config

```json
{
  "name": "TestCreature",
  "type": "parrot",
  "servo_frequency": 50,
  "position_min": 0,
  "position_max": 1023,
  "motors": [
    {
      "type": "servo",
      "id": "neck_left",
      "output_module": "A",
      "output_header": 0,
      "min_pulse_us": 1300,
      "max_pulse_us": 2200,
      "smoothing_value": 0.90,
      "inverted": false,
      "default_position": "center"
    },
    {
      "type": "dynamixel",
      "id": "head_pan",
      "output_module": "A",
      "dxl_id": 1,
      "min_position": 1024,
      "max_position": 3072,
      "smoothing_value": 0.85,
      "inverted": false,
      "default_position": "center"
    }
  ]
}
```

Resulting CONFIG: `CONFIG\tSERVO 0 1300 2200\tDYNAMIXEL 1 1024 3072 40`
Resulting POS: `POS\t0 1750\tD1 2048`
Telemetry from firmware: `DSENSE\tD1 43 -50 7400`

---

## Implementation Order

1. **CMakeLists.txt** -- CC_VER4 build support
2. **config.h** -- VER4 pin/pio/baud defines, status lights PIO swap
3. **controller.h/c** -- Dynamixel motor map, init, configure, position, torque lifecycle
4. **config_message.c** -- Parse DYNAMIXEL tokens
5. **position_message.c** -- Handle D-prefix tokens
6. **controller.c** -- Dynamixel task (Sync Write + periodic Sync Read + DSENSE)
7. **main.c** -- Create Dynamixel task
8. **stats_reporter.c** -- Dynamixel bus metrics
9. **Host: Creature.h/cpp** -- `dynamixel` motor type
10. **Host: Servo.h/cpp** -- motor_type field
11. **Host: ServoSpecifier.h** -- type field
12. **Host: CreatureBuilder.cpp** -- Parse dynamixel JSON
13. **Host: ServoConfig.cpp + ServoPosition.cpp** -- Wire format
14. **Host: DynamixelSensorHandler** -- DSENSE message handler
15. **End-to-end testing**

## Verification

1. Build firmware: `HARDWARE_VERSION=4 cmake -B build . && cmake --build build --target controller-rp2350-arm-s-hw4`
2. Build host: `cd ~/code/creature-controller/controller && cmake --build build`
3. Test firmware via USB terminal:
   - Send `CONFIG\tSERVO 0 1000 2000\tDYNAMIXEL 1 0 4095 100`
   - Verify READY response
   - Send `POS\t0 1500\tD1 2048`
   - Verify PWM servo moves and Dynamixel servo moves
   - Verify DSENSE messages appear every ~200ms with temperature/load/voltage
4. Test torque lifecycle: disconnect USB -> verify Dynamixel servos go limp
5. Run firmware unit tests: `cd tests && cmake --build build && ./build/test_dynamixel_servo && ./build/test_dynamixel_protocol`

## Key Files

| File | Change |
|------|--------|
| `firmware/CMakeLists.txt` | CC_VER4 build support |
| `firmware/src/controller/config.h` | VER4 section: DXL pin/pio/baud, status lights PIO swap |
| `firmware/src/controller/controller.h` | DynamixelMotorEntry, new APIs, torque lifecycle |
| `firmware/src/controller/controller.c` | HAL init, motor map, task, torque, DSENSE |
| `firmware/src/controller/main.c` | Create Dynamixel task |
| `firmware/src/messaging/processors/config_message.c` | Parse DYNAMIXEL tokens |
| `firmware/src/messaging/processors/position_message.c` | Handle D-prefix tokens |
| `firmware/src/debug/stats_reporter.c` | Dynamixel bus metrics |
| `controller/src/creature/Creature.h` | Add `dynamixel` motor type |
| `controller/src/creature/Creature.cpp` | `stringToMotorType("dynamixel")` |
| `controller/src/device/Servo.h` | Add motor_type field |
| `controller/src/device/Servo.cpp` | Constructor with motor_type |
| `controller/src/device/ServoSpecifier.h` | Add type field |
| `controller/src/config/CreatureBuilder.cpp` | Parse dynamixel motors from JSON |
| `controller/src/controller/commands/tokens/ServoConfig.cpp` | DYNAMIXEL format |
| `controller/src/controller/commands/tokens/ServoPosition.cpp` | D-prefix format |
| `controller/src/io/handlers/DynamixelSensorHandler.cpp` | New DSENSE handler |
| `controller/src/io/MessageProcessor.cpp` | Register DSENSE handler |
