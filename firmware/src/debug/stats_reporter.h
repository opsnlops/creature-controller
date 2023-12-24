
#pragma once

#include <FreeRTOS.h>
#include <timers.h>

namespace creatures::debug {

    void start_stats_reporter();
    void statsReportTimerCallback(TimerHandle_t xTimer);

};
