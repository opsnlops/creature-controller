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
    static std::atomic<double> currentDynamixelTemperature;
    static std::atomic<double> currentDynamixelLoad;
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
     * Update the current Dynamixel temperature reading
     * @param temperature Max temperature across all Dynamixel servos in degrees Fahrenheit
     */
    static void updateDynamixelTemperature(double temperature);

    /**
     * Update the current Dynamixel load reading
     * @param load Max absolute present_load across all Dynamixel servos (0.1% units, 1000 = 100%)
     */
    static void updateDynamixelLoad(double load);

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

    /**
     * Get the current Dynamixel temperature reading
     * @return Max temperature across all Dynamixel servos in degrees Fahrenheit
     */
    static double getDynamixelTemperature();

    /**
     * Get the current Dynamixel load reading
     * @return Max absolute present_load across all Dynamixel servos (0.1% units, 1000 = 100%)
     */
    static double getDynamixelLoad();
};

} // namespace creatures::watchdog