#pragma once

#include "config/RuntimeConfig.h"
#include "i2c/ControllerIrqManager.h"
#include "i2c/I2cCommandTransport.h"
#include "i2c/PiWiegandDeviceManager.h"

namespace i2c_bus {

class I2cManager {
  public:
    I2cManager();

    bool init(const config::RuntimeConfig& runtime_config);
    bool send_configured_wiegand_frame(unsigned char interface_index, uint32_t card_number,
                                       ConfiguredWiegandSendResult* result) const;
    void service();
    bool status() const;
    const char* last_error() const;

  private:
    bool read_rtc();
    void scan_bus();
    void probe_expected_devices();
    void probe_lcd();

    I2cCommandTransport transport_;
    PiWiegandDeviceManager wiegand_device_manager_;
    ControllerIrqManager controller_irq_manager_;
    bool initialized_;
    const char* last_error_;
};

}  // namespace i2c_bus
