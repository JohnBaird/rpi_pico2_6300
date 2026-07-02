#pragma once

#include <cstdint>

namespace i2c_bus {

class Rp2350PacsProtocol {
  public:
    static constexpr unsigned char kCmdWiegandOutSend = 0x20;
    static constexpr unsigned char kCmdWiegandOutStatus = 0x21;
    static constexpr unsigned char kCmdOutputStatus = 0x40;
    static constexpr unsigned char kCmdOutputSet = 0x41;
    static constexpr unsigned char kCmdOutputPulse = 0x42;
    static constexpr unsigned char kCmdRgbStatus = 0x50;
    static constexpr unsigned char kCmdRgbSet = 0x51;
    static constexpr unsigned char kCmdRgbPulse = 0x52;
    static constexpr unsigned char kCmdUpload = 0x6C;
    static constexpr unsigned char kCmdBuildInfoRead = 0x78;
    static constexpr unsigned char kCmdEventRead = 0x79;
    static constexpr unsigned char kCmdRuntimeConfigRead = 0x7A;

    static constexpr unsigned int kBuildInfoMaxLength = 128;
    static constexpr unsigned int kRuntimeConfigMaxLength = 1024;
    static constexpr unsigned int kEventReadLength = 32;

    struct WiegandOutSendReply {
        unsigned char raw[4];
        unsigned int bytes_read;
        unsigned char status_code;
        unsigned char echoed_command;
    };

    struct WiegandOutStatusReply {
        unsigned char raw[5];
        unsigned int bytes_read;
        unsigned char echoed_command;
        unsigned char busy_flag;
        unsigned char queued_frame_count;
    };

    struct OutputStatusReply {
        unsigned char raw[5];
        unsigned int bytes_read;
        unsigned char echoed_command;
        unsigned char state_bits;
        unsigned char pulse_bits;
    };

    struct OutputCommandReply {
        unsigned char raw[6];
        unsigned int bytes_read;
        unsigned char echoed_command;
        unsigned char status_code;
        unsigned char state_bits;
        unsigned char pulse_bits;
    };

    struct RgbStatusReply {
        unsigned char raw[5];
        unsigned int bytes_read;
        unsigned char current_color;
        unsigned char default_color;
        unsigned char pulse_active;
    };

    struct RgbCommandReply {
        unsigned char raw[5];
        unsigned int bytes_read;
        unsigned char echoed_command;
        unsigned char status_code;
        unsigned char current_color;
    };

    struct EventReadReply {
        unsigned char raw[kEventReadLength];
        unsigned int bytes_read;
    };

    struct ControllerEvent {
        bool queue_empty;
        unsigned char interface_index;
        unsigned char source;
        unsigned char type;
        unsigned char code;
        unsigned char payload[26];
        unsigned char payload_length;
        uint16_t sequence;
    };

    static bool build_wiegand_out_payload(const char* bit_string, unsigned char* payload,
                                          unsigned int payload_capacity,
                                          unsigned int* payload_length);
    static bool build_output_set_payload(unsigned char mask, unsigned char values,
                                         unsigned char* payload, unsigned int payload_capacity,
                                         unsigned int* payload_length);
    static bool build_output_pulse_payload(unsigned char mask, uint16_t duration_ms,
                                           unsigned char* payload,
                                           unsigned int payload_capacity,
                                           unsigned int* payload_length);
    static bool build_rgb_set_payload(unsigned char color_code, unsigned char* payload,
                                      unsigned int payload_capacity,
                                      unsigned int* payload_length);
    static bool build_rgb_pulse_payload(unsigned char color_code, uint16_t duration_ms,
                                        unsigned char* payload, unsigned int payload_capacity,
                                        unsigned int* payload_length);

    static bool parse_wiegand_out_send_reply(const unsigned char* raw, unsigned int bytes_read,
                                             WiegandOutSendReply* reply);
    static bool parse_wiegand_out_status_reply(const unsigned char* raw, unsigned int bytes_read,
                                               WiegandOutStatusReply* reply);
    static bool parse_output_status_reply(const unsigned char* raw, unsigned int bytes_read,
                                          OutputStatusReply* reply);
    static bool parse_output_command_reply(const unsigned char* raw, unsigned int bytes_read,
                                           unsigned char expected_command,
                                           OutputCommandReply* reply);
    static bool parse_rgb_status_reply(const unsigned char* raw, unsigned int bytes_read,
                                       RgbStatusReply* reply);
    static bool parse_rgb_command_reply(const unsigned char* raw, unsigned int bytes_read,
                                        unsigned char expected_command,
                                        RgbCommandReply* reply);
    static bool parse_next_event_reply(const unsigned char* raw, unsigned int bytes_read,
                                       unsigned char interface_index, ControllerEvent* event);
};

}  // namespace i2c_bus
