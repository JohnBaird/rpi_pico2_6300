#include "devices/LedManager.h"

#include "hardware/gpio.h"
#include "pico/time.h"

namespace devices {

LedManager::LedManager(const config::LedConfig& config)
    : config_(config),
      initialized_(false),
      current_output_on_(false),
      next_toggle_at_ms_(0),
      last_error_("not_initialized") {}

bool LedManager::init() {
    if (config_.gpio_pin < 0) {
        last_error_ = "invalid_led_pin";
        return false;
    }

    gpio_init(static_cast<uint>(config_.gpio_pin));
    gpio_set_dir(static_cast<uint>(config_.gpio_pin), GPIO_OUT);

    current_output_on_ = true;
    apply_output(current_output_on_);
    next_toggle_at_ms_ = to_ms_since_boot(get_absolute_time()) + config_.healthy_on_ms;

    initialized_ = true;
    last_error_ = "ok";
    return true;
}

void LedManager::service() {
    if (!initialized_) {
        return;
    }

    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if (static_cast<int32_t>(now_ms - next_toggle_at_ms_) < 0) {
        return;
    }

    current_output_on_ = !current_output_on_;
    apply_output(current_output_on_);
    next_toggle_at_ms_ =
        now_ms + (current_output_on_ ? config_.healthy_on_ms : config_.healthy_off_ms);
}

bool LedManager::status() const { return initialized_; }

const char* LedManager::last_error() const { return last_error_; }

void LedManager::apply_output(bool enabled) {
    const bool pin_value = config_.active_high ? enabled : !enabled;
    gpio_put(static_cast<uint>(config_.gpio_pin), pin_value ? 1 : 0);
}

}  // namespace devices
