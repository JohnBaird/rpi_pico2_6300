#include "core/App.h"

#include <cstdio>

#include "pico/stdlib.h"

namespace core {

App::App(const config::BootConfig& boot_config)
    : boot_config_(boot_config),
      sd_card_manager_(),
      flash_config_store_(),
      config_manager_(flash_config_store_, boot_config.config_source_mode,
                      boot_config.block_on_factory_config_crc_mismatch),
      ds2401_manager_(boot_config.one_wire),
      led_manager_(boot_config.led),
      temperature_sensor_(),
      cpu_temperature_sensor_(),
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
    } else {
        sd_card_manager_.print_directory("/");
    }

    if (boot_config_.config_source_mode == config::ConfigSourceMode::littlefs_primary) {
        if (!flash_config_store_.init()) {
            std::printf("Warning: LittleFS initialization failed (%s)\n",
                        flash_config_store_.last_error());
        }
    } else {
        std::puts("LittleFS: initialization skipped by boot config");
    }

    if (!config_manager_.init()) {
        std::printf("Error: config initialization failed (%s)\n", config_manager_.last_error());
        return false;
    }

    if (!ds2401_manager_.init()) {
        std::printf("Warning: DS2401 initialization failed (%s)\n", ds2401_manager_.last_error());
    } else {
        char hardware_mac[18] = {0};
        if (ds2401_manager_.read_mac_address(hardware_mac, sizeof(hardware_mac))) {
            if (!config_manager_.override_device_mac(hardware_mac)) {
                std::puts("Warning: failed to override runtime MAC with DS2401 value");
            }
        } else {
            std::printf("Warning: DS2401 read failed (%s); using configured MAC instead\n",
                        ds2401_manager_.last_error());
        }
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

    if (!temperature_sensor_.init()) {
        std::printf("Warning: temperature sensor initialization failed (%s)\n",
                    temperature_sensor_.last_error());
    }

    if (!cpu_temperature_sensor_.init()) {
        std::printf("Warning: CPU temperature sensor initialization failed (%s)\n",
                    cpu_temperature_sensor_.last_error());
    }

    if (!ethernet_manager_.init(*runtime_config)) {
        std::printf("Error: ethernet initialization failed (%s)\n",
                    ethernet_manager_.last_error());
        return false;
    }

    if (!mqtt_manager_.init(*runtime_config, flash_config_store_, temperature_sensor_,
                            cpu_temperature_sensor_)) {
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
