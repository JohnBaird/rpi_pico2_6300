#include "devices/Mcp9808TemperatureSensor.h"

#include <cstdio>

namespace devices {
namespace {

constexpr i2c_inst_t* kI2cInstance = i2c0;
constexpr uint8_t kSensorAddress = 0x18;
constexpr uint8_t kAmbientTemperatureRegister = 0x05;
constexpr const char* kSensorName = "MCP9808_0x18";

}  // namespace

Mcp9808TemperatureSensor::Mcp9808TemperatureSensor()
    : initialized_(false), present_(false), last_error_("not_initialized") {}

bool Mcp9808TemperatureSensor::init() {
    uint8_t temperature_bytes[2] = {0, 0};
    if (!read_register(kAmbientTemperatureRegister, temperature_bytes, sizeof(temperature_bytes))) {
        present_ = false;
        std::printf("Temperature: MCP9808 at 0x%02X unavailable (%s)\n", kSensorAddress, last_error_);
        initialized_ = true;
        return false;
    }

    uint16_t raw_value =
        static_cast<uint16_t>(((temperature_bytes[0] & 0x1FU) << 8U) | temperature_bytes[1]);
    if (raw_value > 4095U) {
        raw_value = static_cast<uint16_t>(raw_value - 8192U);
    }
    const float temperature_c = static_cast<float>(static_cast<int16_t>(raw_value)) * 0.0625f;

    present_ = true;
    initialized_ = true;
    last_error_ = "ok";
    std::printf("Temperature: MCP9808 detected at 0x%02X initial=%.2fC\n", kSensorAddress,
                temperature_c);
    return true;
}

bool Mcp9808TemperatureSensor::status() const { return initialized_ && present_; }

const char* Mcp9808TemperatureSensor::last_error() const { return last_error_; }

bool Mcp9808TemperatureSensor::read_temperature_c(float* out_temperature_c) {
    if (out_temperature_c == nullptr) {
        last_error_ = "output_pointer_null";
        return false;
    }

    uint8_t temperature_bytes[2] = {0, 0};
    if (!read_register(kAmbientTemperatureRegister, temperature_bytes, sizeof(temperature_bytes))) {
        present_ = false;
        return false;
    }

    uint16_t raw_value =
        static_cast<uint16_t>(((temperature_bytes[0] & 0x1FU) << 8U) | temperature_bytes[1]);
    if (raw_value > 4095U) {
        raw_value = static_cast<uint16_t>(raw_value - 8192U);
    }

    *out_temperature_c = static_cast<float>(static_cast<int16_t>(raw_value)) * 0.0625f;
    present_ = true;
    initialized_ = true;
    last_error_ = "ok";
    return true;
}

const char* Mcp9808TemperatureSensor::sensor_name() const { return kSensorName; }

bool Mcp9808TemperatureSensor::read_register(uint8_t register_address, uint8_t* destination,
                                             unsigned int length) {
    if (destination == nullptr || length == 0) {
        last_error_ = "invalid_read_buffer";
        return false;
    }

    if (i2c_write_timeout_us(kI2cInstance, kSensorAddress, &register_address, 1, true, 4000) != 1) {
        last_error_ = "register_select_failed";
        return false;
    }

    if (i2c_read_timeout_us(kI2cInstance, kSensorAddress, destination, length, false, 4000) !=
        static_cast<int>(length)) {
        last_error_ = "temperature_read_failed";
        return false;
    }

    return true;
}

}  // namespace devices
