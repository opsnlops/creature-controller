
#pragma once

/**
 * @file dynamixel_registers.h
 * @brief Dynamixel XC430 control table register addresses and protocol constants
 *
 * Register addresses and sizes for XC430-series servos using Protocol 2.0.
 * See the Dynamixel XC430 e-Manual for the complete control table.
 */

// ---- Protocol 2.0 Constants ----

#define DXL_HEADER_0 0xFF
#define DXL_HEADER_1 0xFF
#define DXL_HEADER_2 0xFD
#define DXL_RESERVED 0x00

#define DXL_BROADCAST_ID 0xFE
#define DXL_MAX_ID 0xFD
#define DXL_MAX_PACKET_SIZE 256

// ---- Instructions ----

#define DXL_INST_PING 0x01
#define DXL_INST_READ 0x02
#define DXL_INST_WRITE 0x03
#define DXL_INST_REG_WRITE 0x04
#define DXL_INST_ACTION 0x05
#define DXL_INST_FACTORY_RESET 0x06
#define DXL_INST_REBOOT 0x08
#define DXL_INST_STATUS 0x55
#define DXL_INST_SYNC_READ 0x82
#define DXL_INST_SYNC_WRITE 0x83
#define DXL_INST_BULK_READ 0x92
#define DXL_INST_BULK_WRITE 0x93

// ---- Status Return Error Bits ----

#define DXL_ERR_RESULT_FAIL 0x01
#define DXL_ERR_INST_ERROR 0x02
#define DXL_ERR_CRC_ERROR 0x03
#define DXL_ERR_DATA_RANGE 0x04
#define DXL_ERR_DATA_LENGTH 0x05
#define DXL_ERR_DATA_LIMIT 0x06
#define DXL_ERR_ACCESS 0x07

// ---- Hardware Error Bits (from Hardware Error Status register) ----

#define DXL_HW_ERR_INPUT_VOLTAGE 0x01
#define DXL_HW_ERR_OVERHEATING 0x04
#define DXL_HW_ERR_MOTOR_ENCODER 0x08
#define DXL_HW_ERR_ELEC_SHOCK 0x10
#define DXL_HW_ERR_OVERLOAD 0x20

// ---- Factory Reset Options ----

#define DXL_RESET_ALL 0x00
#define DXL_RESET_EXCEPT_ID 0x01
#define DXL_RESET_EXCEPT_ID_BAUD 0x02

// ---- Baud Rate Indices ----
// These map to the Baud Rate register value (address 8)

#define DXL_BAUD_INDEX_9600 0
#define DXL_BAUD_INDEX_57600 1
#define DXL_BAUD_INDEX_115200 2
#define DXL_BAUD_INDEX_1M 3
#define DXL_BAUD_INDEX_2M 4
#define DXL_BAUD_INDEX_3M 5
#define DXL_BAUD_INDEX_4M 6
#define DXL_BAUD_INDEX_4_5M 7

// ---- Operating Modes ----

#define DXL_OP_MODE_CURRENT 0
#define DXL_OP_MODE_VELOCITY 1
#define DXL_OP_MODE_POSITION 3
#define DXL_OP_MODE_EXT_POSITION 4
#define DXL_OP_MODE_CURRENT_POS 5
#define DXL_OP_MODE_PWM 16

// ---- EEPROM Area (persisted, write only when torque is off) ----

