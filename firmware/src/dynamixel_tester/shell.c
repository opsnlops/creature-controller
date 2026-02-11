
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include <hardware/gpio.h>
#include <hardware/watchdog.h>
#include <pico/time.h>

#include "tusb.h"

#include "config.h"
#include "dynamixel/dynamixel_hal.h"
#include "dynamixel/dynamixel_registers.h"
#include "dynamixel/dynamixel_servo.h"
#include "logging/logging.h"
#include "shell.h"
#include "types.h"
#include "usb.h"
#include "version.h"
#include "watchdog/watchdog.h"

TaskHandle_t shell_task_handle = NULL;

extern volatile size_t xFreeHeapSpace;
extern dxl_hal_context_t *dxl_ctx;

extern u8 *request_buffer;
extern u32 requestBufferIndex;

static void cmd_help(void);
static void cmd_info(void);
static void cmd_ping(u8 *args);
static void cmd_scan(u8 *args);
static void cmd_read_register(u8 *args);
static void cmd_write_register(u8 *args);
static void cmd_set_id(u8 *args);
static void cmd_set_baud_rate(u8 *args);
static void cmd_factory_reset(u8 *args);
static void cmd_reboot_servo(u8 *args);
static void cmd_move(u8 *args);
static void cmd_torque(u8 *args);
static void cmd_status(u8 *args);
static void cmd_led(u8 *args);
static void cmd_velocity(u8 *args);
static void cmd_change_bus_baud(u8 *args);
static void cmd_operating_mode(u8 *args);
static void cmd_profile_accel(u8 *args);
static void cmd_homing_offset(u8 *args);
static void cmd_min_position(u8 *args);
static void cmd_max_position(u8 *args);
static void cmd_sync_status(u8 *args);
static void cmd_sync_move(u8 *args);

static void cmd_register_rw(u8 *args, const char *usage, const char *name, u16 addr, u16 len);
static void scan_callback(u8 id, u16 model_number, u8 firmware_version);

/**
 * Format and send a detailed error response. When the result is DXL_SERVO_ERROR,
 * appends the specific servo error name (e.g. "Data range error", "Access error").
 */
static void send_dxl_error(const char *operation, dxl_result_t res) {
    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    if (res == DXL_SERVO_ERROR) {
        u8 err = dxl_hal_last_servo_error(dxl_ctx);
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "ERR %s: %s (%s)", operation, dxl_result_to_string(res),
                 dxl_error_to_string(err));
    } else {
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "ERR %s: %s", operation, dxl_result_to_string(res));
    }
    send_response(buf);
}

