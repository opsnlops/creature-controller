
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

#include <math.h>
#include <stdlib.h>
#include <unistd.h>


#include "logging/logging.h"

#include "responsive_analog_read_filter.h"
#include "types.h"

analog_filter create_analog_filter(bool sleep_enable, float snap_multiplier, float activity_threshold, bool edge_snap_enable) {

    analog_filter f = {};

    // Grab the defaults from the CPP constructor
    f.sleep_enable = sleep_enable;
    f.snap_multiplier = snap_multiplier;
    f.analog_resolution = 4096;
    f.activity_threshold = activity_threshold;
    f.edge_snap_enable = edge_snap_enable;
    f.error_ema = (float)0.0;
    f.sleeping = false;

    f.raw_value = 0;
    f.responsive_value = 0;
    f.previous_responsive_value = 0;
    f.responsive_value_has_changed = false;

    debug("new analog_filter created");

    return f;

}

uint16_t analog_filter_get_raw_value(analog_filter* filter) {
    return filter->raw_value;
}

uint16_t analog_filter_get_value(analog_filter* filter) {
    return filter->responsive_value;
}

bool analog_filter_has_changed(analog_filter* filter) {
    return filter->responsive_value_has_changed;
}

bool analog_filter_is_sleeping(analog_filter* filter) {
    return filter->sleeping;
}

void analog_filter_update(analog_filter* filter, uint16_t raw_value) {

    filter->raw_value = raw_value;
    filter->previous_responsive_value = filter->responsive_value;
    filter->responsive_value = analog_filter_get_responsive_value(filter, raw_value);
    filter->responsive_value_has_changed = filter->responsive_value != filter->previous_responsive_value;

}

void analog_filter_set_snap_multiplier(analog_filter* filter, float new_multiplier) {

    if(new_multiplier > 1.0) {
        new_multiplier = (float)1.0;
    }
    else if(new_multiplier < 0.0) {
        new_multiplier = (float)0.0;
    }

    filter->snap_multiplier = new_multiplier;
}


void analog_filter_enable_sleep(analog_filter* filter) {
    filter->sleep_enable = true;
}

void analog_filter_disable_sleep(analog_filter* filter) {
    filter->sleep_enable = false;
}

void analog_filter_enable_edge_snap(analog_filter* filter) {
    filter->edge_snap_enable = true;
}

void analog_filter_disable_edge_snap(analog_filter* filter) {
    filter->edge_snap_enable = false;
}

void analog_filter_set_activity_threshold(analog_filter* filter, float new_threshold) {
    filter->activity_threshold = new_threshold;
}

void analog_filter_set_analog_resolution(analog_filter* filter, uint16_t resolution) {
    filter->analog_resolution = resolution;
}

uint16_t analog_filter_get_responsive_value(analog_filter* filter, uint16_t new_value) {

    // If sleep and edge snap are enabled and the new value is very close to an edge, drag it
    // a little closer to the edges.

    // This will make it easier to pull the output values right to the extremes without sleeping,
    // and it'll make movements right near the edge appear larger, making it easier to wake up

    if(filter->sleep_enable && filter->edge_snap_enable) {
        if(new_value < filter->activity_threshold) {
            new_value = (new_value * 2) - filter->activity_threshold;
        }
        else if(new_value > filter->analog_resolution - filter->activity_threshold) {
            new_value = (new_value * 2) - filter->analog_resolution + filter->activity_threshold;
        }
    }

    // get difference between new input value and current smooth value
    uint16_t diff = fabsf(new_value - filter->smooth_value);

    // measure the difference between the new value and current value
    // and use another exponential moving average to work out what
    // the current margin of error is
    filter->error_ema += ((new_value - filter->smooth_value) - filter->error_ema) * 0.4;

    // if sleep has been enabled, sleep when the amount of error is below the activity threshold
    if(filter->sleep_enable) {

        // recalculate sleeping status
        filter->sleeping = fabsf(filter->error_ema) < filter->activity_threshold;

    }

    // if we're allowed to sleep, and we're sleeping
    // then don't update responsiveValue this loop
    // just output the existing responsiveValue
    if(filter->sleep_enable && filter->sleeping) {
        return (uint16_t)filter->smooth_value;
    }

    // use a 'snap curve' function, where we pass in the diff (x) and get back a number from 0-1.
    // We want small values of x to result in an output close to zero, so when the smooth value is close to the input value
    // it'll smooth out noise aggressively by responding slowly to sudden changes.
    // We want a small increase in x to result in a much higher output value, so medium and large movements are snappy and responsive,
    // and aren't made sluggish by unnecessarily filtering out noise. A hyperbola (f(x) = 1/x) curve is used.
    // First x has an offset of 1 applied, so x = 0 now results in a value of 1 from the hyperbola function.
    // High values of x tend toward 0, but we want an output that begins at 0 and tends toward 1, so 1-y flips this up the right way.
    // Finally, the result is multiplied by 2 and capped at a maximum of one, which means that at a certain point all larger movements are maximally snappy

    // then multiply the input by SNAP_MULTIPLER so input values fit the snap curve better.
    float snap = analog_filter_snap_curve(diff * filter->snap_multiplier);

    // when sleep is enabled, the emphasis is stopping on a responsiveValue quickly, and it's less about easing into position.
    // If sleep is enabled, add a small amount to snap so it'll tend to snap into a more accurate position before sleeping starts.
    if(filter->sleep_enable) {
        snap *= 0.5 + 0.5;
    }

    // calculate the exponential moving average based on the snap
    filter->smooth_value += (new_value - filter->smooth_value) * snap;

    // ensure output is in bounds
    if(filter->smooth_value < 0.0) {
        filter->smooth_value = 0.0;
    }
    else if(filter->smooth_value > filter->analog_resolution - 1) {
        filter->smooth_value = filter->analog_resolution - 1;
    }

    return (uint16_t)filter->smooth_value;
}


float analog_filter_snap_curve(float x) {

    float y = 1.0 / (x + 1.0);
    y = (1.0 - y) * 2.0;
    if(y > 1.0) {
        return 1.0;
    }

    return y;
}