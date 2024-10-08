
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>

#include "controller/controller.h"
#include "debug/stats_reporter.h"
#include "io/message_processor.h"

extern TaskHandle_t stats_reporter_task_handle;

// Things to collect
extern volatile size_t xFreeHeapSpace;
extern volatile u64 successful_messages_parsed;
extern volatile u64 failed_messages_parsed;
extern volatile u64 checksum_errors;
extern volatile u64 position_messages_processed;
extern volatile u64 number_of_pwm_wraps;


// From the message processor
extern volatile u64 message_processor_messages_received;
extern volatile u64 message_processor_messages_sent;


// From the UART
extern volatile u64 uart_characters_received;
extern volatile u64 uart_messages_received;
extern volatile u64 uart_messages_sent;


// From the USB pipeline
extern volatile u64 usb_serial_characters_received;

// From the USB worker
extern volatile u64 usb_serial_messages_received;
extern volatile u64 usb_serial_messages_sent;

// From the Sensors
extern double board_temperature;

void start_stats_reporter() {

    TimerHandle_t statsReportTimer = xTimerCreate(
            "StatsReportTimer",              // Timer name
            pdMS_TO_TICKS(20 * 1000),            // Timer period (20 seconds)
            pdTRUE,                          // Auto-reload
            (void *) 0,                        // Timer ID (not used here)
            statsReportTimerCallback         // Callback function
    );

    if (statsReportTimer != NULL) {
        xTimerStart(statsReportTimer, 0); // Start timer
    }
}

void statsReportTimerCallback(TimerHandle_t xTimer) {

    char message[USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH];
    memset(&message, '\0', USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH);

    snprintf(message, USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH,
             "STATS\tHEAP_FREE %lu\tUSB_CRECV %lu\tUSB_MRECV %lu\tUSB_SENT %lu\tUART_CRECV %lu\tUART_MRECV %lu\tUART_SENT %lu\tMP_RECV %lu\tMP_SENT %lu\tS_PARSE %lu\tF_PARSE %lu\tCHKFAIL %lu\tPOS_PROC %lu\tPWM_WRAPS %lu\tTEMP %.2f",
             (unsigned long) xFreeHeapSpace,
             (unsigned long) usb_serial_characters_received,
             (unsigned long) usb_serial_messages_received,
             (unsigned long) usb_serial_messages_sent,
             (unsigned long) uart_characters_received,
             (unsigned long) uart_messages_received,
             (unsigned long) uart_messages_sent,
             (unsigned long) message_processor_messages_received,
             (unsigned long) message_processor_messages_sent,
             (unsigned long) successful_messages_parsed,
             (unsigned long) failed_messages_parsed,
             (unsigned long) checksum_errors,
             (unsigned long) position_messages_processed,
             (unsigned long) number_of_pwm_wraps,
             board_temperature);

    send_to_controller(message);

}
