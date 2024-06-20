
/**
 * A lot of the following is based on the sample code that MicroElektronika provides for the PAC1954.
 *
 * I've ported it to Pi Pico and FreeRTOS, and made some changes to make it more readable and to fit
 * my needs. I'm leaving their copyright notice in place because I really saved a lot of time by
 * using their sample code to bootstrap myself.
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




#include <FreeRTOS.h>

#include "pico/stdio.h"
#include "hardware/i2c.h"
#include "util/string_utils.h"


#include "logging/logging.h"

#include "controller-config.h"

#include "device/pac1954.h"


// From i2c.c
extern volatile bool i2c_setup_completed;



/**
 * @brief PAC1954 Measurement Full Scale Range Definition.
 * @details Specified full scale range for all measurements.
 */
#define PAC1954_FSR_VSOURCE_V  32
#define PAC1954_FSR_ISENSE_A   25
#define PAC1954_FSR_PSENSE_W   800


void init_pac1954(u8 address) {

    debug("init'ing the PAC1954 at address 0x%02X (%s)", address, to_binary_string(address));

    configASSERT(i2c_setup_completed);

    // Gather some device information
    u8 device_id = pac1954_read_register_8bit(PAC1954_PRODUCT_ID_REGISTER);
    u8 manufacturer_id = pac1954_read_register_8bit(PAC1954_PRODUCT_ID_REGISTER);
    u8 revision_id = pac1954_read_register_8bit(PAC1954_REVISION_ID_REGISTER);

    info("pac1954 device id: 0x%02X, manufacturer id: 0x%02X, revision id: 0x%02X", device_id, manufacturer_id,
         revision_id);

    // Make sure we're talking to the pac1954
    configASSERT(device_id == I2c_DEVICE_PAC1954_PRODUCT_ID);

    // Set the PAC1954 to its default configuration
    set_pac1954_default_config();

}


float pac1954_read_power(u8 input_number) {
    float power;

    pac1954_get_calc_measurement(PAC1954_MEAS_SEL_P_SENSE,
                                 PAC1954_CH_SEL_CH_1 + input_number,
                                 PAC1954_AVG_SEL_ENABLE,
                                 PAC1954_MEAS_MODE_UNIPOLAR_FSR,
                                 &power);

    return power;

}

float pac1954_read_voltage(u8 input_number) {
    float voltage;

    pac1954_get_calc_measurement(PAC1954_MEAS_SEL_V_SOURCE,
                                 PAC1954_CH_SEL_CH_1 + input_number,
                                 PAC1954_AVG_SEL_ENABLE,
                                 PAC1954_MEAS_MODE_UNIPOLAR_FSR,
                                 &voltage);

    return voltage;
}

float pac1954_read_current(u8 input_number) {
    float current;

    pac1954_get_calc_measurement(PAC1954_MEAS_SEL_I_SENSE,
                                 PAC1954_CH_SEL_CH_1 + input_number,
                                 PAC1954_AVG_SEL_ENABLE,
                                 PAC1954_MEAS_MODE_UNIPOLAR_FSR,
                                 &current);

    return current;
}


