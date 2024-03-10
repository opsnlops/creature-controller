
#pragma once

#include <FreeRTOS.h>
#include <task.h>


#include "controller/controller.h"
#include "controller-config.h"

/**
 * Note to myself!
 *
 * This is abandoned. I spent a lot of time trying to make this work as I pleased, but
 * I could not get a perfect 50.00Hz out of the PCA9685. It would vary by a few Hz, often
 * as high as 54Hz.
 *
 * The PCA9685 is a 12-bit PWM controller, and it's not really designed for servos. It's
 * for LED lights. I gave it a shot because Adafruit makes it, but in the end, it's not
 * the right tool for this job.
 *
 * I'm leaving this file here as a reminder to myself that I tried, but and it didn't work
 * out like I'd hoped.
 *
 * March, 2024
 */



/*
 * This file defines the functions that are connected to the I2C-based servo modules.
 */

void i2c_servo_module_init();
void i2c_servo_module_start();


/*
 * A task that handles talking to the I2C-based servo modules
 */
portTASK_FUNCTION_PROTO(i2c_servo_module_task, pvParameters);

