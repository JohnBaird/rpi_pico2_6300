#include "i2c/PiWiegandDeviceManager.h"

#include <cstdio>

namespace i2c_bus {
PiWiegandDeviceManager::PiWiegandDeviceManager()
    : devices_{}, device_count_(0), runtime_config_(nullptr), wiegand_builder_() {}

void PiWiegandDeviceManager::init(const I2cCommandTransport* transport, unsigned char base_address,
                                  unsigned char device_count,
                                  const config::RuntimeConfig* runtime_config) {
    device_count_ = device_count <= 8U ? device_count : 8U;
    runtime_config_ = runtime_config;
    for (unsigned char i = 0; i < device_count_; ++i) {
        devices_[i].configure(i, static_cast<unsigned char>(base_address + i), transport);
    }
}

bool PiWiegandDeviceManager::build_configured_wiegand_bit_string(unsigned char interface_index,
                                                                 uint32_t card_number,
                                                                 char* destination,
                                                                 unsigned int destination_size) const {
    if (runtime_config_ == nullptr || destination == nullptr ||
        interface_index >= config::kMaxWiegandInterfaces) {
        return false;
    }

    const char* format_name = runtime_config_->wiegand.output_formats[interface_index];
    if (format_name == nullptr || format_name[0] == '\0') {
        return false;
    }

    wiegand::WiegandFormatDefinition definition{};
    if (!wiegand_builder_.load_format_definition(format_name, &definition)) {
        return false;
    }

    return wiegand_builder_.build_bit_string(definition,
                                             runtime_config_->wiegand.output_facility_codes[interface_index],
                                             card_number, destination, destination_size);
}

bool PiWiegandDeviceManager::send_configured_wiegand_frame(
    unsigned char interface_index, uint32_t card_number, ConfiguredWiegandSendResult* result) const {
    if (result == nullptr) {
        return false;
    }

    result->success = false;
    result->interface_index = interface_index;
    result->address = 0U;
    result->format_name[0] = '\0';
    result->facility_code = 0U;
    result->card_number = card_number;
    result->bit_string[0] = '\0';
    result->send_result = WiegandOutSendResult{};
    result->error_reason = "unknown";

    if (runtime_config_ == nullptr) {
        result->error_reason = "runtime_config_missing";
        return false;
    }

    if (interface_index >= device_count_ || interface_index >= config::kMaxWiegandInterfaces) {
        result->error_reason = "invalid_interface";
        return false;
    }

    const PiWiegandI2cDevice& device = devices_[interface_index];
    result->address = device.address();
    result->facility_code = runtime_config_->wiegand.output_facility_codes[interface_index];

    const char* format_name = runtime_config_->wiegand.output_formats[interface_index];
    if (format_name == nullptr || format_name[0] == '\0') {
        result->error_reason = "format_missing";
        return false;
    }

    const int copied = std::snprintf(result->format_name, sizeof(result->format_name), "%s", format_name);
    if (copied <= 0 || static_cast<unsigned int>(copied) >= sizeof(result->format_name)) {
        result->error_reason = "format_name_invalid";
        return false;
    }

    if (card_number == 0U) {
        result->error_reason = "card_number_invalid";
        return false;
    }

    if (!device.is_present()) {
        result->error_reason = "device_missing";
        return false;
    }

    if (!build_configured_wiegand_bit_string(interface_index, card_number, result->bit_string,
                                             sizeof(result->bit_string))) {
        result->error_reason = "bit_string_build_failed";
        return false;
    }

    if (!device.send_wiegand_out(result->bit_string, &result->send_result)) {
        result->error_reason = "i2c_send_failed";
        return false;
    }

    result->success = true;
    result->error_reason = "ok";
    return true;
}

void PiWiegandDeviceManager::probe_and_log_devices() const {
    for (unsigned char i = 0; i < device_count_; ++i) {
        PiWiegandI2cDevice& device = const_cast<PiWiegandI2cDevice&>(devices_[i]);
        if (!device.probe_presence()) {
            std::printf("I2C: Wiegand processor %u at 0x%02X missing\n", device.interface_index(),
                        device.address());
            continue;
        }

        std::printf("I2C: Wiegand processor %u detected at 0x%02X\n", device.interface_index(),
                    device.address());

        char build_info[129] = {0};
        std::printf("I2C: sending build-info probe to interface %u address 0x%02X command=0x78\n",
                    device.interface_index(), device.address());
        if (!device.read_build_info(build_info, sizeof(build_info))) {
            std::printf("I2C: build-info probe failed for interface %u at 0x%02X command=0x78\n",
                        device.interface_index(), device.address());
            continue;
        }

        std::printf("I2C: build-info response for interface %u at 0x%02X: %s\n",
                    device.interface_index(), device.address(), build_info);

        std::printf("I2C: sending wiegand-out status probe to interface %u address 0x%02X command=0x21\n",
                    device.interface_index(), device.address());

        WiegandOutStatusResult status_result{};
        if (!device.read_wiegand_out_status(&status_result)) {
            std::printf(
                "I2C: wiegand-out status probe failed for interface %u at 0x%02X command=0x21\n",
                device.interface_index(), device.address());
            continue;
        }

        std::printf("I2C: wiegand-out status response for interface %u at 0x%02X bytes=%u raw=",
                    device.interface_index(), device.address(), status_result.bytes_read);
        for (unsigned int raw_index = 0; raw_index < status_result.bytes_read; ++raw_index) {
            std::printf("%s%02X", raw_index == 0U ? "" : " ", status_result.raw[raw_index]);
        }
        std::printf("\n");

        if (status_result.bytes_read >= 5U) {
            std::printf(
                "I2C: wiegand-out status decoded interface=%u echoed_command=0x%02X busy=%u queued=%u\n",
                device.interface_index(), status_result.echoed_command, status_result.busy_flag,
                status_result.queued_frame_count);
        }

        if (runtime_config_ != nullptr && runtime_config_->wiegand.output_enabled) {
            const uint32_t test_card_number =
                runtime_config_->wiegand.test_card_numbers[device.interface_index()];
            ConfiguredWiegandSendResult send_result{};
            if (!send_configured_wiegand_frame(device.interface_index(), test_card_number,
                                               &send_result)) {
                std::printf(
                    "I2C: shared wiegand send failed interface=%u address=0x%02X format=%s facility_code=%lu card_number=%lu reason=%s\n",
                    send_result.interface_index, send_result.address,
                    send_result.format_name[0] != '\0' ? send_result.format_name : "(unset)",
                    static_cast<unsigned long>(send_result.facility_code),
                    static_cast<unsigned long>(send_result.card_number),
                    send_result.error_reason != nullptr ? send_result.error_reason : "(null)");
            } else {
                std::printf(
                    "I2C: sending wiegand-out test frame to interface %u address 0x%02X command=0x20 format=%s facility_code=%lu card_number=%lu bit_string=%s\n",
                    send_result.interface_index, send_result.address, send_result.format_name,
                    static_cast<unsigned long>(send_result.facility_code),
                    static_cast<unsigned long>(send_result.card_number), send_result.bit_string);

                std::printf("I2C: wiegand-out send response for interface %u at 0x%02X bytes=%u raw=",
                            send_result.interface_index, send_result.address,
                            send_result.send_result.bytes_read);
                for (unsigned int raw_index = 0; raw_index < send_result.send_result.bytes_read;
                     ++raw_index) {
                    std::printf("%s%02X", raw_index == 0U ? "" : " ",
                                send_result.send_result.raw[raw_index]);
                }
                std::printf("\n");

                if (send_result.send_result.bytes_read >= 4U) {
                    std::printf(
                        "I2C: wiegand-out send decoded interface=%u echoed_command=0x%02X status=0x%02X\n",
                        send_result.interface_index, send_result.send_result.echoed_command,
                        send_result.send_result.status_code);
                }

                std::printf(
                    "I2C: re-reading wiegand-out status after test send for interface %u address 0x%02X command=0x21\n",
                    device.interface_index(), device.address());
                if (device.read_wiegand_out_status(&status_result)) {
                    std::printf(
                        "I2C: post-send wiegand-out status for interface %u at 0x%02X bytes=%u raw=",
                        device.interface_index(), device.address(), status_result.bytes_read);
                    for (unsigned int raw_index = 0; raw_index < status_result.bytes_read;
                         ++raw_index) {
                        std::printf("%s%02X", raw_index == 0U ? "" : " ",
                                    status_result.raw[raw_index]);
                    }
                    std::printf("\n");
                    if (status_result.bytes_read >= 5U) {
                        std::printf(
                            "I2C: post-send wiegand-out status decoded interface=%u echoed_command=0x%02X busy=%u queued=%u\n",
                            device.interface_index(), status_result.echoed_command,
                            status_result.busy_flag, status_result.queued_frame_count);
                    }
                }
            }
        }

        std::printf("I2C: sending event-read probe to interface %u address 0x%02X command=0x79\n",
                    device.interface_index(), device.address());

        EventReadResult event_result{};
        if (!device.read_next_event(&event_result)) {
            std::printf("I2C: event-read probe failed for interface %u at 0x%02X command=0x79\n",
                        device.interface_index(), device.address());
            continue;
        }

        std::printf("I2C: event-read response for interface %u at 0x%02X bytes=%u raw=",
                    device.interface_index(), device.address(), event_result.bytes_read);
        for (unsigned int raw_index = 0; raw_index < event_result.bytes_read; ++raw_index) {
            std::printf("%s%02X", raw_index == 0U ? "" : " ", event_result.raw[raw_index]);
        }
        std::printf("\n");

        if (event_result.bytes_read >= 6U && event_result.raw[0] == 0U && event_result.raw[1] == 0U &&
            event_result.raw[2] == 0U && event_result.raw[3] == 0U && event_result.raw[4] == 0U &&
            event_result.raw[5] == 0U) {
            std::printf("I2C: event-read queue empty for interface %u at 0x%02X\n",
                        device.interface_index(), device.address());
            continue;
        }

        if (event_result.bytes_read >= 6U) {
            const unsigned int payload_length = event_result.raw[3];
            const unsigned int sequence = static_cast<unsigned int>(event_result.raw[4]) |
                                          static_cast<unsigned int>(event_result.raw[5] << 8U);
            std::printf(
                "I2C: event-read header type=0x%02X source=0x%02X code=0x%02X payload_length=%u sequence=%u\n",
                event_result.raw[0], event_result.raw[1], event_result.raw[2], payload_length,
                sequence);
        }
    }
}

}  // namespace i2c_bus
