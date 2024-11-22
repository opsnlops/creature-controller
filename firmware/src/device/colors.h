
#pragma once

#include "controller/config.h"

// Let's define RGB and HSV as structs to make this easier
typedef struct {
    double r;       // a fraction between 0 and 1
    double g;       // a fraction between 0 and 1
    double b;       // a fraction between 0 and 1
} rgb_t;

typedef struct {
    double h;       // angle in degrees
    double s;       // a fraction between 0 and 1
    double v;       // a fraction between 0 and 1
} hsv_t;



/**
 * Convert HSV to RGB
 *
 * @param hsv the color to convert
 * @return an RGB with the same color
 */
rgb_t hsv_to_rgb(hsv_t in);

/**
 * Convert RGB to HSV
 *
 * @param rgb the color to convert
 * @return an HSV with the same color
 */
hsv_t rgb_to_hsv(rgb_t in);


/**
 * Convert an HSV into u32 RGB like what the ws2812 PIO function expects
 *
 * @param hsv the color to convert
 * @return a u32 with the same color in RGB
 */
u32 hsv_to_urgb(hsv_t hsv);

/**
 * Convert an RGB into u32 RGB like what the ws2812 PIO function expects
 *
 * @param in the input color
 * @return a u32 with the same color in RGB
 */
u32 rgb_to_urgb(rgb_t in);
