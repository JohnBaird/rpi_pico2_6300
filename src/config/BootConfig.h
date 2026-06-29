#pragma once

#include <cstdint>

#include "pico.h"

namespace config {

enum class ConfigSourceMode {
    littlefs_primary,
    factory_only,
};

struct LedConfig {
    uint32_t healthy_on_ms;
    uint32_t healthy_off_ms;
    int gpio_pin;
    bool active_high;
};

struct OneWireConfig {
    int gpio_pin;
    bool use_internal_pull_up;
};

struct StartupGateConfig {
    int button_gpio_pin;
    bool active_low;
    bool use_internal_pull_up;
};

struct BootConfig {
    LedConfig led;
    OneWireConfig one_wire;
    StartupGateConfig startup_gate;
    ConfigSourceMode config_source_mode;
    bool block_on_factory_config_crc_mismatch;
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
    .one_wire =
        {
            // GP6 stays clear of the current I2C bus (GP4/GP5) and SD SPI wiring
            // (GP10-GP13), while landing next to GND on the Pico header for easy
            // short-run 1-wire routing.
            .gpio_pin = 6,
            .use_internal_pull_up = false,
        },
    .startup_gate =
        {
            // GP22 is currently unused by the firmware and sits close to a GND pin
            // on the Pico header, which makes it a good candidate for a simple
            // push-button-to-ground startup gate.
            .button_gpio_pin = 22,
            .active_low = true,
            .use_internal_pull_up = true,
        },
    .config_source_mode = ConfigSourceMode::factory_only,
    .block_on_factory_config_crc_mismatch = false,
};

}  // namespace config
