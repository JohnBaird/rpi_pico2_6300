#include "devices/Mcp9808TemperatureSensor.h"

#include <cstdio>

namespace devices {
namespace {

constexpr i2c_inst_t* kI2cInstance = i2c0;
constexpr uint8_t kSensorAddress = 0x18;
constexpr uint8_t kAmbientTemperatureRegister = 0x05;
constexpr uint8_t kManufacturerIdRegister = 0x06;
constexpr uint8_t kDeviceIdRegister = 0x07;
constexpr uint16_t kMcp9808ManufacturerId = 0x0054;
constexpr uint16_t kMcp9808DeviceIdMask = 0xFF00;
constexpr uint16_t kMcp9808DeviceIdPrefix = 0x0400;
constexpr const char* kSensorName = "MCP9808_0x18";

}  // namespace

Mcp9808TemperatureSensor::Mcp9808TemperatureSensor()
    : initialized_(false), present_(false), last_error_("not_initialized") {}

bool Mcp9808TemperatureSensor::init() {
    if (!verify_sensor_identity()) {
        present_ = false;
        initialized_ = true;
        std::printf("Temperature: MCP9808 at 0x%02X identity check failed (%s)\n", kSensorAddress,
                    last_error_);
        return false;
    }

    uint8_t temperature_bytes[2] = {0, 0};
    if (!read_register(kAmbientTemperatureRegister, temperature_bytes, sizeof(temperature_bytes))) {
        present_ = false;
        std::printf("Temperature: MCP9808 at 0x%02X unavailable (%s)\n", kSensorAddress, last_error_);
        initialized_ = true;
        return false;
    }

    const float temperature_c = decode_temperature_c(temperature_bytes);

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

    *out_temperature_c = decode_temperature_c(temperature_bytes);
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

bool Mcp9808TemperatureSensor::verify_sensor_identity() {
    uint8_t manufacturer_bytes[2] = {0, 0};
    uint8_t device_bytes[2] = {0, 0};
    if (!read_register(kManufacturerIdRegister, manufacturer_bytes, sizeof(manufacturer_bytes)) ||
        !read_register(kDeviceIdRegister, device_bytes, sizeof(device_bytes))) {
        last_error_ = "identity_read_failed";
        return false;
    }

    const uint16_t manufacturer_id = static_cast<uint16_t>(
        (static_cast<uint16_t>(manufacturer_bytes[0]) << 8U) | manufacturer_bytes[1]);
    const uint16_t device_id =
        static_cast<uint16_t>((static_cast<uint16_t>(device_bytes[0]) << 8U) | device_bytes[1]);

    if (manufacturer_id != kMcp9808ManufacturerId ||
        (device_id & kMcp9808DeviceIdMask) != kMcp9808DeviceIdPrefix) {
        last_error_ = "identity_mismatch";
        return false;
    }

    return true;
}

float Mcp9808TemperatureSensor::decode_temperature_c(const uint8_t* temperature_bytes) const {
    if (temperature_bytes == nullptr) {
        return 0.0f;
    }

    const uint16_t register_value = static_cast<uint16_t>(
        (static_cast<uint16_t>(temperature_bytes[0]) << 8U) | temperature_bytes[1]);
    float temperature_c = static_cast<float>(register_value & 0x0FFFU) / 16.0f;
    if ((register_value & 0x1000U) != 0U) {
        temperature_c -= 256.0f;
    }
    return temperature_c;
}

}  // namespace devices
