
#pragma once

#include <FreeRTOS.h>
#include <task.h>


#include "message_processor.h"

#include "logging/logging.h"


#include "controller/config.h"

void uart_serial_init();
void uart_serial_start();

bool is_safe_to_enqueue_incoming_uart_serial();
bool is_safe_to_enqueue_outgoing_uart_serial();

void __isr serial_reader_isr();

portTASK_FUNCTION_PROTO(incoming_uart_serial_reader_task, pvParameters);
portTASK_FUNCTION_PROTO(outgoing_uart_serial_writer_task, pvParameters);
