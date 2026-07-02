#pragma once

#include <cstdint>

#include "i2c/PiWiegandDeviceManager.h"

namespace i2c_bus {

class ControllerIrqManager {
  public:
    struct IrqPinConfig {
        unsigned char interface_index;
        unsigned char gpio_pin;
    };

    static constexpr unsigned int kIrqCount = 4;

    ControllerIrqManager();

    bool init(PiWiegandDeviceManager* device_manager);
    void service();

  private:
    void log_enabled_interfaces() const;
    void service_interface(const IrqPinConfig& config);
    bool is_interface_enabled(unsigned char interface_index) const;
    bool is_irq_asserted(const IrqPinConfig& config) const;

    PiWiegandDeviceManager* device_manager_;
    bool initialized_;
    uint32_t enabled_bitmap_;
    uint64_t next_poll_at_us_;
};

}  // namespace i2c_bus
