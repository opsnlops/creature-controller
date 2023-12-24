
#include <cstring>

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>

#include "debug/stats_reporter.h"
#include "io/usb_serial.h"

#include "tasks.h"

extern TaskHandle_t stats_reporter_task_handle;

// Things to collect
extern volatile size_t xFreeHeapSpace;
extern volatile u64 serial_messages_received;
extern volatile u64 serial_characters_received;
extern volatile u64 serial_messages_sent;
extern volatile u64 successful_messages_parsed;
extern volatile u64 failed_messages_parsed;
extern volatile u64 checksum_errors;
extern volatile u64 position_messages_processed;
extern volatile u64 number_of_pwm_wraps;

namespace creatures::debug {

    void start_stats_reporter() {

        TimerHandle_t statsReportTimer = xTimerCreate(
                "StatsReportTimer",              // Timer name
                pdMS_TO_TICKS(20000),            // Timer period (20 seconds)
                pdTRUE,                          // Auto-reload
                (void *) 0,                        // Timer ID (not used here)
                statsReportTimerCallback         // Callback function
        );

        if (statsReportTimer != nullptr) {
            xTimerStart(statsReportTimer, 0); // Start timer
        }
    }

    void statsReportTimerCallback(TimerHandle_t xTimer) {

        char message[USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH];
        memset(&message, '\0', USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH);

        snprintf(message, USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH,
                 "STATS\tHEAP_FREE %lu\tC_RECV %lu\tM_RECV %lu\tSENT %lu\tS_PARSE %lu\tF_PARSE %lu\tCHKFAIL %lu\tPOS_PROC %lu\tPWM_WRAPS %lu",
                 (unsigned long) xFreeHeapSpace,
                 (unsigned long) serial_characters_received,
                 (unsigned long) serial_messages_received,
                 (unsigned long) serial_messages_sent,
                 (unsigned long) successful_messages_parsed,
                 (unsigned long) failed_messages_parsed,
                 (unsigned long) checksum_errors,
                 (unsigned long) position_messages_processed,
                 (unsigned long) number_of_pwm_wraps);

        send_to_controller(message);
    }

} // creatures::debug