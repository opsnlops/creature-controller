/**
 * @file freertos_mocks.c
 * @brief Mock implementations of FreeRTOS functions for testing
 */

#include "freertos_mocks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- FreeRTOS Queue Mocks ---

QueueHandle_t xQueueCreateMock(UBaseType_t uxQueueLength, UBaseType_t uxItemSize) {
    QueueMock* queue = (QueueMock*)malloc(sizeof(QueueMock));
    if (queue == NULL) {
        return NULL;
    }

    queue->items = (u8*)malloc(uxQueueLength * uxItemSize);
    if (queue->items == NULL) {
        free(queue);
        return NULL;
    }

    queue->length = uxQueueLength;
    queue->itemSize = uxItemSize;
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    return (QueueHandle_t)queue;
}

void vQueueDeleteMock(QueueHandle_t xQueue) {
    if (xQueue == NULL) {
        return;
    }

    QueueMock* queue = (QueueMock*)xQueue;
    if (queue->items != NULL) {
        free(queue->items);
    }
    free(queue);
}

BaseType_t xQueueSendMock(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait) {
    if (xQueue == NULL || pvItemToQueue == NULL) {
        return pdFALSE;
    }

    QueueMock* queue = (QueueMock*)xQueue;
    if (queue->count >= queue->length) {
        return pdFALSE;
    }

    memcpy(queue->items + (queue->rear * queue->itemSize), pvItemToQueue, queue->itemSize);
    queue->rear = (queue->rear + 1) % queue->length;
    queue->count++;
    return pdTRUE;
}

BaseType_t xQueueReceiveMock(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait) {
    if (xQueue == NULL || pvBuffer == NULL) {
        return pdFALSE;
    }

    QueueMock* queue = (QueueMock*)xQueue;
    if (queue->count == 0) {
        return pdFALSE;
    }

    memcpy(pvBuffer, queue->items + (queue->front * queue->itemSize), queue->itemSize);
    queue->front = (queue->front + 1) % queue->length;
    queue->count--;
    return pdTRUE;
}

// --- FreeRTOS Task Mocks ---

BaseType_t xTaskCreateMock(TaskFunction_t pxTaskCode, const char* pcName,
                           configSTACK_DEPTH_TYPE usStackDepth, void* pvParameters,
                           UBaseType_t uxPriority, TaskHandle_t* pxCreatedTask) {
    // For testing, we just pretend task was created successfully
    if (pxCreatedTask != NULL) {
        *pxCreatedTask = (TaskHandle_t)1; // Dummy handle
    }
    return pdPASS;
}

// --- FreeRTOS Timer Mocks ---

TimerHandle_t xTimerCreateMock(const char* pcTimerName, TickType_t xTimerPeriod,
                               UBaseType_t uxAutoReload, void* pvTimerID,
                               TimerCallbackFunction_t pxCallbackFunction) {
    TimerMock* timer = (TimerMock*)malloc(sizeof(TimerMock));
    if (timer == NULL) {
        return NULL;
    }

    timer->period = xTimerPeriod;
    timer->callback = pxCallbackFunction;
    timer->timerID = pvTimerID;
    timer->autoReload = uxAutoReload;
    return (TimerHandle_t)timer;
}

BaseType_t xTimerStartMock(TimerHandle_t xTimer, TickType_t xTicksToWait) {
    if (xTimer == NULL) {
        return pdFALSE;
    }
    return pdPASS;
}

BaseType_t xTimerStopMock(TimerHandle_t xTimer, TickType_t xTicksToWait) {
    if (xTimer == NULL) {
        return pdFALSE;
    }
    return pdPASS;
}

void vTimerDeleteMock(TimerHandle_t xTimer) {
    if (xTimer != NULL) {
        free(xTimer);
    }
}

// --- Critical Section Mocks ---

void vTaskEnterCriticalMock(void) {
    // Just a stub for testing
}

void vTaskExitCriticalMock(void) {
    // Just a stub for testing
}

// --- Utility Functions ---

void resetQueueMock(QueueHandle_t xQueue) {
    if (xQueue == NULL) {
        return;
    }

    QueueMock* queue = (QueueMock*)xQueue;
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
}