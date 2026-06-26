#pragma once

#include <cstdint>

#include "config/BootConfig.h"

namespace devices {

class LedManager {
  public:
    explicit LedManager(const config::LedConfig& config);

    bool init();
    void service();
    bool status() const;
    const char* last_error() const;

  private:
    void apply_output(bool enabled);

    config::LedConfig config_;
    bool initialized_;
    bool current_output_on_;
    uint32_t next_toggle_at_ms_;
    const char* last_error_;
};

}  // namespace devices
