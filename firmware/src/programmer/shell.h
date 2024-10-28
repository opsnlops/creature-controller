
#pragma once

#include <FreeRTOS.h>
#include <task.h>


portTASK_FUNCTION_PROTO(shell_task, pvParameters);

enum ProgrammerState {
    IDLE,
    FILLING_BUFFER,
    FLASHING,
    VERIFYING,
    ERROR,
};


void handle_shell_command(u8 *command);
void launch_shell();
void terminate_shell();

void reset_incoming_buffer();
void reset_request_buffer();
void send_response(const char *response);