void set_pac1954_default_config() {

    debug("setting the PAC1954 to its default configuration");

    u8 defaultConfig[2];

    defaultConfig[0] = PAC1954_CTRLH_SPS_1024_ADAPT_ACC |
                       PAC1954_CTRLH_INT_PIN_ALERT |
                       PAC1954_CTRLH_SLW_PIN_SLOW;
    defaultConfig[1] = PAC1954_CTRLL_ALL_CH_ON;

    i2c_write_blocking(SENSORS_I2C_BUS, I2C_DEVICE_PAC1954, defaultConfig, 2, true);

    defaultConfig[0] = (PAC1954_MEAS_MODE_BIPOLAR_FSR << PAC1954_NEG_PWR_FSR_CH1_OFFSET) |
                       (PAC1954_MEAS_MODE_BIPOLAR_HALF_FSR << PAC1954_NEG_PWR_FSR_CH2_OFFSET) |
                       (PAC1954_MEAS_MODE_UNIPOLAR_FSR << PAC1954_NEG_PWR_FSR_CH3_OFFSET) |
                       (PAC1954_MEAS_MODE_UNIPOLAR_FSR << PAC1954_NEG_PWR_FSR_CH4_OFFSET);
    defaultConfig[1] = (PAC1954_MEAS_MODE_BIPOLAR_FSR << PAC1954_NEG_PWR_FSR_CH1_OFFSET) |
                       (PAC1954_MEAS_MODE_BIPOLAR_HALF_FSR << PAC1954_NEG_PWR_FSR_CH2_OFFSET) |
                       (PAC1954_MEAS_MODE_UNIPOLAR_FSR << PAC1954_NEG_PWR_FSR_CH3_OFFSET) |
                       (PAC1954_MEAS_MODE_UNIPOLAR_FSR << PAC1954_NEG_PWR_FSR_CH4_OFFSET);

    i2c_write_blocking(SENSORS_I2C_BUS, I2C_DEVICE_PAC1954, defaultConfig, 2, true);

    info("PAC1954 has been set to its default configuration");
}


void pac1954_refresh(void) {
    pac1954_write_command(PAC1954_REFRESH_CMD);

    // TODO: In the sample code, there's a 1ms delay here.

}

void pac1954_vol_refresh(void) {
    pac1954_write_command(PAC1954_REG_REFRESH_V);

    // TODO: In the sample code, there's a 1ms delay here.

}


bool pac1954_get_measurement(u8 meas_sel, u8 ch_sel, u8 avg_sel, u32 *data_out) {
    u8 tmp_data[4] = {0};
    u8 n_bytes;

    switch (meas_sel) {
        case PAC1954_MEAS_SEL_V_SOURCE : {
            if (avg_sel == PAC1954_AVG_SEL_DISABLE) {
                tmp_data[0] = 0x06;
            } else {
                tmp_data[0] = 0x0E;
            }

            n_bytes = 2;
            break;
        }
        case PAC1954_MEAS_SEL_I_SENSE : {
            if (avg_sel == PAC1954_AVG_SEL_DISABLE) {
                tmp_data[0] = 0x0A;
            } else {
                tmp_data[0] = 0x12;
            }

            n_bytes = 2;
            break;
        }
        case PAC1954_MEAS_SEL_P_SENSE : {
            tmp_data[0] = 0x16;
            n_bytes = 4;
            break;
        }
        default : {
            warning("Invalid measurement selection");
            return false;
        }
    }

    if ((ch_sel < PAC1954_CH_SEL_CH_1) || (ch_sel > PAC1954_CH_SEL_CH_4)) {
        warning("Invalid channel selection");
        return false;
    }

    tmp_data[0] += ch_sel;


    pac1954_read_data(tmp_data[0], tmp_data, n_bytes);


    for (u8 cnt = 0; cnt < n_bytes; cnt++) {
        if (cnt == 3) {
            *data_out <<= 6;
            *data_out |= tmp_data[cnt] >> 2;
        } else {
            *data_out <<= 8;
            *data_out |= tmp_data[cnt];
        }
    }

    return true;
}


void pac1954_get_acc_count(u32 *data_out) {
    u8 tmp_data[4] = {0};

    tmp_data[0] = PAC1954_REG_ACC_COUNT;

    pac1954_read_data(tmp_data[0], tmp_data, 4);

    *data_out = tmp_data[0];
    *data_out <<= 8;
    *data_out |= tmp_data[1];
    *data_out <<= 8;
    *data_out |= tmp_data[2];
    *data_out <<= 8;
    *data_out |= tmp_data[3];
}


bool pac1954_get_acc_output(u8 ch_sel, u8 *data_out) {
    if ((ch_sel < PAC1954_CH_SEL_CH_1) || (ch_sel > PAC1954_CH_SEL_CH_4)) {
        warning("Invalid channel selection");
        return false;
    }

    pac1954_read_data(ch_sel + 0x02, data_out, 7);
    return true;

}

