
#pragma once


#include <stdio.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include "logging/logging.h"


#include "controller-config.h"

// PAC1954 Register addresses
#define PAC1954_REFRESH_CMD 0x00

#define PAC1954_PRODUCT_ID_REGISTER         0xFD
#define PAC1954_MANUFACTURER_ID_REGISTER    0xFE
#define PAC1954_REVISION_ID_REGISTER        0xFF


void init_pac1954(u8 address);

float pac1954_read_voltage(u8 device_address, u8 input_number);
float pac1954_read_current(u8 device_address, u8 input_number);
float pac1954_read_power(u8 device_address, u8 input_number);


void pac1954_write_command(u8 device_address, u8 command);
void pac1954_read_data(u8 device_address, u8 reg, u8 *buffer, size_t length);
u8 pac1954_read_register_8bit(u8 device_address, u8 register_address);


/**
 * The following definitions are taken from the MikroElektronika PAC1954 Click driver
 *
 * I bought one of those to help me bootstrap the development of the PAC1954 driver, and
 * these are handy to have. I'm not sure I need this copyright notice since these are just
 * values off the datasheet, but I'm gonna leave it since they did really help me
 * get started with this chip! 😍
 */


/****************************************************************************
** Copyright (C) 2020 MikroElektronika d.o.o.
** Contact: https://www.mikroe.com/contact
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
** OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
** DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
** OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
**  USE OR OTHER DEALINGS IN THE SOFTWARE.
****************************************************************************/



typedef struct {
    float voltage;
    float current;
    float power;
} sensor_power_data_t;




void set_pac1954_default_config(u8 device_address);

void pac1954_refresh(u8 device_address);

void pac1954_vol_refresh(u8 device_address);

bool pac1954_get_measurement(u8 device_address, u8 meas_sel, u8 ch_sel, u8 avg_sel, u32 *data_out);

void pac1954_get_acc_count(u8 device_address, u32 *data_out);

bool pac1954_get_acc_output(u8 device_address, u8 ch_sel, u8 *data_out);

bool pac1954_get_calc_measurement(u8 device_address, u8 meas_sel, u8 ch_sel, u8 avg_sel, u8 meas_mode, float *data_out);



/**
 * @brief PAC1954 Registers List.
 * @details List of registers of PAC1954 Click driver.
 */
