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
    printf("[FREERTOS MOCK] Creating queue: length=%lu, item_size=%lu\n",
           (unsigned long)uxQueueLength, (unsigned long)uxItemSize);

    QueueMock* queue = (QueueMock*)malloc(sizeof(QueueMock));
    if (queue == NULL) {
        printf("[FREERTOS MOCK] ERROR: Failed to allocate queue structure\n");
        return NULL;
    }

    queue->items = (u8*)malloc(uxQueueLength * uxItemSize);
    if (queue->items == NULL) {
        printf("[FREERTOS MOCK] ERROR: Failed to allocate queue items\n");
        free(queue);
        return NULL;
    }

    queue->length = uxQueueLength;
    queue->itemSize = uxItemSize;
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;

    // Initialize memory to zeros
    memset(queue->items, 0, uxQueueLength * uxItemSize);

    return (QueueHandle_t)queue;
}

void vQueueDeleteMock(QueueHandle_t xQueue) {
    printf("[FREERTOS MOCK] Deleting queue: %p\n", xQueue);

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
    if (xQueue == NULL) {
        printf("[FREERTOS MOCK] ERROR: Queue send to NULL queue\n");
        return pdFALSE;
    }

    if (pvItemToQueue == NULL) {
        printf("[FREERTOS MOCK] ERROR: Queue send NULL item\n");
        return pdFALSE;
    }

    QueueMock* queue = (QueueMock*)xQueue;
    if (queue->count >= queue->length) {
        printf("[FREERTOS MOCK] ERROR: Queue full on send\n");
        return pdFALSE;
    }

    memcpy(queue->items + (queue->rear * queue->itemSize), pvItemToQueue, queue->itemSize);
    queue->rear = (queue->rear + 1) % queue->length;
    queue->count++;

    printf("[FREERTOS MOCK] Queue send: count=%lu\n", (unsigned long)queue->count);
    return pdTRUE;
}

BaseType_t xQueueReceiveMock(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait) {
    if (xQueue == NULL) {
        printf("[FREERTOS MOCK] ERROR: Queue receive from NULL queue\n");
        return pdFALSE;
    }

    if (pvBuffer == NULL) {
        printf("[FREERTOS MOCK] ERROR: Queue receive to NULL buffer\n");
        return pdFALSE;
    }

    QueueMock* queue = (QueueMock*)xQueue;
    if (queue->count == 0) {
        printf("[FREERTOS MOCK] ERROR: Queue empty on receive\n");
        return pdFALSE;
    }

    memcpy(pvBuffer, queue->items + (queue->front * queue->itemSize), queue->itemSize);
    queue->front = (queue->front + 1) % queue->length;
    queue->count--;

    printf("[FREERTOS MOCK] Queue receive: count=%lu\n", (unsigned long)queue->count);
    return pdTRUE;
}

// --- FreeRTOS Task Mocks ---

BaseType_t xTaskCreateMock(TaskFunction_t pxTaskCode, const char* pcName,
                           configSTACK_DEPTH_TYPE usStackDepth, void* pvParameters,
                           UBaseType_t uxPriority, TaskHandle_t* pxCreatedTask) {
    printf("[FREERTOS MOCK] Creating task: %s\n", pcName ? pcName : "unnamed");

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
    printf("[FREERTOS MOCK] Creating timer: %s\n", pcTimerName ? pcTimerName : "unnamed");

    TimerMock* timer = (TimerMock*)malloc(sizeof(TimerMock));
    if (timer == NULL) {
        printf("[FREERTOS MOCK] ERROR: Failed to allocate timer\n");
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
        printf("[FREERTOS MOCK] ERROR: Starting NULL timer\n");
        return pdFALSE;
    }

    printf("[FREERTOS MOCK] Starting timer: %p\n", xTimer);
    return pdPASS;
}

BaseType_t xTimerStopMock(TimerHandle_t xTimer, TickType_t xTicksToWait) {
    if (xTimer == NULL) {
        printf("[FREERTOS MOCK] ERROR: Stopping NULL timer\n");
        return pdFALSE;
    }

    printf("[FREERTOS MOCK] Stopping timer: %p\n", xTimer);
    return pdPASS;
}

void vTimerDeleteMock(TimerHandle_t xTimer) {
    printf("[FREERTOS MOCK] Deleting timer: %p\n", xTimer);

    if (xTimer != NULL) {
        free(xTimer);
    }
}

// --- Critical Section Mocks ---

void vTaskEnterCriticalMock(void) {
    printf("[FREERTOS MOCK] Enter critical section\n");
}

void vTaskExitCriticalMock(void) {
    printf("[FREERTOS MOCK] Exit critical section\n");
}

// --- Utility Functions ---

void resetQueueMock(QueueHandle_t xQueue) {
    if (xQueue == NULL) {
        printf("[FREERTOS MOCK] ERROR: Reset on NULL queue\n");
        return;
    }

    QueueMock* queue = (QueueMock*)xQueue;
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;

    printf("[FREERTOS MOCK] Reset queue: %p\n", xQueue);
}