void handle_shell_command(u8 *buffer) {
    debug("handling command: %s", buffer);

    // Parse the command token (first space-delimited word)
    char *cmd = (char *)buffer;
    char *args = NULL;

    // Find the first space to split command from arguments
    char *space = strchr(cmd, ' ');
    if (space != NULL) {
        *space = '\0';
        args = space + 1;
        // Skip leading whitespace in args
        while (*args == ' ')
            args++;
        if (*args == '\0')
            args = NULL;
    }

    if (strcmp(cmd, "H") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "I") == 0) {
        cmd_info();
    } else if (strcmp(cmd, "P") == 0) {
        cmd_ping((u8 *)args);
    } else if (strcmp(cmd, "S") == 0) {
        cmd_scan((u8 *)args);
    } else if (strcmp(cmd, "RR") == 0) {
        cmd_read_register((u8 *)args);
    } else if (strcmp(cmd, "RW") == 0) {
        cmd_write_register((u8 *)args);
    } else if (strcmp(cmd, "ID") == 0) {
        cmd_set_id((u8 *)args);
    } else if (strcmp(cmd, "BR") == 0) {
        cmd_set_baud_rate((u8 *)args);
    } else if (strcmp(cmd, "FR") == 0) {
        cmd_factory_reset((u8 *)args);
    } else if (strcmp(cmd, "RB") == 0) {
        cmd_reboot_servo((u8 *)args);
    } else if (strcmp(cmd, "M") == 0) {
        cmd_move((u8 *)args);
    } else if (strcmp(cmd, "T") == 0) {
        cmd_torque((u8 *)args);
    } else if (strcmp(cmd, "ST") == 0) {
        cmd_status((u8 *)args);
    } else if (strcmp(cmd, "SS") == 0) {
        cmd_sync_status((u8 *)args);
    } else if (strcmp(cmd, "SM") == 0) {
        cmd_sync_move((u8 *)args);
    } else if (strcmp(cmd, "L") == 0) {
        cmd_led((u8 *)args);
    } else if (strcmp(cmd, "V") == 0) {
        cmd_velocity((u8 *)args);
    } else if (strcmp(cmd, "CB") == 0) {
        cmd_change_bus_baud((u8 *)args);
    } else if (strcmp(cmd, "OM") == 0) {
        cmd_operating_mode((u8 *)args);
    } else if (strcmp(cmd, "PA") == 0) {
        cmd_profile_accel((u8 *)args);
    } else if (strcmp(cmd, "HO") == 0) {
        cmd_homing_offset((u8 *)args);
    } else if (strcmp(cmd, "MN") == 0) {
        cmd_min_position((u8 *)args);
    } else if (strcmp(cmd, "MX") == 0) {
        cmd_max_position((u8 *)args);
    } else if (strcmp(cmd, "R") == 0) {
        info("rebooting tester...");
        send_response("BYE!");
        vTaskDelay(pdMS_TO_TICKS(30));
        reboot();
    } else {
        char response[OUTGOING_RESPONSE_BUFFER_SIZE];
        memset(response, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);
        snprintf(response, OUTGOING_RESPONSE_BUFFER_SIZE, "ERR Unknown command: %s (use H for help)", cmd);
        send_response(response);
    }

    reset_request_buffer();
}

static void cmd_help(void) {
    char baud_line[OUTGOING_RESPONSE_BUFFER_SIZE];
    snprintf(baud_line, OUTGOING_RESPONSE_BUFFER_SIZE,
             "\nDynamixel Servo Tester (bus baud: %lu):", dxl_hal_get_baud_rate(dxl_ctx));
    send_response(baud_line);
    send_response("  H              - Help (this message)");
    send_response("  I              - System info (JSON)");
    send_response("  P <id>         - Ping servo");
    send_response("  S [start] [end]- Scan for servos (default 0-253)");
    send_response("  RR <id> <addr> <len> - Read register");
    send_response("  RW <id> <addr> <len> <val> - Write register");
    send_response("  ID <old> <new> - Change servo ID");
    send_response("  BR <id> <idx>  - Set baud rate (0=9.6k 1=57.6k 2=115.2k 3=1M 4=2M 5=3M 6=4M 7=4.5M)");
    send_response("  FR <id> <opt>  - Factory reset (0=all, 1=keep ID, 2=keep ID+baud)");
    send_response("  RB <id>        - Reboot servo");
    send_response("  M <id> <pos>   - Move to position (0-4095)");
    send_response("  T <id> <0|1>   - Torque enable/disable");
    send_response("  ST <id>        - Read full status");
    send_response("  SS <id> [id...]- Sync read status from multiple servos");
    send_response("  SM <id:pos>... - Sync write positions (e.g. SM 1:2048 2:1024)");
    send_response("  L <id> <0|1>   - LED on/off");
    send_response("  V <id> <vel>   - Set profile velocity");
    send_response("  CB <rate>      - Change PIO baud rate");
    send_response("  R              - Reboot tester");
    send_response("Register Shortcuts (omit value to read):");
    send_response("  OM <id> [mode] - Operating mode (0=cur 1=vel 3=pos 4=ext 5=c+p 16=PWM)");
    send_response("  PA <id> [accel]- Profile acceleration");
    send_response("  HO <id> [offset] - Homing offset");
    send_response("  MN <id> [min]  - Min position limit");
    send_response("  MX <id> [max]  - Max position limit");
}

