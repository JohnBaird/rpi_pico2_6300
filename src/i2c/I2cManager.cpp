#include "i2c/I2cManager.h"

#include <cstdio>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

namespace i2c_bus {
namespace {

constexpr i2c_inst_t* kI2cInstance = i2c0;
constexpr uint kSdaPin = PICO_DEFAULT_I2C_SDA_PIN;
constexpr uint kSclPin = PICO_DEFAULT_I2C_SCL_PIN;
constexpr uint32_t kI2cBaudRate = 100 * 1000;
constexpr uint32_t kShortTransferTimeoutUs = 4000;
constexpr uint32_t kLongTransferTimeoutUs = 20000;
constexpr uint8_t kRtcAddress = 0x68;
constexpr uint8_t kLcdAddressA = 0x27;
constexpr uint8_t kLcdAddressB = 0x3F;
constexpr uint8_t kWiegandBaseAddress = 0x30;
constexpr uint8_t kWiegandCount = 8;
constexpr uint32_t kCommandSettleDelayUs = 2000;

uint8_t bcd_to_decimal(uint8_t value) {
    return static_cast<uint8_t>(((value >> 4U) * 10U) + (value & 0x0FU));
}

}  // namespace

I2cManager::I2cManager()
    : transport_(kI2cInstance, kShortTransferTimeoutUs, kLongTransferTimeoutUs,
                 kCommandSettleDelayUs),
      wiegand_device_manager_(),
      controller_irq_manager_(),
      initialized_(false),
      last_error_("not_initialized") {}

bool I2cManager::init(const config::RuntimeConfig& runtime_config) {
    std::printf("I2C: initializing i2c0 on SDA=GP%u SCL=GP%u\n", kSdaPin, kSclPin);

    i2c_init(kI2cInstance, kI2cBaudRate);
    gpio_set_function(kSdaPin, GPIO_FUNC_I2C);
    gpio_set_function(kSclPin, GPIO_FUNC_I2C);
    gpio_pull_up(kSdaPin);
    gpio_pull_up(kSclPin);

    wiegand_device_manager_.init(&transport_, kWiegandBaseAddress, kWiegandCount, &runtime_config);

    scan_bus();
    probe_expected_devices();
    probe_lcd();
    wiegand_device_manager_.probe_and_log_devices();
    controller_irq_manager_.init(&wiegand_device_manager_);

    if (!read_rtc()) {
        std::puts("I2C: RTC read skipped or failed");
    }

    initialized_ = true;
    last_error_ = "ok";
    return true;
}

bool I2cManager::send_configured_wiegand_frame(unsigned char interface_index, uint32_t card_number,
                                               ConfiguredWiegandSendResult* result) const {
    return wiegand_device_manager_.send_configured_wiegand_frame(interface_index, card_number, result);
}

void I2cManager::service() { controller_irq_manager_.service(); }

bool I2cManager::status() const { return initialized_; }

const char* I2cManager::last_error() const { return last_error_; }

bool I2cManager::read_rtc() {
    if (!transport_.probe_address(kRtcAddress)) {
        std::printf("I2C: RTC not detected at 0x%02X\n", kRtcAddress);
        return false;
    }

    uint8_t register_index = 0x00;
    if (i2c_write_timeout_us(kI2cInstance, kRtcAddress, &register_index, 1, true,
                             kShortTransferTimeoutUs) != 1) {
        std::puts("I2C: RTC register select failed");
        return false;
    }

    uint8_t rtc_data[7] = {0};
    if (i2c_read_timeout_us(kI2cInstance, kRtcAddress, rtc_data, sizeof(rtc_data), false,
                            kShortTransferTimeoutUs) !=
        static_cast<int>(sizeof(rtc_data))) {
        std::puts("I2C: RTC read failed");
        return false;
    }

    const uint8_t seconds = bcd_to_decimal(static_cast<uint8_t>(rtc_data[0] & 0x7FU));
    const uint8_t minutes = bcd_to_decimal(static_cast<uint8_t>(rtc_data[1] & 0x7FU));
    const uint8_t hours = bcd_to_decimal(static_cast<uint8_t>(rtc_data[2] & 0x3FU));
    const uint8_t day = bcd_to_decimal(static_cast<uint8_t>(rtc_data[4] & 0x3FU));
    const uint8_t month = bcd_to_decimal(static_cast<uint8_t>(rtc_data[5] & 0x1FU));
    const uint8_t year = bcd_to_decimal(rtc_data[6]);

    std::printf("I2C: RTC DS3231 time 20%02u-%02u-%02u %02u:%02u:%02u\n", year, month, day, hours,
                minutes, seconds);
    return true;
}

void I2cManager::scan_bus() {
    std::puts("I2C: scanning addresses 0x08 to 0x77");

    for (uint8_t address = 0x08; address <= 0x77; ++address) {
        if (transport_.probe_address(address)) {
            std::printf("I2C: found device at 0x%02X\n", address);
        }
    }
}

void I2cManager::probe_expected_devices() {
    std::printf("I2C: RTC probe at 0x%02X -> %s\n", kRtcAddress,
                transport_.probe_address(kRtcAddress) ? "present" : "missing");
    std::printf("I2C: LCD probe at 0x%02X -> %s\n", kLcdAddressA,
                transport_.probe_address(kLcdAddressA) ? "present" : "missing");
    std::printf("I2C: LCD probe at 0x%02X -> %s\n", kLcdAddressB,
                transport_.probe_address(kLcdAddressB) ? "present" : "missing");
}

void I2cManager::probe_lcd() {
    if (transport_.probe_address(kLcdAddressA)) {
        std::printf("I2C: LCD candidate present at 0x%02X\n", kLcdAddressA);
        return;
    }

    if (transport_.probe_address(kLcdAddressB)) {
        std::printf("I2C: LCD candidate present at 0x%02X\n", kLcdAddressB);
        return;
    }

    std::puts("I2C: LCD not detected at 0x27 or 0x3F");
}

}  // namespace i2c_bus
