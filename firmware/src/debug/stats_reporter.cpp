
#include <cstring>

#include <FreeRTOS.h>
#include <task.h>

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


void start_stats_reporter() {

    xTaskCreate(stats_reporter_task,
                "stats_reporter",
                256,
                nullptr,
                1,
                &stats_reporter_task_handle);
}

portTASK_FUNCTION(stats_reporter_task, pvParameters) {

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    for (EVER) {

        while (true) {

            char message[USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH];
            memset(&message, '\0', USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH);

            snprintf(message, USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH, "STATS\tHEAP_FREE %lu\tC_RECV %lu\tM_RECV %lu\tSENT %lu\tS_PARSE %lu\tF_PARSE %lu\tCHKFAIL %lu\tPOS_PROC %lu",
                     (unsigned long)xFreeHeapSpace,
                     (unsigned long)serial_characters_received,
                     (unsigned long)serial_messages_received,
                     (unsigned long)serial_messages_sent,
                     (unsigned long)successful_messages_parsed,
                     (unsigned long)failed_messages_parsed,
                     (unsigned long)checksum_errors,
                     (unsigned long)position_messages_processed);


            send_to_controller(message);

            vTaskDelay(pdMS_TO_TICKS(20000));
        }
    }
#pragma clang diagnostic pop
}
