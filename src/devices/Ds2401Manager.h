#pragma once

#include <cstdint>

#include "config/BootConfig.h"

namespace devices {

class Ds2401Manager {
  public:
    explicit Ds2401Manager(const config::OneWireConfig& config);

    bool init();
    bool read_mac_address(char* destination, unsigned int destination_size);
    bool status() const;
    const char* last_error() const;

  private:
    bool read_rom(uint8_t rom[8]);
    bool read_rom_once(uint8_t rom[8]);
    bool reset_and_detect_presence() const;
    void drive_bus_low() const;
    void release_bus() const;
    void write_bit(bool bit_value) const;
    bool read_bit() const;
    void write_byte(uint8_t value) const;
    uint8_t read_byte() const;
    uint8_t compute_crc8(const uint8_t* data, unsigned int length) const;

    const config::OneWireConfig& config_;
    bool initialized_;
    const char* last_error_;
};

}  // namespace devices