static void cmd_info(void) {
    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);
    snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE,
             "{\"type\": \"dynamixel_tester\", \"version\": \"%s\", \"free_heap\": %u, \"uptime\": %lu, \"data_pin\": "
             "%d, \"baud_rate\": %lu}",
             CREATURE_FIRMWARE_VERSION_STRING, xFreeHeapSpace, to_ms_since_boot(get_absolute_time()), DXL_DATA_PIN,
             dxl_hal_get_baud_rate(dxl_ctx));
    send_response(buf);
}

static void cmd_ping(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: P <id>");
        return;
    }

    u32 id = (u32)strtoul((char *)args, NULL, 10);
    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);
    if (id > DXL_MAX_ID) {
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "ERR ID out of range (0-%u)", DXL_MAX_ID);
        send_response(buf);
        return;
    }

    dxl_ping_result_t result;
    dxl_result_t res = dxl_ping(dxl_ctx, (u8)id, &result);

    if (res == DXL_OK) {
        memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK ID=%lu model=%u fw=%u", id, result.model_number,
                 result.firmware_version);
        send_response(buf);
    } else {
        send_dxl_error("Ping failed", res);
    }
}

static u32 scan_found_count;

static void scan_callback(u8 id, u16 model_number, u8 firmware_version) {
    scan_found_count++;
    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);
    snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "  ID=%u model=%u fw=%u", id, model_number, firmware_version);
    send_response(buf);
}

static void cmd_scan(u8 *args) {
    u32 start_id = 0;
    u32 end_id = 253;

    if (args != NULL) {
        char *end_ptr = NULL;
        start_id = (u32)strtoul((char *)args, &end_ptr, 10);
        if (end_ptr != NULL && *end_ptr == ' ') {
            end_id = (u32)strtoul(end_ptr + 1, NULL, 10);
        }
    }

    if (start_id > DXL_MAX_ID)
        start_id = 0;
    if (end_id > DXL_MAX_ID)
        end_id = DXL_MAX_ID;

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);
    snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "Scanning IDs %lu-%lu...", start_id, end_id);
    send_response(buf);

    scan_found_count = 0;
    dxl_scan(dxl_ctx, (u8)start_id, (u8)end_id, scan_callback);

    if (scan_found_count == 0) {
        u32 current_baud = dxl_hal_get_baud_rate(dxl_ctx);
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE,
                 "No servos found. New servos default to 57600 baud; current bus baud is %lu.", current_baud);
        send_response(buf);
        if (current_baud != 57600) {
            send_response("Try: CB 57600");
        }
    } else {
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "Scan complete. Found %lu servo(s).", scan_found_count);
        send_response(buf);
    }
}

static void cmd_read_register(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: RR <id> <addr> <len>");
        return;
    }

    char *p = (char *)args;
    u32 id = (u32)strtoul(p, &p, 10);
    u32 addr = (u32)strtoul(p, &p, 10);
    u32 len = (u32)strtoul(p, &p, 10);

    if (len == 0 || len > 4) {
        send_response("ERR Length must be 1, 2, or 4");
        return;
    }

    u32 value = 0;
    dxl_result_t res = dxl_read_register(dxl_ctx, (u8)id, (u16)addr, (u16)len, &value);

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);

    if (res == DXL_OK) {
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK [%lu]@%lu = %lu (0x%lX)", len, addr, value, value);
        send_response(buf);
    } else {
        send_dxl_error("Read failed", res);
    }
}

static void cmd_write_register(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: RW <id> <addr> <len> <val>");
        return;
    }

    char *p = (char *)args;
    u32 id = (u32)strtoul(p, &p, 10);
    u32 addr = (u32)strtoul(p, &p, 10);
    u32 len = (u32)strtoul(p, &p, 10);
    u32 val = (u32)strtoul(p, &p, 10);

    if (len == 0 || len > 4) {
        send_response("ERR Length must be 1, 2, or 4");
        return;
    }

    dxl_result_t res = dxl_write_register(dxl_ctx, (u8)id, (u16)addr, (u16)len, val);

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);

    if (res == DXL_OK) {
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK Wrote %lu to [%lu]@%lu", val, len, addr);
        send_response(buf);
    } else {
        send_dxl_error("Write failed", res);
    }
}