#define PAC1954_REG_REFRESH                   0x00
#define PAC1954_REG_CTRL                      0x01
#define PAC1954_REG_ACC_COUNT                 0x02
#define PAC1954_REG_VACC_CH1                  0x03
#define PAC1954_REG_VACC_CH2                  0x04
#define PAC1954_REG_VACC_CH3                  0x05
#define PAC1954_REG_VACC_CH4                  0x06
#define PAC1954_REG_VBUS_CH1                  0x07
#define PAC1954_REG_VBUS_CH2                  0x08
#define PAC1954_REG_VBUS_CH3                  0x09
#define PAC1954_REG_VBUS_CH4                  0x0A
#define PAC1954_REG_VSENSE_CH1                0x0B
#define PAC1954_REG_VSENSE_CH2                0x0C
#define PAC1954_REG_VSENSE_CH3                0x0D
#define PAC1954_REG_VSENSE_CH4                0x0E
#define PAC1954_REG_VBUS_CH1_AVG              0x0F
#define PAC1954_REG_VBUS_CH2_AVG              0x10
#define PAC1954_REG_VBUS_CH3_AVG              0x11
#define PAC1954_REG_VBUS_CH4_AVG              0x12
#define PAC1954_REG_VSENSE_CH1_AVG            0x13
#define PAC1954_REG_VSENSE_CH2_AVG            0x14
#define PAC1954_REG_VSENSE_CH3_AVG            0x15
#define PAC1954_REG_VSENSE_CH4_AVG            0x16
#define PAC1954_REG_VPOWER_CH1                0x17
#define PAC1954_REG_VPOWER_CH2                0x18
#define PAC1954_REG_VPOWER_CH3                0x19
#define PAC1954_REG_VPOWER_CH4                0x1A
#define PAC1954_REG_SMBUS_CFG                 0x1C
#define PAC1954_REG_NEG_PWR_FSR               0x1D
#define PAC1954_REG_REFRESH_G                 0x1E
#define PAC1954_REG_REFRESH_V                 0x1F
#define PAC1954_REG_SLOW                      0x20
#define PAC1954_REG_CTRL_ACT                  0x21
#define PAC1954_REG_NEG_PWR_FSR_ACT           0x22
#define PAC1954_REG_CTRL_LAT                  0x23
#define PAC1954_REG_NEG_PWR_FSR_LAT           0x24
#define PAC1954_REG_ACC_CFG                   0x25
#define PAC1954_REG_ALERT_STATUS              0x26
#define PAC1954_REG_SLOW_ALERT1               0x27
#define PAC1954_REG_GPIO_ALERT2               0x28
#define PAC1954_REG_ACC_FULLNESS_LIM          0x29
#define PAC1954_REG_OC_LIM_CH1                0x30
#define PAC1954_REG_OC_LIM_CH2                0x31
#define PAC1954_REG_OC_LIM_CH3                0x32
#define PAC1954_REG_OC_LIM_CH4                0x33
#define PAC1954_REG_UC_LIM_CH1                0x34
#define PAC1954_REG_UC_LIM_CH2                0x35
#define PAC1954_REG_UC_LIM_CH3                0x36
#define PAC1954_REG_UC_LIM_CH4                0x37
#define PAC1954_REG_OP_LIM_CH1                0x38
#define PAC1954_REG_OP_LIM_CH2                0x39
#define PAC1954_REG_OP_LIM_CH3                0x3A
#define PAC1954_REG_OP_LIM_CH4                0x3B
#define PAC1954_REG_OV_LIM_CH1                0x3C
#define PAC1954_REG_OV_LIM_CH2                0x3D
#define PAC1954_REG_OV_LIM_CH3                0x3E
#define PAC1954_REG_OV_LIM_CH4                0x3F
#define PAC1954_REG_UV_LIM_CH1                0x40
#define PAC1954_REG_UV_LIM_CH2                0x41
#define PAC1954_REG_UV_LIM_CH3                0x42
#define PAC1954_REG_UV_LIM_CH4                0x43
#define PAC1954_REG_OC_LIM_NSAMPLES           0x44
#define PAC1954_REG_UC_LIM_NSAMPLES           0x45
#define PAC1954_REG_OP_LIM_NSAMPLES           0x46
#define PAC1954_REG_OV_LIM_NSAMPLES           0x47
#define PAC1954_REG_UV_LIM_NSAMPLES           0x48
#define PAC1954_REG_ALERT_ENABLE              0x49
#define PAC1954_REG_ACC_CFG_ACT               0x4A
#define PAC1954_REG_ACC_CFG_LAT               0x4B
#define PAC1954_REG_ID_PRODUCT                0xFD
#define PAC1954_REG_ID_MANUFACTURER           0xFE
#define PAC1954_REG_ID_REVISION               0xFF

/**
 * @brief PAC1954 Control Settings.
 * @details Specified setting for the CTRL register of PAC1954 Click driver.
 */
#define PAC1954_CTRLH_SPS_1024_ADAPT_ACC      0x00
#define PAC1954_CTRLH_SPS_256_ADAPT_ACC       0x10
#define PAC1954_CTRLH_SPS_64_ADAPT_ACC        0x20
#define PAC1954_CTRLH_SPS_8_ADAPT_ACC         0x30
#define PAC1954_CTRLH_SPS_1024                0x40
#define PAC1954_CTRLH_SPS_256                 0x50
#define PAC1954_CTRLH_SPS_64                  0x60
#define PAC1954_CTRLH_SPS_8                   0x70
#define PAC1954_CTRLH_SINGLE_SHOT_MODE        0x80
#define PAC1954_CTRLH_SINGLE_SHOT_8X          0x90
#define PAC1954_CTRLH_FAST_MODE               0xA0
#define PAC1954_CTRLH_BURST_MODE              0xB0
#define PAC1954_CTRLH_SLEEP                   0xF0
#define PAC1954_CTRLH_INT_PIN_ALERT           0x00
#define PAC1954_CTRLH_INT_PIN_DIG_IN          0x04
#define PAC1954_CTRLH_INT_PIN_DIG_OUT         0x08
#define PAC1954_CTRLH_INT_PIN_SLOW            0x0C
#define PAC1954_CTRLH_SLW_PIN_ALERT           0x00
#define PAC1954_CTRLH_SLW_PIN_DIG_IN          0x01
#define PAC1954_CTRLH_SLW_PIN_DIG_OUT         0x02
#define PAC1954_CTRLH_SLW_PIN_SLOW            0x03
#define PAC1954_CTRLL_CH1_OFF                 0x80
#define PAC1954_CTRLL_CH2_OFF                 0x40
#define PAC1954_CTRLL_CH3_OFF                 0x20
#define PAC1954_CTRLL_CH4_OFF                 0x10
#define PAC1954_CTRLL_ALL_CH_OFF              0xF0
#define PAC1954_CTRLL_ALL_CH_ON               0x00

