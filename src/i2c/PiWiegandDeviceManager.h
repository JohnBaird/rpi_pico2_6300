#pragma once

#include "i2c/PiWiegandI2cDevice.h"

namespace i2c_bus {

class PiWiegandDeviceManager {
  public:
    PiWiegandDeviceManager();

    void init(const I2cCommandTransport* transport, unsigned char base_address,
              unsigned char device_count);
    void probe_and_log_devices() const;

  private:
    PiWiegandI2cDevice devices_[8];
    unsigned char device_count_;
};

}  // namespace i2c_bus