static void cmd_set_id(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: ID <old> <new>");
        return;
    }

    char *p = (char *)args;
    u32 old_id = (u32)strtoul(p, &p, 10);
    u32 new_id = (u32)strtoul(p, &p, 10);

    dxl_result_t res = dxl_set_id(dxl_ctx, (u8)old_id, (u8)new_id);

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);

    if (res == DXL_OK) {
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK ID changed from %lu to %lu", old_id, new_id);
        send_response(buf);
    } else {
        send_dxl_error("Set ID failed", res);
    }
}

static void cmd_set_baud_rate(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: BR <id> <baud_index>");
        return;
    }

    char *p = (char *)args;
    u32 id = (u32)strtoul(p, &p, 10);
    u32 baud_idx = (u32)strtoul(p, &p, 10);

    u32 actual_rate = dxl_baud_index_to_rate((u8)baud_idx);
    if (actual_rate == 0) {
        char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "ERR Invalid baud index (0-%u)", DXL_MAX_BAUD_INDEX);
        send_response(buf);
        return;
    }

    dxl_result_t res = dxl_set_baud_rate(dxl_ctx, (u8)id, (u8)baud_idx);

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);

    if (res == DXL_OK) {
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK Baud rate set to index %lu (%lu bps)", baud_idx, actual_rate);
        send_response(buf);
    } else {
        send_dxl_error("Set baud rate failed", res);
    }
}

static void cmd_factory_reset(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: FR <id> <option>");
        return;
    }

    char *p = (char *)args;
    u32 id = (u32)strtoul(p, &p, 10);
    u32 option = (u32)strtoul(p, &p, 10);

    if (option > 2) {
        send_response("ERR Option must be 0, 1, or 2");
        return;
    }

    dxl_result_t res = dxl_factory_reset(dxl_ctx, (u8)id, (u8)option);

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);

    if (res == DXL_OK) {
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK Factory reset complete (option %lu)", option);
        send_response(buf);
    } else {
        send_dxl_error("Factory reset failed", res);
    }
}

static void cmd_reboot_servo(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: RB <id>");
        return;
    }

    u32 id = (u32)strtoul((char *)args, NULL, 10);

    dxl_result_t res = dxl_reboot(dxl_ctx, (u8)id);

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);

    if (res == DXL_OK) {
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK Servo %lu rebooting", id);
        send_response(buf);
    } else {
        send_dxl_error("Reboot failed", res);
    }
}

static void cmd_move(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: M <id> <position>");
        return;
    }

    char *p = (char *)args;
    u32 id = (u32)strtoul(p, &p, 10);
    u32 position = (u32)strtoul(p, &p, 10);

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);

    if (position > DXL_POSITION_MAX) {
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "ERR Position must be %u-%u", DXL_POSITION_MIN, DXL_POSITION_MAX);
        send_response(buf);
        return;
    }

    dxl_result_t res = dxl_set_position(dxl_ctx, (u8)id, position);

    if (res == DXL_OK) {
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK Moving ID %lu to position %lu", id, position);
        send_response(buf);
    } else {
        send_dxl_error("Move failed", res);
    }
}

static void cmd_torque(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: T <id> <0|1>");
        return;
    }

    char *p = (char *)args;
    u32 id = (u32)strtoul(p, &p, 10);
    u32 enable = (u32)strtoul(p, &p, 10);

    dxl_result_t res = dxl_set_torque(dxl_ctx, (u8)id, enable != 0);

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);

    if (res == DXL_OK) {
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK Torque %s for ID %lu", enable ? "enabled" : "disabled", id);
        send_response(buf);
    } else {
        send_dxl_error("Torque command failed", res);
    }
}

