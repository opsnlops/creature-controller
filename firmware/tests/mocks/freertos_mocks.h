/**
 * @file freertos_mocks.h
 * @brief Mock implementation of FreeRTOS functions for testing
 */

#pragma once

#include <stdlib.h>
#include "types.h"

// --- FreeRTOS Type Definitions ---

typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
typedef void* SemaphoreHandle_t;
typedef void* TimerHandle_t;
typedef void* EventGroupHandle_t;

typedef unsigned long UBaseType_t;
typedef long BaseType_t;
typedef unsigned long TickType_t;
typedef u32 configSTACK_DEPTH_TYPE;

#define pdFALSE     ( ( BaseType_t ) 0 )
#define pdTRUE      ( ( BaseType_t ) 1 )
#define pdPASS      ( pdTRUE )
#define pdFAIL      ( pdFALSE )

// Function pointer types
typedef void (*TaskFunction_t)(void*);
typedef void (*TimerCallbackFunction_t)(TimerHandle_t);

// Mock structures for internal representation
typedef struct {
    u8* items;
    UBaseType_t length;
    UBaseType_t itemSize;
    UBaseType_t front;
    UBaseType_t rear;
    UBaseType_t count;
} QueueMock;

typedef struct {
    TickType_t period;
    TimerCallbackFunction_t callback;
    void* timerID;
    UBaseType_t autoReload;
} TimerMock;

// --- FreeRTOS Queue Functions ---
QueueHandle_t xQueueCreateMock(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);
void vQueueDeleteMock(QueueHandle_t xQueue);
BaseType_t xQueueSendMock(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait);
BaseType_t xQueueReceiveMock(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait);

// --- FreeRTOS Task Functions ---
BaseType_t xTaskCreateMock(TaskFunction_t pxTaskCode, const char* pcName,
                           configSTACK_DEPTH_TYPE usStackDepth, void* pvParameters,
                           UBaseType_t uxPriority, TaskHandle_t* pxCreatedTask);

// --- FreeRTOS Timer Functions ---
TimerHandle_t xTimerCreateMock(const char* pcTimerName, TickType_t xTimerPeriod,
                               UBaseType_t uxAutoReload, void* pvTimerID,
                               TimerCallbackFunction_t pxCallbackFunction);
BaseType_t xTimerStartMock(TimerHandle_t xTimer, TickType_t xTicksToWait);
BaseType_t xTimerStopMock(TimerHandle_t xTimer, TickType_t xTicksToWait);
void vTimerDeleteMock(TimerHandle_t xTimer);

// --- Critical Section Functions ---
void vTaskEnterCriticalMock(void);
void vTaskExitCriticalMock(void);

// --- Utility Functions ---
void resetQueueMock(QueueHandle_t xQueue);

// --- Macros to replace FreeRTOS functions with mocks ---
#define xQueueCreate xQueueCreateMock
#define vQueueDelete vQueueDeleteMock
#define xQueueSend xQueueSendMock
#define xQueueReceive xQueueReceiveMock
#define xTaskCreate xTaskCreateMock
#define xTimerCreate xTimerCreateMock
#define xTimerStart xTimerStartMock
#define xTimerStop xTimerStopMock
#define vTimerDelete vTimerDeleteMock
#define vTaskEnterCritical vTaskEnterCriticalMock
#define vTaskExitCritical vTaskExitCriticalMock

// Additional FreeRTOS macros needed by the firmware
#define portMAX_DELAY ((TickType_t)0xFFFFFFFF)
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(ms) (ms)
#define configASSERT(x) if(!(x)) {printf("configASSERT FAILED: %s, line %d\n", __FILE__, __LINE__);}
