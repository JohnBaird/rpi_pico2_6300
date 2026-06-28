#include "devices/Ds2401Manager.h"

#include <cstdio>

#include "hardware/gpio.h"
#include "pico/time.h"

namespace devices {
namespace {

constexpr uint8_t kReadRomCommand = 0x33;
constexpr uint8_t kDs2401FamilyCode = 0x01;
constexpr uint8_t kMacUnicastMask = 0xFE;
constexpr uint8_t kMacLocallyAdministeredBit = 0x02;
constexpr unsigned int kReadRomMaxAttempts = 5;
constexpr uint32_t kReadRomRetryDelayMs = 5;

}  // namespace

Ds2401Manager::Ds2401Manager(const config::OneWireConfig& config)
    : config_(config), initialized_(false), last_error_("not_initialized") {}

bool Ds2401Manager::init() {
    if (config_.gpio_pin < 0) {
        last_error_ = "invalid_one_wire_pin";
        return false;
    }

    gpio_init(static_cast<uint>(config_.gpio_pin));
    if (config_.use_internal_pull_up) {
        gpio_pull_up(static_cast<uint>(config_.gpio_pin));
    } else {
        gpio_disable_pulls(static_cast<uint>(config_.gpio_pin));
    }
    gpio_set_dir(static_cast<uint>(config_.gpio_pin), GPIO_IN);

    initialized_ = true;
    last_error_ = "ok";
    return true;
}

bool Ds2401Manager::read_mac_address(char* destination, unsigned int destination_size) {
    if (!initialized_) {
        last_error_ = "not_initialized";
        return false;
    }

    if (destination == nullptr || destination_size < 18) {
        last_error_ = "mac_buffer_too_small";
        return false;
    }

    uint8_t rom[8] = {0};
    if (!read_rom(rom)) {
        return false;
    }

    uint8_t mac[6] = {rom[1], rom[2], rom[3], rom[4], rom[5], rom[6]};
    // DS2401 bytes 1..6 are the 48-bit serial number. We derive a standards-compliant
    // unicast, locally-administered MAC from those 6 bytes by adjusting only the first-byte
    // address-control bits.
    mac[0] = static_cast<uint8_t>((mac[0] & kMacUnicastMask) | kMacLocallyAdministeredBit);

    const int bytes_written =
        std::snprintf(destination, destination_size, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],
                      mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (bytes_written != 17) {
        last_error_ = "mac_format_failed";
        return false;
    }

    std::printf(
        "DS2401: family=%02X serial=%02X%02X%02X%02X%02X%02X crc=%02X crc_ok=true "
        "derived_mac=%s on GP%d\n",
        rom[0], rom[1], rom[2], rom[3], rom[4], rom[5], rom[6], rom[7], destination,
        config_.gpio_pin);
    last_error_ = "ok";
    return true;
}

bool Ds2401Manager::status() const { return initialized_; }

const char* Ds2401Manager::last_error() const { return last_error_; }

bool Ds2401Manager::read_rom(uint8_t rom[8]) {
    for (unsigned int attempt = 1; attempt <= kReadRomMaxAttempts; ++attempt) {
        if (read_rom_once(rom)) {
            return true;
        }
        if (attempt < kReadRomMaxAttempts) {
            sleep_ms(kReadRomRetryDelayMs);
        }
    }
    return false;
}

bool Ds2401Manager::read_rom_once(uint8_t rom[8]) {
    if (!reset_and_detect_presence()) {
        last_error_ = "device_not_present";
        return false;
    }

    write_byte(kReadRomCommand);
    for (unsigned int index = 0; index < 8; ++index) {
        rom[index] = read_byte();
    }

    if (rom[0] != kDs2401FamilyCode) {
        last_error_ = "unexpected_family_code";
        return false;
    }

    if (compute_crc8(rom, 7) != rom[7]) {
        last_error_ = "crc_check_failed";
        return false;
    }

    return true;
}

bool Ds2401Manager::reset_and_detect_presence() const {
    drive_bus_low();
    sleep_us(480);
    release_bus();
    sleep_us(70);
    const bool device_present = gpio_get(static_cast<uint>(config_.gpio_pin)) == 0;
    sleep_us(410);
    return device_present;
}

void Ds2401Manager::drive_bus_low() const {
    gpio_put(static_cast<uint>(config_.gpio_pin), 0);
    gpio_set_dir(static_cast<uint>(config_.gpio_pin), GPIO_OUT);
}

void Ds2401Manager::release_bus() const {
    gpio_set_dir(static_cast<uint>(config_.gpio_pin), GPIO_IN);
    if (config_.use_internal_pull_up) {
        gpio_pull_up(static_cast<uint>(config_.gpio_pin));
    }
}

void Ds2401Manager::write_bit(bool bit_value) const {
    drive_bus_low();
    if (bit_value) {
        sleep_us(6);
        release_bus();
        sleep_us(64);
        return;
    }

    sleep_us(60);
    release_bus();
    sleep_us(10);
}

bool Ds2401Manager::read_bit() const {
    drive_bus_low();
    sleep_us(6);
    release_bus();
    sleep_us(9);
    const bool bit_value = gpio_get(static_cast<uint>(config_.gpio_pin)) != 0;
    sleep_us(55);
    return bit_value;
}

void Ds2401Manager::write_byte(uint8_t value) const {
    for (unsigned int bit = 0; bit < 8; ++bit) {
        write_bit((value & 0x01u) != 0);
        value >>= 1U;
    }
}

uint8_t Ds2401Manager::read_byte() const {
    uint8_t value = 0;
    for (unsigned int bit = 0; bit < 8; ++bit) {
        if (read_bit()) {
            value |= static_cast<uint8_t>(1u << bit);
        }
    }
    return value;
}

uint8_t Ds2401Manager::compute_crc8(const uint8_t* data, unsigned int length) const {
    uint8_t crc = 0;
    for (unsigned int byte_index = 0; byte_index < length; ++byte_index) {
        uint8_t in_byte = data[byte_index];
        for (unsigned int bit = 0; bit < 8; ++bit) {
            const uint8_t mix = static_cast<uint8_t>((crc ^ in_byte) & 0x01u);
            crc >>= 1U;
            if (mix != 0) {
                crc ^= 0x8Cu;
            }
            in_byte >>= 1U;
        }
    }
    return crc;
}

}  // namespace devices