static void cmd_status(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: ST <id>");
        return;
    }

    u32 id = (u32)strtoul((char *)args, NULL, 10);

    dxl_servo_status_t status;
    dxl_result_t res = dxl_read_status(dxl_ctx, (u8)id, &status);

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);

    if (res == DXL_OK) {
        if (status.hardware_error != 0) {
            char hw_err_str[64];
            dxl_hw_error_to_string(status.hardware_error, hw_err_str, sizeof(hw_err_str));
            snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE,
                     "OK pos=%ld temp=%u voltage=%u load=%d moving=%u hw_err=0x%02X (%s)",
                     (long)status.present_position, status.present_temperature, status.present_voltage,
                     status.present_load, status.moving, status.hardware_error, hw_err_str);
        } else {
            snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK pos=%ld temp=%u voltage=%u load=%d moving=%u hw_err=none",
                     (long)status.present_position, status.present_temperature, status.present_voltage,
                     status.present_load, status.moving);
        }
        send_response(buf);
    } else {
        send_dxl_error("Status read failed", res);
    }
}

static void cmd_sync_status(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: SS <id1> [id2] [id3] ...");
        return;
    }

    u8 ids[DXL_MAX_SYNC_SERVOS];
    u8 id_count = 0;
    char *p = (char *)args;

    while (*p != '\0' && id_count < DXL_MAX_SYNC_SERVOS) {
        while (*p == ' ')
            p++;
        if (*p == '\0')
            break;

        u32 id = (u32)strtoul(p, &p, 10);
        if (id > DXL_MAX_ID) {
            char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
            snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "ERR ID out of range (0-%u)", DXL_MAX_ID);
            send_response(buf);
            return;
        }
        ids[id_count++] = (u8)id;
    }

    if (id_count == 0) {
        send_response("ERR No servo IDs specified");
        return;
    }

    dxl_sync_status_result_t results[DXL_MAX_SYNC_SERVOS];
    u8 result_count = 0;

    absolute_time_t t_start = get_absolute_time();
    dxl_result_t res = dxl_sync_read_status(dxl_ctx, ids, id_count, results, &result_count);
    i32 sync_read_us = (i32)absolute_time_diff_us(t_start, get_absolute_time());

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];

    if (res != DXL_OK) {
        send_dxl_error("Sync read failed", res);
        return;
    }

    for (u8 i = 0; i < id_count; i++) {
        memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);
        if (results[i].valid) {
            snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "ID=%u pos=%ld temp=%u voltage=%u load=%d", results[i].id,
                     (long)results[i].status.present_position, results[i].status.present_temperature,
                     results[i].status.present_voltage, results[i].status.present_load);
        } else if (results[i].servo_error != 0) {
            snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "ID=%u error: %s", results[i].id,
                     dxl_error_to_string(results[i].servo_error));
        } else {
            snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "ID=%u no response", results[i].id);
        }
        send_response(buf);
    }

    snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "sync_read: %ld us, %u/%u servos responded", (long)sync_read_us,
             result_count, id_count);
    send_response(buf);
}

static void cmd_sync_move(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: SM <id:pos> [id:pos] ... (e.g. SM 1:2048 2:1024)");
        return;
    }

    dxl_sync_position_t entries[DXL_MAX_SYNC_SERVOS];
    u8 count = 0;
    char *p = (char *)args;

    while (*p != '\0' && count < DXL_MAX_SYNC_SERVOS) {
        while (*p == ' ')
            p++;
        if (*p == '\0')
            break;

        // Parse "id:position"
        u32 id = (u32)strtoul(p, &p, 10);
        if (*p != ':') {
            send_response("ERR Expected id:position format");
            return;
        }
        p++; // skip ':'
        u32 pos = (u32)strtoul(p, &p, 10);

        if (id > DXL_MAX_ID) {
            char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
            snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "ERR ID out of range (0-%u)", DXL_MAX_ID);
            send_response(buf);
            return;
        }
        if (pos > DXL_POSITION_MAX) {
            char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
            snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "ERR Position must be %u-%u", DXL_POSITION_MIN,
                     DXL_POSITION_MAX);
            send_response(buf);
            return;
        }

        entries[count].id = (u8)id;
        entries[count].position = pos;
        count++;
    }

    if (count == 0) {
        send_response("ERR No servo entries specified");
        return;
    }

    dxl_result_t res = dxl_sync_write_position(dxl_ctx, entries, count);

    if (res == DXL_OK) {
        char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK Sync write %u servos", count);
        send_response(buf);
    } else {
        send_dxl_error("Sync write failed", res);
    }
}

