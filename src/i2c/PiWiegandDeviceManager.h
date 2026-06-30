#pragma once

#include "config/RuntimeConfig.h"
#include "i2c/PiWiegandI2cDevice.h"
#include "wiegand/WiegandBuilder.h"

namespace i2c_bus {

struct ConfiguredWiegandSendResult {
    bool success;
    unsigned char interface_index;
    unsigned char address;
    char format_name[32];
    uint32_t facility_code;
    uint32_t card_number;
    char bit_string[65];
    WiegandOutSendResult send_result;
    const char* error_reason;
};

class PiWiegandDeviceManager {
  public:
    PiWiegandDeviceManager();

    void init(const I2cCommandTransport* transport, unsigned char base_address,
              unsigned char device_count, const config::RuntimeConfig* runtime_config);
    bool send_configured_wiegand_frame(unsigned char interface_index, uint32_t card_number,
                                       ConfiguredWiegandSendResult* result) const;
    void probe_and_log_devices() const;

  private:
    bool build_configured_wiegand_bit_string(unsigned char interface_index, uint32_t card_number,
                                             char* destination,
                                             unsigned int destination_size) const;

    PiWiegandI2cDevice devices_[8];
    unsigned char device_count_;
    const config::RuntimeConfig* runtime_config_;
    wiegand::WiegandBuilder wiegand_builder_;
};

}  // namespace i2c_bus
