#include "watchdog/WatchdogGlobals.h"

namespace creatures::watchdog {

// Initialize static members
std::atomic<double> WatchdogGlobals::currentTemperature{0.0};
std::atomic<double> WatchdogGlobals::currentPowerDraw{0.0};
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

double WatchdogGlobals::getTemperature() { return currentTemperature.load(); }

double WatchdogGlobals::getPowerDraw() { return currentPowerDraw.load(); }

} // namespace creatures::watchdog