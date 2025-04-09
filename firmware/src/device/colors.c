#include <limits.h>
#include <math.h>

#include "colors.h"
#include "types.h"
#include "status_lights.h"

/* Fast HSV to u32 conversion for WS2812 LEDs */
u32 hsv_to_urgb(hsv_t in) {
    // Early exit for grayscale (saturation near zero)
    if (in.s <= 0.0001) {
        u8 val = (u8)(in.v * UCHAR_MAX);
        return ((u32)val << 8) | ((u32)val << 16) | val;
    }

    // Normalize hue to 0-360 range
    double hh = fmod(in.h, 360.0);
    if (hh < 0.0) hh += 360.0;

    // Convert to 0-6 range
    hh /= 60.0;

    // Get the fractional and integer parts
    int i = (int)hh;
    double ff = hh - i;

    // Calculate color components
    u8 p = (u8)((in.v * (1.0 - in.s)) * UCHAR_MAX);
    u8 q = (u8)((in.v * (1.0 - (in.s * ff))) * UCHAR_MAX);
    u8 t = (u8)((in.v * (1.0 - (in.s * (1.0 - ff)))) * UCHAR_MAX);
    u8 v = (u8)(in.v * UCHAR_MAX);

    // Calculate RGB and pack directly to GRB format for WS2812
    u8 r, g, b;

    switch(i) {
        case 0:
            r = v; g = t; b = p;
            break;
        case 1:
            r = q; g = v; b = p;
            break;
        case 2:
            r = p; g = v; b = t;
            break;
        case 3:
            r = p; g = q; b = v;
            break;
        case 4:
            r = t; g = p; b = v;
            break;
        case 5:
        default:
            r = v; g = p; b = q;
            break;
    }

    // Return in GRB format
    return ((u32)g << 16) | ((u32)r << 8) | b;
}

/* Convert HSV to fixed-point representation */
hsv_fixed_t hsv_to_fixed(hsv_t in) {
    hsv_fixed_t fixed;

    // Normalize hue to 0-360 range
    double hue = fmod(in.h, 360.0);
    if (hue < 0.0) hue += 360.0;

    // Convert to fixed point representations
    fixed.h = (u16)((hue / 360.0) * 65535.0);
    fixed.s = (u8)(in.s * 255.0);
    fixed.v = (u8)(in.v * 255.0);

    return fixed;
}

/* Direct conversion from fixed-point HSV to u32 RGB */
u32 hsv_fixed_to_urgb(hsv_fixed_t in) {
    // Early exit for grayscale
    if (in.s == 0) {
        return ((u32)in.v << 8) | ((u32)in.v << 16) | in.v;
    }

    // Scale hue from 0-65535 to 0-6*65536
    u32 hue_sector = ((u32)in.h * 6) >> 8;

    // Extract integer and fractional parts (0-6 range with 8-bit fraction)
    u8 sector = hue_sector >> 16;            // Integer part 0-5
    u8 fraction = (hue_sector >> 8) & 0xFF;  // Fractional part 0-255

    // Calculate color components
    u16 p = ((u16)in.v * (255 - in.s)) >> 8;
    u16 q = ((u16)in.v * (255 - ((in.s * fraction) >> 8))) >> 8;
    u16 t = ((u16)in.v * (255 - ((in.s * (255 - fraction)) >> 8))) >> 8;

    // Calculate RGB directly
    u8 r, g, b;

    switch(sector) {
        case 0:
            r = in.v; g = t; b = p;
            break;
        case 1:
            r = q; g = in.v; b = p;
            break;
        case 2:
            r = p; g = in.v; b = t;
            break;
        case 3:
            r = p; g = q; b = in.v;
            break;
        case 4:
            r = t; g = p; b = in.v;
            break;
        case 5:
        default:
            r = in.v; g = p; b = q;
            break;
    }

    // Return in GRB format for WS2812
    return ((u32)g << 16) | ((u32)r << 8) | b;
}