static void cmd_led(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: L <id> <0|1>");
        return;
    }

    char *p = (char *)args;
    u32 id = (u32)strtoul(p, &p, 10);
    u32 on = (u32)strtoul(p, &p, 10);

    dxl_result_t res = dxl_set_led(dxl_ctx, (u8)id, on != 0);

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);

    if (res == DXL_OK) {
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK LED %s for ID %lu", on ? "on" : "off", id);
        send_response(buf);
    } else {
        send_dxl_error("LED command failed", res);
    }
}

static void cmd_velocity(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: V <id> <velocity>");
        return;
    }

    char *p = (char *)args;
    u32 id = (u32)strtoul(p, &p, 10);
    u32 velocity = (u32)strtoul(p, &p, 10);

    dxl_result_t res = dxl_set_profile_velocity(dxl_ctx, (u8)id, velocity);

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);

    if (res == DXL_OK) {
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK Profile velocity set to %lu for ID %lu", velocity, id);
        send_response(buf);
    } else {
        send_dxl_error("Velocity command failed", res);
    }
}

static void cmd_change_bus_baud(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: CB <rate>");
        return;
    }

    u32 rate = (u32)strtoul((char *)args, NULL, 10);
    if (rate == 0) {
        send_response("ERR Invalid baud rate");
        return;
    }

    dxl_hal_set_baud_rate(dxl_ctx, rate);

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);
    snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK PIO baud rate changed to %lu", rate);
    send_response(buf);
}

static void cmd_register_rw(u8 *args, const char *usage, const char *name, u16 addr, u16 len) {
    if (args == NULL) {
        char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
        snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "ERR Usage: %s", usage);
        send_response(buf);
        return;
    }

    char *p = (char *)args;
    u32 id = (u32)strtoul(p, &p, 10);

    while (*p == ' ')
        p++;

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);

    if (*p == '\0') {
        u32 value = 0;
        dxl_result_t res = dxl_read_register(dxl_ctx, (u8)id, addr, len, &value);
        if (res == DXL_OK) {
            snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK %s=%lu", name, value);
            send_response(buf);
        } else {
            send_dxl_error("Read failed", res);
        }
    } else {
        u32 value = (u32)strtoul(p, NULL, 10);
        dxl_result_t res = dxl_write_register(dxl_ctx, (u8)id, addr, len, value);
        if (res == DXL_OK) {
            snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK %s set to %lu", name, value);
            send_response(buf);
        } else {
            send_dxl_error("Write failed", res);
        }
    }
}

static void cmd_operating_mode(u8 *args) {
    if (args == NULL) {
        send_response("ERR Usage: OM <id> [mode] (0=cur 1=vel 3=pos 4=ext 5=c+p 16=PWM)");
        return;
    }

    char *p = (char *)args;
    u32 id = (u32)strtoul(p, &p, 10);

    while (*p == ' ')
        p++;

    char buf[OUTGOING_RESPONSE_BUFFER_SIZE];
    memset(buf, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);

    if (*p == '\0') {
        u32 value = 0;
        dxl_result_t res = dxl_read_register(dxl_ctx, (u8)id, DXL_REG_OPERATING_MODE, 1, &value);
        if (res == DXL_OK) {
            const char *mode_name = "unknown";
            switch (value) {
            case DXL_OP_MODE_CURRENT:
                mode_name = "current";
                break;
            case DXL_OP_MODE_VELOCITY:
                mode_name = "velocity";
                break;
            case DXL_OP_MODE_POSITION:
                mode_name = "position";
                break;
            case DXL_OP_MODE_EXT_POSITION:
                mode_name = "ext_position";
                break;
            case DXL_OP_MODE_CURRENT_POS:
                mode_name = "current_position";
                break;
            case DXL_OP_MODE_PWM:
                mode_name = "PWM";
                break;
            }
            snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK operating_mode=%lu (%s)", value, mode_name);
            send_response(buf);
        } else {
            send_dxl_error("Read failed", res);
        }
    } else {
        u32 mode = (u32)strtoul(p, NULL, 10);
        dxl_result_t res = dxl_write_register(dxl_ctx, (u8)id, DXL_REG_OPERATING_MODE, 1, mode);
        if (res == DXL_OK) {
            snprintf(buf, OUTGOING_RESPONSE_BUFFER_SIZE, "OK operating_mode set to %lu", mode);
            send_response(buf);
        } else {
            send_dxl_error("Write failed", res);
        }
    }
}

