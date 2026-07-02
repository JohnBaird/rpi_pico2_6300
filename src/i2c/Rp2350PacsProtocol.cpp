#include "i2c/Rp2350PacsProtocol.h"

#include <cstring>

namespace i2c_bus {
namespace {

bool zero_reply(unsigned char* raw, unsigned int raw_size, unsigned int* bytes_read) {
    if (raw == nullptr || bytes_read == nullptr) {
        return false;
    }
    for (unsigned int i = 0; i < raw_size; ++i) {
        raw[i] = 0U;
    }
    *bytes_read = 0U;
    return true;
}

}  // namespace

bool Rp2350PacsProtocol::build_wiegand_out_payload(const char* bit_string, unsigned char* payload,
                                                   unsigned int payload_capacity,
                                                   unsigned int* payload_length) {
    if (bit_string == nullptr || payload == nullptr || payload_length == nullptr ||
        payload_capacity < 4U) {
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
    const unsigned int total_length = 3U + packed_byte_count;
    if (total_length > payload_capacity) {
        return false;
    }

    for (unsigned int i = 0; i < total_length; ++i) {
        payload[i] = 0U;
    }

    payload[0] = kCmdWiegandOutSend;
    payload[1] = static_cast<unsigned char>(bit_count);
    payload[2] = static_cast<unsigned char>(packed_byte_count);
    for (unsigned int bit_index = 0; bit_index < bit_count; ++bit_index) {
        if (bit_string[bit_index] == '1') {
            payload[3U + (bit_index / 8U)] |=
                static_cast<unsigned char>(0x80U >> (bit_index % 8U));
        }
    }

    *payload_length = total_length;
    return true;
}

bool Rp2350PacsProtocol::build_output_set_payload(unsigned char mask, unsigned char values,
                                                  unsigned char* payload,
                                                  unsigned int payload_capacity,
                                                  unsigned int* payload_length) {
    if (payload == nullptr || payload_length == nullptr || payload_capacity < 3U) {
        return false;
    }
    payload[0] = kCmdOutputSet;
    payload[1] = mask;
    payload[2] = values;
    *payload_length = 3U;
    return true;
}

bool Rp2350PacsProtocol::build_output_pulse_payload(unsigned char mask, uint16_t duration_ms,
                                                    unsigned char* payload,
                                                    unsigned int payload_capacity,
                                                    unsigned int* payload_length) {
    if (payload == nullptr || payload_length == nullptr || payload_capacity < 4U) {
        return false;
    }
    payload[0] = kCmdOutputPulse;
    payload[1] = mask;
    payload[2] = static_cast<unsigned char>(duration_ms & 0xFFU);
    payload[3] = static_cast<unsigned char>((duration_ms >> 8U) & 0xFFU);
    *payload_length = 4U;
    return true;
}

bool Rp2350PacsProtocol::build_rgb_set_payload(unsigned char color_code, unsigned char* payload,
                                               unsigned int payload_capacity,
                                               unsigned int* payload_length) {
    if (payload == nullptr || payload_length == nullptr || payload_capacity < 2U) {
        return false;
    }
    payload[0] = kCmdRgbSet;
    payload[1] = color_code;
    *payload_length = 2U;
    return true;
}

bool Rp2350PacsProtocol::build_rgb_pulse_payload(unsigned char color_code, uint16_t duration_ms,
                                                 unsigned char* payload,
                                                 unsigned int payload_capacity,
                                                 unsigned int* payload_length) {
    if (payload == nullptr || payload_length == nullptr || payload_capacity < 4U) {
        return false;
    }
    payload[0] = kCmdRgbPulse;
    payload[1] = color_code;
    payload[2] = static_cast<unsigned char>(duration_ms & 0xFFU);
    payload[3] = static_cast<unsigned char>((duration_ms >> 8U) & 0xFFU);
    *payload_length = 4U;
    return true;
}

bool Rp2350PacsProtocol::parse_wiegand_out_send_reply(const unsigned char* raw,
                                                      unsigned int bytes_read,
                                                      WiegandOutSendReply* reply) {
    if (raw == nullptr || reply == nullptr) {
        return false;
    }
    zero_reply(reply->raw, sizeof(reply->raw), &reply->bytes_read);
    reply->status_code = 0U;
    reply->echoed_command = 0U;

    if (bytes_read > sizeof(reply->raw)) {
        return false;
    }
    for (unsigned int i = 0; i < bytes_read; ++i) {
        reply->raw[i] = raw[i];
    }
    reply->bytes_read = bytes_read;

    if (bytes_read < 4U || raw[0] != 0x57U || raw[1] != 0x4FU || raw[3] != kCmdWiegandOutSend) {
        return false;
    }

    reply->status_code = raw[2];
    reply->echoed_command = raw[3];
    return true;
}

bool Rp2350PacsProtocol::parse_wiegand_out_status_reply(const unsigned char* raw,
                                                        unsigned int bytes_read,
                                                        WiegandOutStatusReply* reply) {
    if (raw == nullptr || reply == nullptr) {
        return false;
    }
    zero_reply(reply->raw, sizeof(reply->raw), &reply->bytes_read);
    reply->echoed_command = 0U;
    reply->busy_flag = 0U;
    reply->queued_frame_count = 0U;

    if (bytes_read > sizeof(reply->raw)) {
        return false;
    }
    for (unsigned int i = 0; i < bytes_read; ++i) {
        reply->raw[i] = raw[i];
    }
    reply->bytes_read = bytes_read;

    if (bytes_read < 5U || raw[0] != 0x57U || raw[1] != 0x53U || raw[2] != kCmdWiegandOutStatus) {
        return false;
    }

    reply->echoed_command = raw[2];
    reply->busy_flag = raw[3];
    reply->queued_frame_count = raw[4];
    return true;
}

bool Rp2350PacsProtocol::parse_output_status_reply(const unsigned char* raw, unsigned int bytes_read,
                                                   OutputStatusReply* reply) {
    if (raw == nullptr || reply == nullptr) {
        return false;
    }
    zero_reply(reply->raw, sizeof(reply->raw), &reply->bytes_read);
    reply->echoed_command = 0U;
    reply->state_bits = 0U;
    reply->pulse_bits = 0U;

    if (bytes_read > sizeof(reply->raw)) {
        return false;
    }
    for (unsigned int i = 0; i < bytes_read; ++i) {
        reply->raw[i] = raw[i];
    }
    reply->bytes_read = bytes_read;

    if (bytes_read < 5U || raw[0] != 0x4FU || raw[1] != 0x53U || raw[2] != kCmdOutputStatus) {
        return false;
    }

    reply->echoed_command = raw[2];
    reply->state_bits = raw[3];
    reply->pulse_bits = raw[4];
    return true;
}

bool Rp2350PacsProtocol::parse_output_command_reply(const unsigned char* raw,
                                                    unsigned int bytes_read,
                                                    unsigned char expected_command,
                                                    OutputCommandReply* reply) {
    if (raw == nullptr || reply == nullptr) {
        return false;
    }
    zero_reply(reply->raw, sizeof(reply->raw), &reply->bytes_read);
    reply->echoed_command = 0U;
    reply->status_code = 0U;
    reply->state_bits = 0U;
    reply->pulse_bits = 0U;

    if (bytes_read > sizeof(reply->raw)) {
        return false;
    }
    for (unsigned int i = 0; i < bytes_read; ++i) {
        reply->raw[i] = raw[i];
    }
    reply->bytes_read = bytes_read;

    if (bytes_read < 6U || raw[0] != 0x4FU || raw[1] != 0x43U || raw[2] != expected_command) {
        return false;
    }

    reply->echoed_command = raw[2];
    reply->status_code = raw[3];
    reply->state_bits = raw[4];
    reply->pulse_bits = raw[5];
    return true;
}

bool Rp2350PacsProtocol::parse_rgb_status_reply(const unsigned char* raw, unsigned int bytes_read,
                                                RgbStatusReply* reply) {
    if (raw == nullptr || reply == nullptr) {
        return false;
    }
    zero_reply(reply->raw, sizeof(reply->raw), &reply->bytes_read);
    reply->current_color = 0U;
    reply->default_color = 0U;
    reply->pulse_active = 0U;

    if (bytes_read > sizeof(reply->raw)) {
        return false;
    }
    for (unsigned int i = 0; i < bytes_read; ++i) {
        reply->raw[i] = raw[i];
    }
    reply->bytes_read = bytes_read;

    if (bytes_read < 5U || raw[0] != 0x52U || raw[1] != 0x47U) {
        return false;
    }

    reply->current_color = raw[2];
    reply->default_color = raw[3];
    reply->pulse_active = raw[4];
    return true;
}

bool Rp2350PacsProtocol::parse_rgb_command_reply(const unsigned char* raw, unsigned int bytes_read,
                                                 unsigned char expected_command,
                                                 RgbCommandReply* reply) {
    if (raw == nullptr || reply == nullptr) {
        return false;
    }
    zero_reply(reply->raw, sizeof(reply->raw), &reply->bytes_read);
    reply->echoed_command = 0U;
    reply->status_code = 0U;
    reply->current_color = 0U;

    if (bytes_read > sizeof(reply->raw)) {
        return false;
    }
    for (unsigned int i = 0; i < bytes_read; ++i) {
        reply->raw[i] = raw[i];
    }
    reply->bytes_read = bytes_read;

    if (bytes_read < 5U || raw[0] != 0x52U || raw[1] != 0x43U || raw[2] != expected_command) {
        return false;
    }

    reply->echoed_command = raw[2];
    reply->status_code = raw[3];
    reply->current_color = raw[4];
    return true;
}

bool Rp2350PacsProtocol::parse_next_event_reply(const unsigned char* raw, unsigned int bytes_read,
                                                unsigned char interface_index,
                                                ControllerEvent* event) {
    if (raw == nullptr || event == nullptr || bytes_read > kEventReadLength) {
        return false;
    }

    event->queue_empty = false;
    event->interface_index = interface_index;
    event->source = 0U;
    event->type = 0U;
    event->code = 0U;
    event->payload_length = 0U;
    event->sequence = 0U;
    for (unsigned int i = 0; i < sizeof(event->payload); ++i) {
        event->payload[i] = 0U;
    }

    if (bytes_read < 6U) {
        return false;
    }

    if (raw[0] == 0U && raw[1] == 0U && raw[2] == 0U && raw[3] == 0U && raw[4] == 0U &&
        raw[5] == 0U) {
        event->queue_empty = true;
        return true;
    }

    event->type = raw[0];
    event->source = raw[1];
    event->code = raw[2];
    event->payload_length = raw[3];
    event->sequence =
        static_cast<uint16_t>(static_cast<uint16_t>(raw[4]) | (static_cast<uint16_t>(raw[5]) << 8U));

    if (6U + event->payload_length > bytes_read || event->payload_length > sizeof(event->payload)) {
        return false;
    }

    for (unsigned int i = 0; i < event->payload_length; ++i) {
        event->payload[i] = raw[6U + i];
    }

    return true;
}

}  // namespace i2c_bus
