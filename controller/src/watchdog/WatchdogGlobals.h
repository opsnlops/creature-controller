#pragma once

#include <atomic>
#include <mutex>

namespace creatures::watchdog {

/**
 * Global variables for watchdog monitoring
 * These are updated by the sensor message processor and read by the watchdog thread
 */
struct WatchdogGlobals {
    static std::atomic<double> currentTemperature;
    static std::atomic<double> currentPowerDraw;
    static std::mutex dataUpdateMutex;

    /**
     * Update the current temperature reading
     * @param temperature Temperature in degrees Fahrenheit
     */
    static void updateTemperature(double temperature);

    /**
     * Update the current power draw reading
     * @param powerDraw Power draw in watts
     */
    static void updatePowerDraw(double powerDraw);

    /**
     * Get the current temperature reading
     * @return Temperature in degrees Fahrenheit
     */
    static double getTemperature();

    /**
     * Get the current power draw reading
     * @return Power draw in watts
     */
    static double getPowerDraw();
};

} // namespace creatures::watchdog