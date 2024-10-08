

#include <stddef.h>
#include <stdio.h>


#include <FreeRTOS.h>
#include <task.h>

#include "types.h"


// Keep track of the amount of free heap space
extern volatile size_t xFreeHeapSpace;


void vApplicationMallocFailedHook(void) {
    /* Force an assert. */
    configASSERT((volatile void *) NULL);
}


void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {

    // Halt the system on a stack overflow.
    taskDISABLE_INTERRUPTS();

    // Print to the console that something really bad happened
    printf("Stack overflow in task: %s\n", pcTaskName);

    // Assert to halt the system
    configASSERT((volatile void *) NULL);
}

void vApplicationIdleHook(void) {

    // Record the free heap space for the stats handler
    xFreeHeapSpace = xPortGetFreeHeapSize();

}

void vApplicationTickHook(void) {
    // Nothing for now

}