#pragma once

#include "hardware/i2c.h"

namespace i2c_bus {

class I2cCommandTransport {
  public:
    I2cCommandTransport(i2c_inst_t* instance, unsigned int short_timeout_us,
                        unsigned int long_timeout_us, unsigned int command_settle_delay_us);

    bool probe_address(unsigned char address) const;
    bool exchange_command(unsigned char address, const unsigned char* payload,
                          unsigned int payload_length, unsigned char* buffer,
                          unsigned int buffer_length, unsigned int* bytes_read) const;
    bool read_binary_command(unsigned char address, unsigned char command, unsigned char* buffer,
                             unsigned int buffer_length, unsigned int* bytes_read) const;
    bool read_text_command(unsigned char address, unsigned char command, char* buffer,
                           unsigned int buffer_length) const;

  private:
    i2c_inst_t* instance_;
    unsigned int short_timeout_us_;
    unsigned int long_timeout_us_;
    unsigned int command_settle_delay_us_;
};

}  // namespace i2c_bus
