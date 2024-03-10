
#pragma once

#include <FreeRTOS.h>
#include <task.h>


#include "controller/controller.h"

#include "controller-config.h"

/*
 * This file defines the functions that are connected to the I2C-based servo modules.
 */

void i2c_servo_module_init();
void i2c_servo_module_start();


/*
 * A task that handles talking to the I2C-based servo modules
 */
portTASK_FUNCTION_PROTO(i2c_servo_module_task, pvParameters);

