
#pragma once


#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>


void message_processor_init();
void message_processor_start();





/**
 * Send a message to the controller via whatever interface is in use at the time
 *
 * @param message
 * @return
 */
bool send_to_controller(const char *message);




/**
 * Reads the incoming queue and processes messages
 *
 * @param pvParameters
 */
portTASK_FUNCTION_PROTO(incoming_message_processor_task, pvParameters);

/**
 * Reads the outgoing queue and passes messages along to the senders
 *
 * @param pvParameters
 */
portTASK_FUNCTION_PROTO(outgoing_message_processor_task, pvParameters);
