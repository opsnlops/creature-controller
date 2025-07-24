
#pragma once

#include <future>

// Define the GPIO registers. These are from the BCM2835 manual.
#define GPIO_BASE 0x3F200000 // Raspberry Pi 2/3/4
#define GPIO_SIZE (256)

#define GPIO_SET *(gpio + 7) // sets bits which are 1, ignores bits which are 0
#define GPIO_CLR                                                               \
    *(gpio + 10) // clears bits which are 1, ignores bits which are 0
#define GPIO_IN(g) (*(gpio + 13) & (1 << g)) // 0 if LOW, (1<<g) if HIGH

namespace creatures::device {

class GPIO {
  public:
    GPIO(const std::shared_ptr<creatures::Logger> &logger, bool enabled);
    ~GPIO() = default;

    void init();

    void toggleFirmwareReset();

  private:
    bool enabled;
    void *gpio_map;
    volatile unsigned *gpio;

    void turn_off(int pin);
    void turn_on(int pin);
    static void set_output(volatile unsigned *gpio, int g);

    std::shared_ptr<creatures::Logger> logger;
    std::future<void> firmwareResetFuture;
};

} // namespace creatures::device