#define DXL_REG_MODEL_NUMBER 0        // 2 bytes, RO
#define DXL_REG_MODEL_INFO 2          // 4 bytes, RO
#define DXL_REG_FIRMWARE_VERSION 6    // 1 byte,  RO
#define DXL_REG_ID 7                  // 1 byte,  RW
#define DXL_REG_BAUD_RATE 8           // 1 byte,  RW
#define DXL_REG_RETURN_DELAY 9        // 1 byte,  RW
#define DXL_REG_DRIVE_MODE 10         // 1 byte,  RW
#define DXL_REG_OPERATING_MODE 11     // 1 byte,  RW
#define DXL_REG_SECONDARY_ID 12       // 1 byte,  RW
#define DXL_REG_PROTOCOL_TYPE 13      // 1 byte,  RW
#define DXL_REG_HOMING_OFFSET 20      // 4 bytes, RW
#define DXL_REG_MOVING_THRESHOLD 24   // 4 bytes, RW
#define DXL_REG_TEMPERATURE_LIMIT 31  // 1 byte,  RW
#define DXL_REG_MAX_VOLTAGE_LIMIT 32  // 2 bytes, RW
#define DXL_REG_MIN_VOLTAGE_LIMIT 34  // 2 bytes, RW
#define DXL_REG_PWM_LIMIT 36          // 2 bytes, RW
#define DXL_REG_VELOCITY_LIMIT 44     // 4 bytes, RW
#define DXL_REG_MAX_POSITION_LIMIT 48 // 4 bytes, RW
#define DXL_REG_MIN_POSITION_LIMIT 52 // 4 bytes, RW
#define DXL_REG_SHUTDOWN 63           // 1 byte,  RW

// ---- RAM Area (volatile, available while powered) ----

#define DXL_REG_TORQUE_ENABLE 64         // 1 byte,  RW
#define DXL_REG_LED 65                   // 1 byte,  RW
#define DXL_REG_STATUS_RETURN_LEVEL 68   // 1 byte,  RW
#define DXL_REG_REGISTERED_INST 69       // 1 byte,  RO
#define DXL_REG_HARDWARE_ERROR 70        // 1 byte,  RO
#define DXL_REG_VELOCITY_I_GAIN 76       // 2 bytes, RW
#define DXL_REG_VELOCITY_P_GAIN 78       // 2 bytes, RW
#define DXL_REG_POSITION_D_GAIN 80       // 2 bytes, RW
#define DXL_REG_POSITION_I_GAIN 82       // 2 bytes, RW
#define DXL_REG_POSITION_P_GAIN 84       // 2 bytes, RW
#define DXL_REG_FF_2ND_GAIN 88           // 2 bytes, RW
#define DXL_REG_FF_1ST_GAIN 90           // 2 bytes, RW
#define DXL_REG_BUS_WATCHDOG 98          // 1 byte,  RW
#define DXL_REG_GOAL_PWM 100             // 2 bytes, RW
#define DXL_REG_GOAL_VELOCITY 104        // 4 bytes, RW
#define DXL_REG_PROFILE_ACCELERATION 108 // 4 bytes, RW
#define DXL_REG_PROFILE_VELOCITY 112     // 4 bytes, RW
#define DXL_REG_GOAL_POSITION 116        // 4 bytes, RW
#define DXL_REG_REALTIME_TICK 120        // 2 bytes, RO
#define DXL_REG_MOVING 122               // 1 byte,  RO
#define DXL_REG_MOVING_STATUS 123        // 1 byte,  RO
#define DXL_REG_PRESENT_PWM 124          // 2 bytes, RO
#define DXL_REG_PRESENT_LOAD 126         // 2 bytes, RO
#define DXL_REG_PRESENT_VELOCITY 128     // 4 bytes, RO
#define DXL_REG_PRESENT_POSITION 132     // 4 bytes, RO
#define DXL_REG_VELOCITY_TRAJECTORY 136  // 4 bytes, RO
#define DXL_REG_POSITION_TRAJECTORY 140  // 4 bytes, RO
#define DXL_REG_PRESENT_INPUT_VOLT 144   // 2 bytes, RO
#define DXL_REG_PRESENT_TEMPERATURE 146  // 1 byte,  RO

// ---- Position Limits ----

#define DXL_POSITION_MIN 0
#define DXL_POSITION_MAX 4095
#define DXL_POSITION_CENTER 2048

#define DXL_MAX_BAUD_INDEX 7
