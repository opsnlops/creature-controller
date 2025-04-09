/**
 * @file colors.h
 * @brief Optimized color conversion utilities for WS2812 RGB/RGBW LEDs
 *
 * This header provides structures and functions for working with RGB and HSV
 * color spaces, with optimizations tailored for microcontroller performance
 * on platforms like the Raspberry Pi Pico. The functions focus on efficient
 * conversion between different color representations with minimal computational
 * overhead.
 */

#pragma once

#include "controller/config.h"

/**
 * @brief HSV color structure
 *
 * Represents a color in the HSV (Hue, Saturation, Value) color space.
 */
typedef struct {
    double h;       /**< Hue angle in degrees (0-360) */
    double s;       /**< Saturation as a fraction between 0 and 1 */
    double v;       /**< Value/Brightness as a fraction between 0 and 1 */
} hsv_t;

/**
 * @brief Fixed-point HSV color structure for optimized calculations
 *
 * Provides a fixed-point representation of HSV colors to reduce
 * floating-point calculations on limited hardware.
 */
typedef struct {
    u16 h;          /**< Hue in range 0-65535 representing 0-360 degrees */
    u8 s;           /**< Saturation in range 0-255 representing 0.0-1.0 */
    u8 v;           /**< Value in range 0-255 representing 0.0-1.0 */
} hsv_fixed_t;

/**
 * @brief Convert HSV color directly to u32 format for WS2812 LEDs
 *
 * Efficiently transforms an HSV color to the packed u32 format expected by
 * WS2812 LED drivers (GRB order). This is an optimized function that avoids
 * creating temporary structures.
 *
 * @param in HSV color input
 * @return u32 packed color in GRB order (0x00GGRRBB)
 */
u32 hsv_to_urgb(hsv_t in);

/**
 * @brief Convert fixed-point HSV to u32 format
 *
 * Converts from the fixed-point HSV representation to the u32 format
 * expected by WS2812 LEDs. This is the most efficient conversion function
 * when working with fixed numeric ranges.
 *
 * @param in Fixed-point HSV color input
 * @return u32 packed color in GRB order (0x00GGRRBB)
 */
u32 hsv_fixed_to_urgb(hsv_fixed_t in);

/**
 * @brief Create a fixed-point HSV from floating-point HSV
 *
 * Utility function to convert standard HSV to fixed-point HSV format.
 *
 * @param in Standard HSV color
 * @return Equivalent fixed-point HSV representation
 */
hsv_fixed_t hsv_to_fixed(hsv_t in);

/**
 * @brief Get a rainbow-effect color based on a position
 *
 * Returns a color from a continuous rainbow spectrum based on
 * a position value. Useful for creating rainbow effects with minimal
 * computation.
 *
 * @param position A value from 0 to 255 representing position in the rainbow
 * @return u32 packed color in GRB order (0x00GGRRBB)
 */
u32 rainbow_color(u8 position);

/**
 * @brief Interpolate between two colors in HSV space
 *
 * Calculates an intermediate color between two HSV colors.
 *
 * @param start Starting HSV color
 * @param end Ending HSV color
 * @param progress Progress value from 0.0 (start) to 1.0 (end)
 * @return Interpolated HSV color
 */
hsv_t interpolate_hsv(hsv_t start, hsv_t end, double progress);

/**
 * @brief Interpolate between two colors and return u32 format
 *
 * Direct calculation of an interpolated color in u32 format.
 *
 * @param start_hue Starting hue (0-359)
 * @param end_hue Ending hue (0-359)
 * @param steps Total number of steps in transition
 * @param current_step Current step in transition
 * @param s Saturation (0.0-1.0)
 * @param v Value/Brightness (0.0-1.0)
 * @return u32 packed color in GRB order (0x00GGRRBB)
 */
u32 interpolate_hue_to_urgb(u16 start_hue, u16 end_hue, u8 steps, u8 current_step, double s, double v);

/**
 * @brief Get the next color in a golden ratio sequence
 *
 * Returns the next color in a sequence based on the golden ratio,
 * which creates visually pleasing color progressions.
 *
 * @param old_color Previous color value (0.0-1.0)
 * @return Next color value (0.0-1.0)
 */
double get_next_random_color(double old_color);

/**
 * @brief Get the next color in a rainbow sequence
 *
 * Advances a hue value by a fixed increment to create a rainbow effect.
 *
 * @param old_color Previous hue value (0.0-359.9)
 * @return Next hue value (0.0-359.9)
 */
double get_next_rainbow_color(double old_color);