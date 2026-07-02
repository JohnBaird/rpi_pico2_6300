#pragma once

#include "i2c/I2cCommandTransport.h"
#include "i2c/Rp2350PacsProtocol.h"

namespace i2c_bus {

class PacsControllerDevice {
  public:
    PacsControllerDevice();

    void configure(unsigned char interface_index, unsigned char address,
                   const I2cCommandTransport* transport);

    unsigned char interface_index() const;
    unsigned char address() const;
    bool is_present() const;
    bool probe_presence();

    bool read_build_info(char* buffer, unsigned int buffer_length) const;
    bool read_runtime_config(char* buffer, unsigned int buffer_length) const;
    bool read_next_event(Rp2350PacsProtocol::EventReadReply* reply) const;

    bool send_wiegand_out(const char* bit_string,
                          Rp2350PacsProtocol::WiegandOutSendReply* reply) const;
    bool read_wiegand_out_status(Rp2350PacsProtocol::WiegandOutStatusReply* reply) const;

    bool read_output_status(Rp2350PacsProtocol::OutputStatusReply* reply) const;
    bool set_outputs(unsigned char mask, unsigned char values,
                     Rp2350PacsProtocol::OutputCommandReply* reply) const;
    bool pulse_outputs(unsigned char mask, uint16_t duration_ms,
                       Rp2350PacsProtocol::OutputCommandReply* reply) const;

    bool read_rgb_status(Rp2350PacsProtocol::RgbStatusReply* reply) const;
    bool set_rgb(unsigned char color_code, Rp2350PacsProtocol::RgbCommandReply* reply) const;
    bool pulse_rgb(unsigned char color_code, uint16_t duration_ms,
                   Rp2350PacsProtocol::RgbCommandReply* reply) const;

    bool exchange_command(unsigned char command_id, const unsigned char* payload,
                          unsigned int payload_length, unsigned char* reply_buffer,
                          unsigned int reply_buffer_length, unsigned int* bytes_read) const;
    bool read_binary_command(unsigned char command_id, unsigned char* reply_buffer,
                             unsigned int reply_buffer_length, unsigned int* bytes_read) const;

  private:
    unsigned char interface_index_;
    unsigned char address_;
    const I2cCommandTransport* transport_;
    bool present_;
};

}  // namespace i2c_bus