/**
 * @brief PAC1954 Measurement Type Offset For Channels.
 * @details Specified setting for NEG_PWR_FSR register of PAC1954 Click driver.
 */
#define PAC1954_NEG_PWR_FSR_CH1_OFFSET        6
#define PAC1954_NEG_PWR_FSR_CH2_OFFSET        4
#define PAC1954_NEG_PWR_FSR_CH3_OFFSET        2
#define PAC1954_NEG_PWR_FSR_CH4_OFFSET        0

/**
 * @brief PAC1954 SMBus Settings.
 * @details Specified setting for SMBUS SETTINGS register of PAC1954 Click driver.
 */
#define PAC1954_SMBUS_INT_PIN_MASK            0x80
#define PAC1954_SMBUS_SLW_PIN_MASK            0x40
#define PAC1954_SMBUS_ALERT_MASK              0x20
#define PAC1954_SMBUS_POR_MASK                0x10
#define PAC1954_SMBUS_TIMEOUT_OFF             0x00
#define PAC1954_SMBUS_TIMEOUT_ON              0x08
#define PAC1954_SMBUS_BYTE_COUNT_OFF          0x00
#define PAC1954_SMBUS_BYTE_COUNT_ON           0x04
#define PAC1954_SMBUS_AUTO_INC_SKIP_ON        0x00
#define PAC1954_SMBUS_AUTO_INC_SKIP_OFF       0x02
#define PAC1954_SMBUS_I2C_HIGH_SPEED          0x01

/**
 * @brief PAC1954 Measurement Data Selector.
 * @details Measurement data selection of PAC1954 Click driver.
 */
#define PAC1954_MEAS_SEL_V_SOURCE             0
#define PAC1954_MEAS_SEL_I_SENSE              1
#define PAC1954_MEAS_SEL_P_SENSE              2

/**
 * @brief PAC1954 Channel Selector.
 * @details Channel selection of PAC1954 Click driver.
 */
#define PAC1954_CH_SEL_CH_1                   1
#define PAC1954_CH_SEL_CH_2                   2
#define PAC1954_CH_SEL_CH_3                   3
#define PAC1954_CH_SEL_CH_4                   4

/**
 * @brief PAC1954 Average Selector.
 * @details Selection for averaged or single data used in calculations of PAC1954 Click driver.
 */
#define PAC1954_AVG_SEL_DISABLE               0
#define PAC1954_AVG_SEL_ENABLE                1

/**
 * @brief PAC1954 Measurement Type Definition.
 * @details Type of measurement data of PAC1954 Click driver.
 */
#define PAC1954_MEAS_MODE_UNIPOLAR_FSR        0
#define PAC1954_MEAS_MODE_BIPOLAR_FSR         1
#define PAC1954_MEAS_MODE_BIPOLAR_HALF_FSR    2

/**
 * @brief PAC1954 SLW Pin Settings.
 * @details Settings for the Slow pin of PAC1954 Click driver.
 */
#define PAC1954_ALL_CH_SAMPLE_8SPS_ON         1
#define PAC1954_ALL_CH_SAMPLE_8SPS_OFF        0

/**
 * @brief PAC1954 Device Power Control.
 * @details Power control using EN pin of PAC1954 Click driver.
 */
#define PAC1954_DEV_ENABLE                    1
#define PAC1954_DEV_PWR_DWN                   0

/**
 * @brief PAC1954 Alert Indicator.
 * @details Alert indicator of PAC1954 Click driver.
 */
#define PAC1954_ALERT_ACTIVE                  0
#define PAC1954_ALERT_INACTIVE                1

/**
 * @brief PAC1954 device address setting.
 * @details Specified setting for device slave address selection of
 * PAC1954 Click driver.
 */
#define PAC1954_DEV_ADDR_0                    0x10
#define PAC1954_DEV_ADDR_1                    0x1F
