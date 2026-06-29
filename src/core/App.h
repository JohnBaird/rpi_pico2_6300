#pragma once

#include "config/BootConfig.h"
#include "config/ConfigManager.h"
#include "devices/Ds2401Manager.h"
#include "devices/LedManager.h"
#include "devices/Mcp9808TemperatureSensor.h"
#include "devices/Rp2350TemperatureSensor.h"
#include "i2c/I2cManager.h"
#include "mqtt/MqttManager.h"
#include "network/EthernetManager.h"
#include "storage/FlashConfigStore.h"
#include "storage/SdCardManager.h"

namespace core {

class App {
  public:
    explicit App(const config::BootConfig& boot_config);

    bool init();
    void run();

  private:
    const config::BootConfig& boot_config_;
    storage::SdCardManager sd_card_manager_;
    storage::FlashConfigStore flash_config_store_;
    config::ConfigManager config_manager_;
    devices::Ds2401Manager ds2401_manager_;
    devices::LedManager led_manager_;
    devices::Mcp9808TemperatureSensor temperature_sensor_;
    devices::Rp2350TemperatureSensor cpu_temperature_sensor_;
    i2c_bus::I2cManager i2c_manager_;
    network::EthernetManager ethernet_manager_;
    mqtt::MqttManager mqtt_manager_;
    bool initialized_;
};

}  // namespace core
