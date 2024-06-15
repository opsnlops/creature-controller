
/*
     This is a C port of the cool library at:
       https://github.com/dxinteractive/ResponsiveAnalogRead

    Modifications for use with the Pi Pico written by April White (@opsnlops), 2023.


    The original licence is:

    MIT License

    Copyright (c) 2016 Damien Clarke

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

 */


#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>


typedef struct {
    uint16_t analog_resolution;
    float snap_multiplier;
    bool sleep_enable;
    float activity_threshold;
    bool edge_snap_enable;

    float smooth_value;
    //uint32_t last_activity_ms;
    float error_ema;
    bool sleeping;

    uint16_t raw_value;
    uint16_t responsive_value;
    uint16_t previous_responsive_value;
    bool responsive_value_has_changed;
} analog_filter;


analog_filter create_analog_filter(bool sleep_enable, float snap_multiplier);
uint16_t analog_filter_get_raw_value(analog_filter* filter);
uint16_t analog_filter_get_value(analog_filter* filter);
bool analog_filter_has_changed(analog_filter* filter);
bool analog_filter_is_sleeping(analog_filter* filter);
void analog_filter_update(analog_filter* filter, uint16_t raw_value);

void analog_filter_set_snap_multiplier(analog_filter* filter, float new_multiplier);
void analog_filter_enable_sleep(analog_filter* filter);
void analog_filter_disable_sleep(analog_filter* filter);
void analog_filter_enable_edge_snap(analog_filter* filter);
void analog_filter_disable_edge_snap(analog_filter* filter);
void analog_filter_set_activity_threshold(analog_filter* filter, float new_threshold);
void analog_filter_set_analog_resolution(analog_filter* filter, uint16_t resolution);

uint16_t analog_filter_get_responsive_value(analog_filter* filter, uint16_t new_value);
float analog_filter_snap_curve(float x);

#ifdef __cplusplus
}
#endif