/* Pre-computed rainbow colors to avoid repeated HSV->RGB conversions */
static const u32 rainbow_table[256] = {
        0xFF0000, 0xFF0400, 0xFF0800, 0xFF0C00, 0xFF1000, 0xFF1400, 0xFF1800, 0xFF1C00,
        0xFF2000, 0xFF2400, 0xFF2800, 0xFF2C00, 0xFF3000, 0xFF3400, 0xFF3800, 0xFF3C00,
        0xFF4000, 0xFF4400, 0xFF4800, 0xFF4C00, 0xFF5000, 0xFF5400, 0xFF5800, 0xFF5C00,
        0xFF6000, 0xFF6400, 0xFF6800, 0xFF6C00, 0xFF7000, 0xFF7400, 0xFF7800, 0xFF7C00,
        0xFF8000, 0xFF8400, 0xFF8800, 0xFF8C00, 0xFF9000, 0xFF9400, 0xFF9800, 0xFF9C00,
        0xFFA000, 0xFFA400, 0xFFA800, 0xFFAC00, 0xFFB000, 0xFFB400, 0xFFB800, 0xFFBC00,
        0xFFC000, 0xFFC400, 0xFFC800, 0xFFCC00, 0xFFD000, 0xFFD400, 0xFFD800, 0xFFDC00,
        0xFFE000, 0xFFE400, 0xFFE800, 0xFFEC00, 0xFFF000, 0xFFF400, 0xFFF800, 0xFFFC00,
        0xFCFF00, 0xF8FF00, 0xF4FF00, 0xF0FF00, 0xECFF00, 0xE8FF00, 0xE4FF00, 0xE0FF00,
        0xDCFF00, 0xD8FF00, 0xD4FF00, 0xD0FF00, 0xCCFF00, 0xC8FF00, 0xC4FF00, 0xC0FF00,
        0xBCFF00, 0xB8FF00, 0xB4FF00, 0xB0FF00, 0xACFF00, 0xA8FF00, 0xA4FF00, 0xA0FF00,
        0x9CFF00, 0x98FF00, 0x94FF00, 0x90FF00, 0x8CFF00, 0x88FF00, 0x84FF00, 0x80FF00,
        0x7CFF00, 0x78FF00, 0x74FF00, 0x70FF00, 0x6CFF00, 0x68FF00, 0x64FF00, 0x60FF00,
        0x5CFF00, 0x58FF00, 0x54FF00, 0x50FF00, 0x4CFF00, 0x48FF00, 0x44FF00, 0x40FF00,
        0x3CFF00, 0x38FF00, 0x34FF00, 0x30FF00, 0x2CFF00, 0x28FF00, 0x24FF00, 0x20FF00,
        0x1CFF00, 0x18FF00, 0x14FF00, 0x10FF00, 0x0CFF00, 0x08FF00, 0x04FF00, 0x00FF00,
        0x00FF04, 0x00FF08, 0x00FF0C, 0x00FF10, 0x00FF14, 0x00FF18, 0x00FF1C, 0x00FF20,
        0x00FF24, 0x00FF28, 0x00FF2C, 0x00FF30, 0x00FF34, 0x00FF38, 0x00FF3C, 0x00FF40,
        0x00FF44, 0x00FF48, 0x00FF4C, 0x00FF50, 0x00FF54, 0x00FF58, 0x00FF5C, 0x00FF60,
        0x00FF64, 0x00FF68, 0x00FF6C, 0x00FF70, 0x00FF74, 0x00FF78, 0x00FF7C, 0x00FF80,
        0x00FF84, 0x00FF88, 0x00FF8C, 0x00FF90, 0x00FF94, 0x00FF98, 0x00FF9C, 0x00FFA0,
        0x00FFA4, 0x00FFA8, 0x00FFAC, 0x00FFB0, 0x00FFB4, 0x00FFB8, 0x00FFBC, 0x00FFC0,
        0x00FFC4, 0x00FFC8, 0x00FFCC, 0x00FFD0, 0x00FFD4, 0x00FFD8, 0x00FFDC, 0x00FFE0,
        0x00FFE4, 0x00FFE8, 0x00FFEC, 0x00FFF0, 0x00FFF4, 0x00FFF8, 0x00FFFC, 0x00FFFF,
        0x00FCFF, 0x00F8FF, 0x00F4FF, 0x00F0FF, 0x00ECFF, 0x00E8FF, 0x00E4FF, 0x00E0FF,
        0x00DCFF, 0x00D8FF, 0x00D4FF, 0x00D0FF, 0x00CCFF, 0x00C8FF, 0x00C4FF, 0x00C0FF,
        0x00BCFF, 0x00B8FF, 0x00B4FF, 0x00B0FF, 0x00ACFF, 0x00A8FF, 0x00A4FF, 0x00A0FF,
        0x009CFF, 0x0098FF, 0x0094FF, 0x0090FF, 0x008CFF, 0x0088FF, 0x0084FF, 0x0080FF,
        0x007CFF, 0x0078FF, 0x0074FF, 0x0070FF, 0x006CFF, 0x0068FF, 0x0064FF, 0x0060FF,
        0x005CFF, 0x0058FF, 0x0054FF, 0x0050FF, 0x004CFF, 0x0048FF, 0x0044FF, 0x0040FF,
        0x003CFF, 0x0038FF, 0x0034FF, 0x0030FF, 0x002CFF, 0x0028FF, 0x0024FF, 0x0020FF,
        0x001CFF, 0x0018FF, 0x0014FF, 0x0010FF, 0x000CFF, 0x0008FF, 0x0004FF, 0x0000FF
};