bool pac1954_get_calc_measurement(u8 meas_sel, u8 ch_sel, u8 avg_sel, u8 meas_mode, float *data_out) {
    u32 ret_val = 0;
    volatile int16_t ret_val_16_sign;
    volatile int32_t ret_val_32_sign;

    if ((ch_sel < PAC1954_CH_SEL_CH_1) || (ch_sel > PAC1954_CH_SEL_CH_4)) {
        warning("Invalid channel selection");
        return false;
    }

    bool error_flag = pac1954_get_measurement(meas_sel, ch_sel, avg_sel, &ret_val);

    switch (meas_sel) {
        case PAC1954_MEAS_SEL_V_SOURCE : {
            if (meas_mode == PAC1954_MEAS_MODE_UNIPOLAR_FSR) {
                *data_out = ret_val;
                *data_out /= 0x10000;
                *data_out *= PAC1954_FSR_VSOURCE_V;
            } else {
                ret_val_16_sign = (int16_t) ret_val;
                *data_out = ret_val_16_sign;
                *data_out /= 0x8000;
                *data_out *= PAC1954_FSR_VSOURCE_V;

                if (meas_mode != PAC1954_MEAS_MODE_BIPOLAR_FSR) {
                    *data_out /= 0x02;
                }
            }
            break;
        }
        case PAC1954_MEAS_SEL_I_SENSE : {
            if (meas_mode == PAC1954_MEAS_MODE_UNIPOLAR_FSR) {
                *data_out = ret_val;
                *data_out /= 0x10000;
                *data_out *= PAC1954_FSR_ISENSE_A;
            } else {
                ret_val_16_sign = (int16_t) ret_val;
                *data_out = ret_val_16_sign;
                *data_out /= 0x8000;
                *data_out *= PAC1954_FSR_ISENSE_A;

                if (meas_mode != PAC1954_MEAS_MODE_BIPOLAR_FSR) {
                    *data_out /= 0x02;
                }
            }
            break;
        }
        case PAC1954_MEAS_SEL_P_SENSE : {
            if (meas_mode == PAC1954_MEAS_MODE_UNIPOLAR_FSR) {
                *data_out = ret_val;
                *data_out /= 0x40000000;
                *data_out *= PAC1954_FSR_PSENSE_W;
            } else {
                ret_val_32_sign = (int32_t) ret_val;

                if (ret_val & 0x20000000) {
                    ret_val_32_sign |= 0xC0000000;
                }

                *data_out = ret_val_32_sign;
                *data_out /= 0x20000000;
                *data_out *= PAC1954_FSR_PSENSE_W;

                if (meas_mode != PAC1954_MEAS_MODE_BIPOLAR_FSR) {
                    *data_out /= 0x02;
                }
            }
            break;
        }
        default : {
            warning("Invalid measurement");
            return false;
        }
    }

    return true;
}


// Function to write a command to the PAC1954
void pac1954_write_command(u8 command) {
    configASSERT(i2c_setup_completed);

    i2c_write_blocking(SENSORS_I2C_BUS, I2C_DEVICE_PAC1954, &command, 1, true);
}

// Function to read data from the PAC1954
void pac1954_read_data(u8 reg, u8 *buffer, size_t length) {
    configASSERT(i2c_setup_completed);

    i2c_write_blocking(SENSORS_I2C_BUS, I2C_DEVICE_PAC1954, &reg, 1, true);
    i2c_read_blocking(SENSORS_I2C_BUS, I2C_DEVICE_PAC1954, buffer, length, false);
}


/**
 * Cheater method to read an 8-bit register from the PAC1954
 *
 * @param register_address The register to read
 * @return the value of the register
 */
u8 pac1954_read_register_8bit(u8 register_address) {
    u8 data;
    pac1954_read_data(register_address, &data, 1);
    return data;
}