static void cmd_profile_accel(u8 *args) {
    cmd_register_rw(args, "PA <id> [accel]", "profile_acceleration", DXL_REG_PROFILE_ACCELERATION, 4);
}

static void cmd_homing_offset(u8 *args) {
    cmd_register_rw(args, "HO <id> [offset]", "homing_offset", DXL_REG_HOMING_OFFSET, 4);
}

static void cmd_min_position(u8 *args) {
    cmd_register_rw(args, "MN <id> [min]", "min_position", DXL_REG_MIN_POSITION_LIMIT, 4);
}

static void cmd_max_position(u8 *args) {
    cmd_register_rw(args, "MX <id> [max]", "max_position", DXL_REG_MAX_POSITION_LIMIT, 4);
}

void terminate_shell() {
    reset_request_buffer();

    if (shell_task_handle != NULL) {
        vTaskDelete(shell_task_handle);
        shell_task_handle = NULL;
        info("shell task terminated");
    } else {
        warning("shell task already terminated");
    }
}

void launch_shell() {
    if (shell_task_handle != NULL) {
        warning("shell task already running");
        return;
    }

    reset_request_buffer();

    xTaskCreate(shell_task, "shell_task", configMINIMAL_STACK_SIZE + 512, NULL, 1, &shell_task_handle);
    info("shell task launched");
}

void reset_request_buffer() {
    debug("resetting request buffer");
    memset(request_buffer, '\0', INCOMING_REQUEST_BUFFER_SIZE);
    requestBufferIndex = 0;
}

void send_response(const char *response) {
    u32 incoming_length = strlen(response);
    u32 message_length =
        incoming_length < OUTGOING_RESPONSE_BUFFER_SIZE ? incoming_length : OUTGOING_RESPONSE_BUFFER_SIZE;
    u32 buffer_size = message_length + 2;
    char *response_buffer = pvPortMalloc(buffer_size);

    if (response_buffer == NULL) {
        warning("unable to allocate a buffer for the response!");
        return;
    }

    memset(response_buffer, '\0', buffer_size);
    stpncpy(response_buffer, response, message_length);
    response_buffer[message_length] = '\n';

    cdc_send(response_buffer);
    vPortFree(response_buffer);
}

portTASK_FUNCTION(shell_task, pvParameters) {
    reset_request_buffer();

    for (EVER) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        debug("shell active. requestBufferIndex %lu", requestBufferIndex);
    }
}

// Automatically called by TinyUSB when data is received
void tud_cdc_rx_cb(__attribute__((unused)) uint8_t itf) {
    gpio_put(INCOMING_LED_PIN, true);

    u32 count = tud_cdc_available();
    if (count == 0)
        return;

    u8 tempBuffer[count];
    u32 readCount = tud_cdc_read(tempBuffer, count);

    for (u32 i = 0; i < readCount; i++) {
        char ch = tempBuffer[i];

        if (ch == 0x0A || ch == 0x0D) {
            if (requestBufferIndex == 0) {
                continue;
            }
            request_buffer[requestBufferIndex] = '\0';
            handle_shell_command(request_buffer);
            reset_request_buffer();
        } else {
            if (requestBufferIndex < INCOMING_REQUEST_BUFFER_SIZE - 1) {
                request_buffer[requestBufferIndex++] = ch;
            } else {
                warning("Request buffer overflow");
                reset_request_buffer();
            }
        }
    }
}
