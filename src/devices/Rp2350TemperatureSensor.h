#pragma once

namespace devices {

class Rp2350TemperatureSensor {
  public:
    Rp2350TemperatureSensor();

    bool init();
    bool status() const;
    const char* last_error() const;
    bool read_temperature_c(float* out_temperature_c);
    const char* sensor_name() const;

  private:
    bool initialized_;
    const char* last_error_;
};

}  // namespace devices
