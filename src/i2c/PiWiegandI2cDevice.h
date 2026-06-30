#pragma once

#include "i2c/I2cCommandTransport.h"

namespace i2c_bus {

struct EventReadResult {
    unsigned char raw[32];
    unsigned int bytes_read;
};

struct WiegandOutStatusResult {
    unsigned char raw[5];
    unsigned int bytes_read;
    unsigned char echoed_command;
    unsigned char busy_flag;
    unsigned char queued_frame_count;
};

struct WiegandOutSendResult {
    unsigned char raw[4];
    unsigned int bytes_read;
    unsigned char status_code;
    unsigned char echoed_command;
};

class PiWiegandI2cDevice {
  public:
    PiWiegandI2cDevice();

    void configure(unsigned char interface_index, unsigned char address,
                   const I2cCommandTransport* transport);

    unsigned char interface_index() const;
    unsigned char address() const;
    bool is_present() const;
    bool probe_presence();
    bool read_build_info(char* buffer, unsigned int buffer_length) const;
    bool send_wiegand_out(const char* bit_string, WiegandOutSendResult* result) const;
    bool read_wiegand_out_status(WiegandOutStatusResult* result) const;
    bool read_next_event(EventReadResult* result) const;

  private:
    unsigned char interface_index_;
    unsigned char address_;
    const I2cCommandTransport* transport_;
    bool present_;
};

}  // namespace i2c_bus
