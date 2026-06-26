#pragma once

#include <cstdint>

#include "pico.h"

namespace config {

struct LedConfig {
    uint32_t healthy_on_ms;
    uint32_t healthy_off_ms;
    int gpio_pin;
    bool active_high;
};

struct BootConfig {
    LedConfig led;
};

constexpr BootConfig kMilestone1BootConfig{
    .led =
        {
            .healthy_on_ms = 100,
            .healthy_off_ms = 900,
#ifdef PICO_DEFAULT_LED_PIN
            .gpio_pin = PICO_DEFAULT_LED_PIN,
#else
            .gpio_pin = -1,
#endif
            .active_high = true,
        },
};

}  // namespace config
