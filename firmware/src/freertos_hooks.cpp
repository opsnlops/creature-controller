
#include <FreeRTOS.h>
#include <task.h>

#include <cstdio>


// Keep track of the amount of free heap space
extern volatile size_t xFreeHeapSpace;


extern "C" void vApplicationMallocFailedHook( void )
{
    /* Force an assert. */
    configASSERT( ( volatile void * ) nullptr );
}



void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {

    // Halt the system on a stack overflow.
    taskDISABLE_INTERRUPTS();

    // Log the overflow. Replace this with your logging method.
    printf("Stack overflow in task: %s\n", pcTaskName);

    // Assert to halt the system
    configASSERT(true);
}

void vApplicationIdleHook( void )
{

    // Record the free heap space for the stats handler
    xFreeHeapSpace = xPortGetFreeHeapSize();

}

void vApplicationTickHook( void )
{
    // Nothing for now

}