#include "core/App.h"

#include <cstdio>

#include "pico/stdlib.h"

namespace core {

App::App(const config::BootConfig& boot_config)
    : boot_config_(boot_config),
      sd_card_manager_(),
      flash_config_store_(),
      config_manager_(flash_config_store_),
      led_manager_(boot_config.led),
      i2c_manager_(),
      ethernet_manager_(),
      mqtt_manager_(),
      initialized_(false) {}

bool App::init() {
    std::puts("Boot: initializing Milestone 5 application");

    if (!led_manager_.init()) {
        std::puts("Error: LED initialization failed. Check board LED definition.");
        return false;
    }

    if (!sd_card_manager_.init()) {
        std::printf("Warning: SD initialization failed (%s)\n", sd_card_manager_.last_error());
    }

    if (!flash_config_store_.init()) {
        std::printf("Warning: LittleFS initialization failed (%s)\n",
                    flash_config_store_.last_error());
    }

    if (!config_manager_.init()) {
        std::printf("Error: config initialization failed (%s)\n", config_manager_.last_error());
        return false;
    }

    config_manager_.print_summary();

    const config::RuntimeConfig* runtime_config = config_manager_.runtime_config();
    if (runtime_config == nullptr) {
        std::puts("Error: runtime config unavailable after successful initialization");
        return false;
    }

    if (!i2c_manager_.init()) {
        std::printf("Error: I2C initialization failed (%s)\n", i2c_manager_.last_error());
        return false;
    }

    if (!ethernet_manager_.init(*runtime_config)) {
        std::printf("Error: ethernet initialization failed (%s)\n",
                    ethernet_manager_.last_error());
        return false;
    }

    if (!mqtt_manager_.init(*runtime_config)) {
        std::printf("Error: MQTT initialization failed (%s)\n", mqtt_manager_.last_error());
        return false;
    }

    initialized_ = true;
    std::puts("Boot: initialization complete");
    return true;
}

void App::run() {
    if (!initialized_) {
        std::puts("Error: App::run called before successful initialization");
        return;
    }

    std::puts("Run: entering main service loop");
    while (true) {
        led_manager_.service();
        i2c_manager_.service();
        ethernet_manager_.service();
        mqtt_manager_.service();
        tight_loop_contents();
    }
}

}  // namespace core
