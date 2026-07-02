#pragma once

#include "hardware/i2c.h"

namespace devices {

class Mcp9808TemperatureSensor {
  public:
    Mcp9808TemperatureSensor();

    bool init();
    bool status() const;
    const char* last_error() const;
    bool read_temperature_c(float* out_temperature_c);
    const char* sensor_name() const;

  private:
    bool read_register(uint8_t register_address, uint8_t* destination, unsigned int length);
    bool verify_sensor_identity();
    float decode_temperature_c(const uint8_t* temperature_bytes) const;

    bool initialized_;
    bool present_;
    const char* last_error_;
};

}  // namespace devices
