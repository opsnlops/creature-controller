#include "watchdog/WatchdogGlobals.h"

namespace creatures::watchdog {

// Initialize static members
std::atomic<double> WatchdogGlobals::currentTemperature{0.0};
std::atomic<double> WatchdogGlobals::currentPowerDraw{0.0};
std::atomic<double> WatchdogGlobals::currentDynamixelTemperature{0.0};
std::atomic<double> WatchdogGlobals::currentDynamixelLoad{0.0};
std::mutex WatchdogGlobals::dataUpdateMutex;

void WatchdogGlobals::updateTemperature(double temperature) {
    std::lock_guard<std::mutex> lock(dataUpdateMutex);
    currentTemperature.store(temperature);
    
    // Debug logging - only log every 10th update to avoid spam
    static int updateCounter = 0;
    if (++updateCounter >= 10) {
        // Note: Can't use logger here since this is a static method
        // Temperature updates will show in the watchdog debug logs instead
        updateCounter = 0;
    }
}

void WatchdogGlobals::updatePowerDraw(double powerDraw) {
    std::lock_guard<std::mutex> lock(dataUpdateMutex);
    currentPowerDraw.store(powerDraw);
}

void WatchdogGlobals::updateDynamixelTemperature(double temperature) {
    std::lock_guard<std::mutex> lock(dataUpdateMutex);
    currentDynamixelTemperature.store(temperature);
}

void WatchdogGlobals::updateDynamixelLoad(double load) {
    std::lock_guard<std::mutex> lock(dataUpdateMutex);
    currentDynamixelLoad.store(load);
}

double WatchdogGlobals::getTemperature() { return currentTemperature.load(); }

double WatchdogGlobals::getPowerDraw() { return currentPowerDraw.load(); }

double WatchdogGlobals::getDynamixelTemperature() { return currentDynamixelTemperature.load(); }

double WatchdogGlobals::getDynamixelLoad() { return currentDynamixelLoad.load(); }

} // namespace creatures::watchdog