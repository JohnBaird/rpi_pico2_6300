#include "devices/Rp2350TemperatureSensor.h"

#include <cstdio>

#include "hardware/adc.h"

namespace devices {
namespace {

constexpr uint kTemperatureSensorAdcChannel = 4;
constexpr const char* kSensorName = "CPU_temp";

}  // namespace

Rp2350TemperatureSensor::Rp2350TemperatureSensor()
    : initialized_(false), last_error_("not_initialized") {}

bool Rp2350TemperatureSensor::init() {
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(kTemperatureSensorAdcChannel);

    float temperature_c = 0.0f;
    if (!read_temperature_c(&temperature_c)) {
        std::printf("Temperature: RP2350 internal sensor unavailable (%s)\n", last_error_);
        initialized_ = true;
        return false;
    }

    initialized_ = true;
    last_error_ = "ok";
    std::printf("Temperature: RP2350 internal sensor initial=%.2fC\n", temperature_c);
    return true;
}

bool Rp2350TemperatureSensor::status() const { return initialized_; }

const char* Rp2350TemperatureSensor::last_error() const { return last_error_; }

bool Rp2350TemperatureSensor::read_temperature_c(float* out_temperature_c) {
    if (out_temperature_c == nullptr) {
        last_error_ = "output_pointer_null";
        return false;
    }

    adc_select_input(kTemperatureSensorAdcChannel);
    const uint16_t raw = adc_read();
    const float conversion = 3.3f / static_cast<float>(1u << 12);
    const float voltage = static_cast<float>(raw) * conversion;
    *out_temperature_c = 27.0f - ((voltage - 0.706f) / 0.001721f);
    last_error_ = "ok";
    return true;
}

const char* Rp2350TemperatureSensor::sensor_name() const { return kSensorName; }

}  // namespace devices