/* Quick lookup rainbow color - most efficient method for rainbow effects */
u32 rainbow_color(u8 position) {
    // Convert from 0x00RRGGBB format in the table to 0x00GGRRBB for WS2812
    u32 color = rainbow_table[position];
    u8 r = (color >> 16) & 0xFF;
    u8 g = (color >> 8) & 0xFF;
    u8 b = color & 0xFF;

    // Return in GRB format
    return ((u32)g << 16) | ((u32)r << 8) | b;
}

/* HSV color interpolation */
hsv_t interpolate_hsv(hsv_t start, hsv_t end, double progress) {
    hsv_t result;

    // Ensure progress is in the 0.0 to 1.0 range
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;

    // Handle hue interpolation (special case due to circular nature)
    double hue_diff = end.h - start.h;

    // Take the shortest path around the hue circle
    if (hue_diff > 180.0) {
        hue_diff -= 360.0;
    } else if (hue_diff < -180.0) {
        hue_diff += 360.0;
    }

    result.h = start.h + (hue_diff * progress);
    if (result.h < 0.0) result.h += 360.0;
    if (result.h >= 360.0) result.h -= 360.0;

    // Linear interpolation for saturation and value
    result.s = start.s + ((end.s - start.s) * progress);
    result.v = start.v + ((end.v - start.v) * progress);

    return result;
}

/* Direct hue interpolation to u32 - optimized for status light transitions */
u32 interpolate_hue_to_urgb(u16 start_hue, u16 end_hue, u8 steps, u8 current_step, double s, double v) {
    // Ensure current_step is in range
    if (current_step >= steps) {
        current_step = steps - 1;
    }

    // Get the progress as a fraction
    double progress = (double)current_step / (double)(steps - 1);

    // Calculate hue difference, taking the shorter path around the circle
    int hue_diff = end_hue - start_hue;
    if (hue_diff > 18000) {  // Half of 36000
        hue_diff -= 36000;
    } else if (hue_diff < -18000) {
        hue_diff += 36000;
    }

    // Calculate interpolated hue in 0-36000 range (100x degrees)
    int interpolated_hue = start_hue + (int)((double)hue_diff * progress);

    // Normalize to 0-36000 range
    while (interpolated_hue < 0) interpolated_hue += 36000;
    while (interpolated_hue >= 36000) interpolated_hue -= 36000;

    // Convert to degrees and create HSV
    hsv_t color = {
            .h = (double)interpolated_hue / 100.0,
            .s = s,
            .v = v
    };

    // Convert directly to urgb
    return hsv_to_urgb(color);
}

/**
 * Get a random color based on the golden ratio
 *
 * @param oldColor the old color
 * @return a new random color that's pleasing to the eye
 */
double get_next_random_color(double oldColor) {
    // https://martin.ankerl.com/2009/12/09/how-to-create-random-colors-programmatically/
    double tempColor = oldColor + GOLDEN_RATIO_CONJUGATE;
    tempColor = fmod(tempColor, 1.0);
    return tempColor;
}

/**
 * Get the next color in the HSV rainbow
 *
 * @param oldColor the old color
 * @return oldColor + IO_LIGHT_COLOR_CYCLE_SPEED or 0.0 if it's past 360
 */
double get_next_rainbow_color(double oldColor) {
    double nextColor = oldColor + IO_LIGHT_COLOR_CYCLE_SPEED;
    if(nextColor > 359.9) {
        return 0.0;
    }

    return nextColor;
}