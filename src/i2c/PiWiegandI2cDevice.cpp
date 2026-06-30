#include "i2c/PiWiegandI2cDevice.h"

namespace i2c_bus {
namespace {

constexpr unsigned char kWiegandOutSendCommand = 0x20;
constexpr unsigned char kBuildInfoReadCommand = 0x78;
constexpr unsigned char kWiegandOutStatusCommand = 0x21;
constexpr unsigned char kReadNextEventCommand = 0x79;

}  // namespace

PiWiegandI2cDevice::PiWiegandI2cDevice()
    : interface_index_(0), address_(0), transport_(nullptr), present_(false) {}

void PiWiegandI2cDevice::configure(unsigned char interface_index, unsigned char address,
                                   const I2cCommandTransport* transport) {
    interface_index_ = interface_index;
    address_ = address;
    transport_ = transport;
    present_ = false;
}

unsigned char PiWiegandI2cDevice::interface_index() const { return interface_index_; }

unsigned char PiWiegandI2cDevice::address() const { return address_; }

bool PiWiegandI2cDevice::is_present() const { return present_; }

bool PiWiegandI2cDevice::probe_presence() {
    present_ = transport_ != nullptr && transport_->probe_address(address_);
    return present_;
}

bool PiWiegandI2cDevice::read_build_info(char* buffer, unsigned int buffer_length) const {
    return transport_ != nullptr &&
           transport_->read_text_command(address_, kBuildInfoReadCommand, buffer, buffer_length);
}

bool PiWiegandI2cDevice::send_wiegand_out(const char* bit_string, WiegandOutSendResult* result) const {
    if (transport_ == nullptr || bit_string == nullptr || result == nullptr) {
        return false;
    }

    unsigned int bit_count = 0U;
    while (bit_string[bit_count] != '\0') {
        if (bit_string[bit_count] != '0' && bit_string[bit_count] != '1') {
            return false;
        }
        ++bit_count;
    }

    if (bit_count < 4U || bit_count > 48U) {
        return false;
    }

    const unsigned int packed_byte_count = (bit_count + 7U) / 8U;
    unsigned char payload[3 + 6] = {0};
    payload[0] = kWiegandOutSendCommand;
    payload[1] = static_cast<unsigned char>(bit_count);
    payload[2] = static_cast<unsigned char>(packed_byte_count);
    for (unsigned int bit_index = 0; bit_index < bit_count; ++bit_index) {
        if (bit_string[bit_index] == '1') {
            payload[3U + (bit_index / 8U)] |= static_cast<unsigned char>(0x80U >> (bit_index % 8U));
        }
    }

    for (unsigned int i = 0; i < sizeof(result->raw); ++i) {
        result->raw[i] = 0U;
    }
    result->bytes_read = 0U;
    result->status_code = 0U;
    result->echoed_command = 0U;

    if (!transport_->exchange_command(address_, payload, 3U + packed_byte_count, result->raw,
                                      sizeof(result->raw), &result->bytes_read)) {
        return false;
    }

    if (result->bytes_read >= 4U) {
        result->status_code = result->raw[2];
        result->echoed_command = result->raw[3];
    }
    return true;
}

bool PiWiegandI2cDevice::read_wiegand_out_status(WiegandOutStatusResult* result) const {
    if (transport_ == nullptr || result == nullptr) {
        return false;
    }

    for (unsigned int i = 0; i < sizeof(result->raw); ++i) {
        result->raw[i] = 0U;
    }
    result->bytes_read = 0U;
    result->echoed_command = 0U;
    result->busy_flag = 0U;
    result->queued_frame_count = 0U;

    if (!transport_->read_binary_command(address_, kWiegandOutStatusCommand, result->raw,
                                         sizeof(result->raw), &result->bytes_read)) {
        return false;
    }

    if (result->bytes_read >= 5U) {
        result->echoed_command = result->raw[2];
        result->busy_flag = result->raw[3];
        result->queued_frame_count = result->raw[4];
    }
    return true;
}

bool PiWiegandI2cDevice::read_next_event(EventReadResult* result) const {
    if (transport_ == nullptr || result == nullptr) {
        return false;
    }

    result->bytes_read = 0U;
    return transport_->read_binary_command(address_, kReadNextEventCommand, result->raw,
                                           sizeof(result->raw), &result->bytes_read);
}

}  // namespace i2c_bus
