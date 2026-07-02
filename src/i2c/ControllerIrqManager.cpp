#include "i2c/ControllerIrqManager.h"

#include <cstdio>

#include "hardware/gpio.h"
#include "pico/time.h"

namespace i2c_bus {
namespace {

constexpr ControllerIrqManager::IrqPinConfig kIrqPins[ControllerIrqManager::kIrqCount] = {
    {0U, 14U},
    {1U, 2U},
    {2U, 3U},
    {3U, 7U},
};

constexpr uint64_t kPollIntervalUs = 10U * 1000U;
constexpr unsigned int kMaxEventsPerServicePass = 8U;
constexpr uint64_t kMaxServiceBudgetUs = 3000U;

}  // namespace

ControllerIrqManager::ControllerIrqManager()
    : device_manager_(nullptr), initialized_(false), enabled_bitmap_(0U), next_poll_at_us_(0U) {}

bool ControllerIrqManager::init(PiWiegandDeviceManager* device_manager) {
    device_manager_ = device_manager;
    enabled_bitmap_ = 0U;
    initialized_ = false;

    if (device_manager_ == nullptr) {
        return false;
    }

    for (const IrqPinConfig& config : kIrqPins) {
        PacsControllerDevice* device = device_manager_->find_device_by_interface(config.interface_index);
        if (device == nullptr || !device->is_present()) {
            continue;
        }

        gpio_init(config.gpio_pin);
        gpio_set_dir(config.gpio_pin, GPIO_IN);
        gpio_pull_up(config.gpio_pin);
        enabled_bitmap_ |= static_cast<uint32_t>(1UL << config.interface_index);
    }

    next_poll_at_us_ = time_us_64() + kPollIntervalUs;
    initialized_ = true;
    log_enabled_interfaces();
    return true;
}

void ControllerIrqManager::service() {
    if (!initialized_ || enabled_bitmap_ == 0U) {
        return;
    }

    const uint64_t now_us = time_us_64();
    if (now_us < next_poll_at_us_) {
        return;
    }
    next_poll_at_us_ = now_us + kPollIntervalUs;

    for (const IrqPinConfig& config : kIrqPins) {
        if (!is_interface_enabled(config.interface_index) || !is_irq_asserted(config)) {
            continue;
        }
        service_interface(config);
    }
}

void ControllerIrqManager::log_enabled_interfaces() const {
    if (enabled_bitmap_ == 0U) {
        std::puts("IRQ: no controller IRQ inputs enabled");
        return;
    }

    for (const IrqPinConfig& config : kIrqPins) {
        if (!is_interface_enabled(config.interface_index)) {
            continue;
        }
        std::printf(
            "IRQ: enabled interface=%u gpio=GP%u mode=input pullup poll_interval_ms=10 active_low=true\n",
            config.interface_index, config.gpio_pin);
    }
}

void ControllerIrqManager::service_interface(const IrqPinConfig& config) {
    if (device_manager_ == nullptr) {
        return;
    }

    PacsControllerDevice* device = device_manager_->find_device_by_interface(config.interface_index);
    if (device == nullptr || !device->is_present()) {
        return;
    }

    std::printf("IRQ: interface=%u gpio=GP%u level=LOW draining events address=0x%02X\n",
                config.interface_index, config.gpio_pin, device->address());

    const uint64_t started_at_us = time_us_64();
    unsigned int drained_events = 0U;
    while (drained_events < kMaxEventsPerServicePass &&
           (time_us_64() - started_at_us) < kMaxServiceBudgetUs) {
        Rp2350PacsProtocol::EventReadReply reply{};
        if (!device->read_next_event(&reply)) {
            std::printf("IRQ: event-read failed interface=%u address=0x%02X\n",
                        config.interface_index, device->address());
            return;
        }

        Rp2350PacsProtocol::ControllerEvent event{};
        if (!Rp2350PacsProtocol::parse_next_event_reply(reply.raw, reply.bytes_read,
                                                        config.interface_index, &event)) {
            std::printf("IRQ: event decode failed interface=%u address=0x%02X bytes=%u\n",
                        config.interface_index, device->address(), reply.bytes_read);
            return;
        }

        if (event.queue_empty) {
            if (drained_events > 0U) {
                std::printf("IRQ: interface=%u queue empty after draining %u event(s)\n",
                            config.interface_index, drained_events);
            }
            return;
        }

        std::printf("IRQ Event: interface=%u gpio=GP%u address=0x%02X type=0x%02X source=0x%02X code=0x%02X payload_length=%u sequence=%u raw=",
                    config.interface_index, config.gpio_pin, device->address(), event.type,
                    event.source, event.code, event.payload_length, event.sequence);
        for (unsigned int raw_index = 0; raw_index < reply.bytes_read; ++raw_index) {
            std::printf("%s%02X", raw_index == 0U ? "" : " ", reply.raw[raw_index]);
        }
        std::printf("\n");
        ++drained_events;
    }

    if (is_irq_asserted(config)) {
        std::printf(
            "IRQ: interface=%u gpio=GP%u drain budget reached drained=%u will_continue_next_cycle\n",
            config.interface_index, config.gpio_pin, drained_events);
    }
}

bool ControllerIrqManager::is_interface_enabled(unsigned char interface_index) const {
    return (enabled_bitmap_ & static_cast<uint32_t>(1UL << interface_index)) != 0U;
}

bool ControllerIrqManager::is_irq_asserted(const IrqPinConfig& config) const {
    return gpio_get(config.gpio_pin) == 0U;
}

}  // namespace i2c_bus
