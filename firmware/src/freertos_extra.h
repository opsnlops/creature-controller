
#pragma once

#include <FreeRTOS.h>
#include <task.h>

void vApplicationMallocFailedHook( void );
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
void vApplicationIdleHook( void );

void vApplicationTickHook( void );
