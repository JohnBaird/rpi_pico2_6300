#include "i2c/PacsControllerDevice.h"

namespace i2c_bus {

PacsControllerDevice::PacsControllerDevice()
    : interface_index_(0), address_(0), transport_(nullptr), present_(false) {}

void PacsControllerDevice::configure(unsigned char interface_index, unsigned char address,
                                     const I2cCommandTransport* transport) {
    interface_index_ = interface_index;
    address_ = address;
    transport_ = transport;
    present_ = false;
}

unsigned char PacsControllerDevice::interface_index() const { return interface_index_; }

unsigned char PacsControllerDevice::address() const { return address_; }

bool PacsControllerDevice::is_present() const { return present_; }

bool PacsControllerDevice::probe_presence() {
    present_ = transport_ != nullptr && transport_->probe_address(address_);
    return present_;
}

bool PacsControllerDevice::read_build_info(char* buffer, unsigned int buffer_length) const {
    return transport_ != nullptr &&
           transport_->read_text_command(address_, Rp2350PacsProtocol::kCmdBuildInfoRead, buffer,
                                         buffer_length);
}

bool PacsControllerDevice::read_runtime_config(char* buffer, unsigned int buffer_length) const {
    return transport_ != nullptr &&
           transport_->read_text_command(address_, Rp2350PacsProtocol::kCmdRuntimeConfigRead,
                                         buffer, buffer_length);
}

bool PacsControllerDevice::read_next_event(Rp2350PacsProtocol::EventReadReply* reply) const {
    if (transport_ == nullptr || reply == nullptr) {
        return false;
    }
    for (unsigned int i = 0; i < sizeof(reply->raw); ++i) {
        reply->raw[i] = 0U;
    }
    reply->bytes_read = 0U;
    return transport_->read_binary_command(address_, Rp2350PacsProtocol::kCmdEventRead, reply->raw,
                                           sizeof(reply->raw), &reply->bytes_read);
}

bool PacsControllerDevice::send_wiegand_out(
    const char* bit_string, Rp2350PacsProtocol::WiegandOutSendReply* reply) const {
    if (transport_ == nullptr || bit_string == nullptr || reply == nullptr) {
        return false;
    }

    unsigned char payload[9] = {0};
    unsigned int payload_length = 0U;
    if (!Rp2350PacsProtocol::build_wiegand_out_payload(bit_string, payload, sizeof(payload),
                                                       &payload_length)) {
        return false;
    }

    unsigned char raw_reply[sizeof(reply->raw)] = {0};
    unsigned int bytes_read = 0U;
    if (!transport_->exchange_command(address_, payload, payload_length, raw_reply, sizeof(raw_reply),
                                      &bytes_read)) {
        return false;
    }

    return Rp2350PacsProtocol::parse_wiegand_out_send_reply(raw_reply, bytes_read, reply);
}

bool PacsControllerDevice::read_wiegand_out_status(
    Rp2350PacsProtocol::WiegandOutStatusReply* reply) const {
    if (transport_ == nullptr || reply == nullptr) {
        return false;
    }

    unsigned char raw_reply[sizeof(reply->raw)] = {0};
    unsigned int bytes_read = 0U;
    if (!transport_->read_binary_command(address_, Rp2350PacsProtocol::kCmdWiegandOutStatus,
                                         raw_reply, sizeof(raw_reply), &bytes_read)) {
        return false;
    }

    return Rp2350PacsProtocol::parse_wiegand_out_status_reply(raw_reply, bytes_read, reply);
}

bool PacsControllerDevice::read_output_status(Rp2350PacsProtocol::OutputStatusReply* reply) const {
    if (transport_ == nullptr || reply == nullptr) {
        return false;
    }

    unsigned char raw_reply[sizeof(reply->raw)] = {0};
    unsigned int bytes_read = 0U;
    if (!transport_->read_binary_command(address_, Rp2350PacsProtocol::kCmdOutputStatus,
                                         raw_reply, sizeof(raw_reply), &bytes_read)) {
        return false;
    }

    return Rp2350PacsProtocol::parse_output_status_reply(raw_reply, bytes_read, reply);
}

bool PacsControllerDevice::set_outputs(unsigned char mask, unsigned char values,
                                       Rp2350PacsProtocol::OutputCommandReply* reply) const {
    if (transport_ == nullptr || reply == nullptr) {
        return false;
    }

    unsigned char payload[3] = {0};
    unsigned int payload_length = 0U;
    if (!Rp2350PacsProtocol::build_output_set_payload(mask, values, payload, sizeof(payload),
                                                      &payload_length)) {
        return false;
    }

    unsigned char raw_reply[sizeof(reply->raw)] = {0};
    unsigned int bytes_read = 0U;
    if (!transport_->exchange_command(address_, payload, payload_length, raw_reply, sizeof(raw_reply),
                                      &bytes_read)) {
        return false;
    }

    return Rp2350PacsProtocol::parse_output_command_reply(
        raw_reply, bytes_read, Rp2350PacsProtocol::kCmdOutputSet, reply);
}

bool PacsControllerDevice::pulse_outputs(unsigned char mask, uint16_t duration_ms,
                                         Rp2350PacsProtocol::OutputCommandReply* reply) const {
    if (transport_ == nullptr || reply == nullptr) {
        return false;
    }

    unsigned char payload[4] = {0};
    unsigned int payload_length = 0U;
    if (!Rp2350PacsProtocol::build_output_pulse_payload(mask, duration_ms, payload,
                                                        sizeof(payload), &payload_length)) {
        return false;
    }

    unsigned char raw_reply[sizeof(reply->raw)] = {0};
    unsigned int bytes_read = 0U;
    if (!transport_->exchange_command(address_, payload, payload_length, raw_reply, sizeof(raw_reply),
                                      &bytes_read)) {
        return false;
    }

    return Rp2350PacsProtocol::parse_output_command_reply(
        raw_reply, bytes_read, Rp2350PacsProtocol::kCmdOutputPulse, reply);
}

bool PacsControllerDevice::read_rgb_status(Rp2350PacsProtocol::RgbStatusReply* reply) const {
    if (transport_ == nullptr || reply == nullptr) {
        return false;
    }

    unsigned char raw_reply[sizeof(reply->raw)] = {0};
    unsigned int bytes_read = 0U;
    if (!transport_->read_binary_command(address_, Rp2350PacsProtocol::kCmdRgbStatus, raw_reply,
                                         sizeof(raw_reply), &bytes_read)) {
        return false;
    }

    return Rp2350PacsProtocol::parse_rgb_status_reply(raw_reply, bytes_read, reply);
}

bool PacsControllerDevice::set_rgb(unsigned char color_code,
                                   Rp2350PacsProtocol::RgbCommandReply* reply) const {
    if (transport_ == nullptr || reply == nullptr) {
        return false;
    }

    unsigned char payload[2] = {0};
    unsigned int payload_length = 0U;
    if (!Rp2350PacsProtocol::build_rgb_set_payload(color_code, payload, sizeof(payload),
                                                   &payload_length)) {
        return false;
    }

    unsigned char raw_reply[sizeof(reply->raw)] = {0};
    unsigned int bytes_read = 0U;
    if (!transport_->exchange_command(address_, payload, payload_length, raw_reply, sizeof(raw_reply),
                                      &bytes_read)) {
        return false;
    }

    return Rp2350PacsProtocol::parse_rgb_command_reply(raw_reply, bytes_read,
                                                       Rp2350PacsProtocol::kCmdRgbSet, reply);
}

bool PacsControllerDevice::pulse_rgb(unsigned char color_code, uint16_t duration_ms,
                                     Rp2350PacsProtocol::RgbCommandReply* reply) const {
    if (transport_ == nullptr || reply == nullptr) {
        return false;
    }

    unsigned char payload[4] = {0};
    unsigned int payload_length = 0U;
    if (!Rp2350PacsProtocol::build_rgb_pulse_payload(color_code, duration_ms, payload,
                                                     sizeof(payload), &payload_length)) {
        return false;
    }

    unsigned char raw_reply[sizeof(reply->raw)] = {0};
    unsigned int bytes_read = 0U;
    if (!transport_->exchange_command(address_, payload, payload_length, raw_reply, sizeof(raw_reply),
                                      &bytes_read)) {
        return false;
    }

    return Rp2350PacsProtocol::parse_rgb_command_reply(raw_reply, bytes_read,
                                                       Rp2350PacsProtocol::kCmdRgbPulse, reply);
}

bool PacsControllerDevice::exchange_command(unsigned char command_id, const unsigned char* payload,
                                            unsigned int payload_length,
                                            unsigned char* reply_buffer,
                                            unsigned int reply_buffer_length,
                                            unsigned int* bytes_read) const {
    if (transport_ == nullptr || reply_buffer == nullptr || reply_buffer_length == 0U ||
        bytes_read == nullptr) {
        return false;
    }

    if (payload == nullptr || payload_length == 0U) {
        return transport_->read_binary_command(address_, command_id, reply_buffer, reply_buffer_length,
                                               bytes_read);
    }

    return transport_->exchange_command(address_, payload, payload_length, reply_buffer,
                                        reply_buffer_length, bytes_read);
}

bool PacsControllerDevice::read_binary_command(unsigned char command_id,
                                               unsigned char* reply_buffer,
                                               unsigned int reply_buffer_length,
                                               unsigned int* bytes_read) const {
    return transport_ != nullptr &&
           transport_->read_binary_command(address_, command_id, reply_buffer, reply_buffer_length,
                                           bytes_read);
}

}  // namespace i2c_bus
