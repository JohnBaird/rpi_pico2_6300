#include "i2c/I2cCommandTransport.h"

#include <cctype>

#include "pico/stdlib.h"

namespace i2c_bus {

I2cCommandTransport::I2cCommandTransport(i2c_inst_t* instance, unsigned int short_timeout_us,
                                         unsigned int long_timeout_us,
                                         unsigned int command_settle_delay_us)
    : instance_(instance),
      short_timeout_us_(short_timeout_us),
      long_timeout_us_(long_timeout_us),
      command_settle_delay_us_(command_settle_delay_us) {}

bool I2cCommandTransport::probe_address(unsigned char address) const {
    uint8_t rx_byte = 0;
    const int result =
        i2c_read_timeout_us(instance_, address, &rx_byte, 1, false, short_timeout_us_);
    return result == 1;
}

bool I2cCommandTransport::exchange_command(unsigned char address, const unsigned char* payload,
                                           unsigned int payload_length, unsigned char* buffer,
                                           unsigned int buffer_length,
                                           unsigned int* bytes_read) const {
    if (payload == nullptr || payload_length == 0U || buffer == nullptr || buffer_length == 0U ||
        bytes_read == nullptr) {
        return false;
    }

    *bytes_read = 0U;
    int read_result = -1;

    if (i2c_write_timeout_us(instance_, address, payload, payload_length, false, short_timeout_us_) !=
        static_cast<int>(payload_length)) {
        return false;
    }

    sleep_us(command_settle_delay_us_);
    read_result =
        i2c_read_timeout_us(instance_, address, buffer, buffer_length, false, long_timeout_us_);

    if (read_result <= 0) {
        if (i2c_write_timeout_us(instance_, address, payload, payload_length, true, short_timeout_us_) !=
            static_cast<int>(payload_length)) {
            return false;
        }

        read_result =
            i2c_read_timeout_us(instance_, address, buffer, buffer_length, false, long_timeout_us_);
        if (read_result <= 0) {
            return false;
        }
    }

    *bytes_read = static_cast<unsigned int>(read_result);
    return true;
}

bool I2cCommandTransport::read_binary_command(unsigned char address, unsigned char command,
                                              unsigned char* buffer, unsigned int buffer_length,
                                              unsigned int* bytes_read) const {
    return exchange_command(address, &command, 1U, buffer, buffer_length, bytes_read);
}

bool I2cCommandTransport::read_text_command(unsigned char address, unsigned char command,
                                            char* buffer, unsigned int buffer_length) const {
    if (buffer == nullptr || buffer_length < 2U) {
        return false;
    }

    buffer[0] = '\0';

    unsigned char raw_buffer[128] = {0};
    const unsigned int raw_length =
        buffer_length - 1U < sizeof(raw_buffer) ? buffer_length - 1U : sizeof(raw_buffer);
    unsigned int bytes_read = 0U;
    if (!read_binary_command(address, command, raw_buffer, raw_length, &bytes_read) || bytes_read == 0U) {
        return false;
    }

    unsigned int text_length = bytes_read;
    while (text_length > 0U &&
           (raw_buffer[text_length - 1U] == 0U || std::isspace(raw_buffer[text_length - 1U]) != 0)) {
        --text_length;
    }

    for (unsigned int i = 0; i < text_length; ++i) {
        buffer[i] = static_cast<char>(raw_buffer[i]);
    }
    buffer[text_length] = '\0';
    return text_length > 0U;
}

}  // namespace i2c_bus
