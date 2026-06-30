#include "i2c/PiWiegandDeviceManager.h"

#include <cstdio>

namespace i2c_bus {
namespace {

constexpr const char* kTestWiegandBitString = "10101010";

}  // namespace

PiWiegandDeviceManager::PiWiegandDeviceManager() : devices_{}, device_count_(0) {}

void PiWiegandDeviceManager::init(const I2cCommandTransport* transport, unsigned char base_address,
                                  unsigned char device_count) {
    device_count_ = device_count <= 8U ? device_count : 8U;
    for (unsigned char i = 0; i < device_count_; ++i) {
        devices_[i].configure(i, static_cast<unsigned char>(base_address + i), transport);
    }
}

void PiWiegandDeviceManager::probe_and_log_devices() const {
    for (unsigned char i = 0; i < device_count_; ++i) {
        PiWiegandI2cDevice device = devices_[i];
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

        if (device.interface_index() == 0U) {
            std::printf(
                "I2C: sending wiegand-out test frame to interface %u address 0x%02X command=0x20 bit_string=%s\n",
                device.interface_index(), device.address(), kTestWiegandBitString);

            WiegandOutSendResult send_result{};
            if (!device.send_wiegand_out(kTestWiegandBitString, &send_result)) {
                std::printf(
                    "I2C: wiegand-out send failed for interface %u at 0x%02X command=0x20\n",
                    device.interface_index(), device.address());
                continue;
            }

            std::printf("I2C: wiegand-out send response for interface %u at 0x%02X bytes=%u raw=",
                        device.interface_index(), device.address(), send_result.bytes_read);
            for (unsigned int raw_index = 0; raw_index < send_result.bytes_read; ++raw_index) {
                std::printf("%s%02X", raw_index == 0U ? "" : " ", send_result.raw[raw_index]);
            }
            std::printf("\n");

            if (send_result.bytes_read >= 4U) {
                std::printf(
                    "I2C: wiegand-out send decoded interface=%u echoed_command=0x%02X status=0x%02X\n",
                    device.interface_index(), send_result.echoed_command, send_result.status_code);
            }

            std::printf(
                "I2C: re-reading wiegand-out status after test send for interface %u address 0x%02X command=0x21\n",
                device.interface_index(), device.address());
            if (device.read_wiegand_out_status(&status_result)) {
                std::printf(
                    "I2C: post-send wiegand-out status for interface %u at 0x%02X bytes=%u raw=",
                    device.interface_index(), device.address(), status_result.bytes_read);
                for (unsigned int raw_index = 0; raw_index < status_result.bytes_read; ++raw_index) {
                    std::printf("%s%02X", raw_index == 0U ? "" : " ", status_result.raw[raw_index]);
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
