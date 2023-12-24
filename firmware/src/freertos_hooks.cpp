
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


void vApplicationIdleHook( void )
{

    // Record the free heap space for the stats handler
    xFreeHeapSpace = xPortGetFreeHeapSize();

}

void vApplicationTickHook( void )
{
    // Nothing for now

}