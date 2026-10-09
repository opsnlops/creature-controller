function(check_cli expected_success expected_text)
    execute_process(COMMAND "${CONTROLLER}" ${ARGN}
                    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
                    TIMEOUT 10)
    if(expected_success AND NOT result STREQUAL "0")
        message(FATAL_ERROR "Device discovery failed: ${result}\n${output}\n${error}")
    elseif(NOT expected_success AND result STREQUAL "0")
        message(FATAL_ERROR "Invalid invocation unexpectedly succeeded: ${ARGN}")
    endif()
    string(FIND "${output}\n${error}" "${expected_text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Expected '${expected_text}':\n${output}\n${error}")
    endif()
endfunction()

check_cli(TRUE "audio devices" --list-sound-devices)
check_cli(TRUE "List of network devices:" --list-network-devices)
# Discovery ignores config paths and never attempts to read them.
check_cli(TRUE "audio devices" --list-sound-devices --config /nonexistent/controller.json)
check_cli(TRUE "List of network devices:" --list-network-devices --creature-config /nonexistent/creature.json)
check_cli(FALSE "--config: required")
check_cli(FALSE "--creature-config: required" --config /nonexistent/controller.json)
check_cli(FALSE "--config: required" --creature-config /nonexistent/creature.json)
check_cli(FALSE "--config: required" --validate-creature-config)
# Listing must still reject malformed arguments.
check_cli(FALSE "--config" --list-sound-devices --config)
