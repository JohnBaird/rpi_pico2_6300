#pragma once

namespace i2c_bus {

class I2cManager {
  public:
    I2cManager();

    bool init();
    void service();
    bool status() const;
    const char* last_error() const;

  private:
    bool probe_address(unsigned char address) const;
    bool read_rtc();
    void scan_bus();
    void probe_expected_devices();
    void probe_wiegand_processors();
    void probe_lcd();

    bool initialized_;
    const char* last_error_;
};

}  // namespace i2c_bus
