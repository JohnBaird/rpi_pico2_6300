#include <cstdio>

#include "core/App.h"
#include "config/BootConfig.h"
#include "pico/stdlib.h"

int main() {
    stdio_init_all();
    sleep_ms(2000);

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
