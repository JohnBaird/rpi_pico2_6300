#include <cstdio>

#include "core/App.h"
#include "config/BootConfig.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

namespace {

void wait_for_start_button(const config::StartupGateConfig& startup_gate) {
    if (startup_gate.button_gpio_pin < 0) {
        return;
    }

    gpio_init(static_cast<uint>(startup_gate.button_gpio_pin));
    gpio_set_dir(static_cast<uint>(startup_gate.button_gpio_pin), GPIO_IN);
    if (startup_gate.use_internal_pull_up) {
        gpio_pull_up(static_cast<uint>(startup_gate.button_gpio_pin));
    } else {
        gpio_disable_pulls(static_cast<uint>(startup_gate.button_gpio_pin));
    }

    std::printf("Startup gate: press button on GP%d to continue boot output\n",
                startup_gate.button_gpio_pin);
    std::puts("Startup gate: wire the push-button between the GPIO and GND");

    while (true) {
        const bool raw_level = gpio_get(static_cast<uint>(startup_gate.button_gpio_pin)) != 0;
        const bool pressed = startup_gate.active_low ? !raw_level : raw_level;
        if (pressed) {
            while (true) {
                const bool release_level =
                    gpio_get(static_cast<uint>(startup_gate.button_gpio_pin)) != 0;
                const bool still_pressed = startup_gate.active_low ? !release_level : release_level;
                if (!still_pressed) {
                    break;
                }
                tight_loop_contents();
                sleep_ms(5);
            }

            std::puts("Startup gate: button pressed, continuing boot");
            sleep_ms(50);
            return;
        }

        tight_loop_contents();
        sleep_ms(5);
    }
}

}  // namespace

int main() {
    stdio_init_all();
    sleep_ms(2000);
    wait_for_start_button(config::kMilestone1BootConfig.startup_gate);

    std::puts("W6300 access gateway starting");
    std::puts("Milestone 5: I2C scan + RTC read + peripheral address probe");

    core::App app(config::kMilestone1BootConfig);
    if (!app.init()) {
        std::puts("Fatal: application initialization failed");
        while (true) {
            tight_loop_contents();
        }
    }

    app.run();
    return 0;
}
