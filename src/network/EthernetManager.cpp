#include "network/EthernetManager.h"

#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"

extern "C" {
#include "dhcp.h"
#include "wizchip_conf.h"
#include "wizchip_spi.h"
}

namespace network {
namespace {

constexpr uint8_t kDhcpSocket = 0;
constexpr uint32_t kDhcpTimeoutMs = 30000;
constexpr uint32_t kDhcpPollIntervalMs = 250;
constexpr size_t kEthernetBufferSize = 2048;

EthernetManager* g_active_manager = nullptr;
struct repeating_timer g_dhcp_timer;
uint8_t g_ethernet_buffer[kEthernetBufferSize] = {0};
wiz_NetInfo g_network_info = {};

bool parse_ipv4(const char* text, uint8_t out[4]) {
    unsigned int octets[4] = {0, 0, 0, 0};
    if (text == nullptr || out == nullptr) {
        return false;
    }

    if (std::sscanf(text, "%u.%u.%u.%u", &octets[0], &octets[1], &octets[2], &octets[3]) != 4) {
        return false;
    }

    for (int i = 0; i < 4; ++i) {
        if (octets[i] > 255U) {
            return false;
        }
        out[i] = static_cast<uint8_t>(octets[i]);
    }

    return true;
}

bool parse_mac(const char* text, uint8_t out[6]) {
    unsigned int octets[6] = {0, 0, 0, 0, 0, 0};
    if (text == nullptr || out == nullptr) {
        return false;
    }

    if (std::sscanf(text, "%x:%x:%x:%x:%x:%x", &octets[0], &octets[1], &octets[2], &octets[3],
                    &octets[4], &octets[5]) != 6) {
        return false;
    }

    for (int i = 0; i < 6; ++i) {
        if (octets[i] > 255U) {
            return false;
        }
        out[i] = static_cast<uint8_t>(octets[i]);
    }

    return true;
}

bool dhcp_timer_callback(struct repeating_timer* timer) {
    static_cast<void>(timer);
    DHCP_time_handler();
    return true;
}

void dhcp_assign_callback() {
    if (g_active_manager != nullptr) {
        g_active_manager->handle_dhcp_assigned();
    }
}

void dhcp_conflict_callback() {
    if (g_active_manager != nullptr) {
        g_active_manager->handle_dhcp_conflict();
    }
}

}  // namespace

EthernetManager::EthernetManager()
    : initialized_(false),
      dhcp_enabled_(false),
      dhcp_leased_(false),
      dhcp_conflict_(false),
      last_error_("not_initialized"),
      last_dhcp_run_ms_(0) {}

bool EthernetManager::init(const config::RuntimeConfig& runtime_config) {
    std::puts("Ethernet: initializing W6300 transport");

    g_active_manager = this;
    dhcp_enabled_ = false;
    dhcp_leased_ = false;
    dhcp_conflict_ = false;
    last_dhcp_run_ms_ = 0;
    std::memset(&g_network_info, 0, sizeof(g_network_info));
    g_network_info.dhcp = NETINFO_DHCP;
    g_network_info.ipmode = NETINFO_STATIC_ALL;

    if (!parse_mac(runtime_config.device.mac, g_network_info.mac)) {
        last_error_ = "invalid_device_mac";
        return false;
    }

    wizchip_spi_initialize();
    wizchip_cris_initialize();
    wizchip_reset();
    wizchip_initialize();
    wizchip_check();

    if (!configure_network(runtime_config)) {
        return false;
    }

    initialized_ = true;
    last_error_ = "ok";
    return true;
}

void EthernetManager::service() {
    if (!initialized_ || !dhcp_enabled_) {
        return;
    }

    const unsigned int now_ms = to_ms_since_boot(get_absolute_time());
    if (now_ms - last_dhcp_run_ms_ < kDhcpPollIntervalMs) {
        return;
    }

    last_dhcp_run_ms_ = now_ms;
    const uint8_t dhcp_result = DHCP_run();
    if (dhcp_result == DHCP_FAILED) {
        std::puts("Ethernet: DHCP run reported failure");
    }
}

bool EthernetManager::status() const { return initialized_; }

const char* EthernetManager::last_error() const { return last_error_; }

bool EthernetManager::configure_network(const config::RuntimeConfig& runtime_config) {
    if (std::strcmp(runtime_config.ethernet.mode, "dhcp") == 0) {
        std::puts("Ethernet: DHCP mode requested");
        dhcp_enabled_ = true;
        g_network_info.dhcp = NETINFO_DHCP;
        std::memset(g_network_info.ip, 0, sizeof(g_network_info.ip));
        std::memset(g_network_info.sn, 0, sizeof(g_network_info.sn));
        std::memset(g_network_info.gw, 0, sizeof(g_network_info.gw));
        std::memset(g_network_info.dns, 0, sizeof(g_network_info.dns));

        DHCP_init(kDhcpSocket, g_ethernet_buffer);
        reg_dhcp_cbfunc(dhcp_assign_callback, dhcp_assign_callback, dhcp_conflict_callback);
        if (!add_repeating_timer_ms(1000, dhcp_timer_callback, nullptr, &g_dhcp_timer)) {
            last_error_ = "dhcp_timer_start_failed";
            return false;
        }

        return run_dhcp_until_leased();
    }

    if (std::strcmp(runtime_config.ethernet.mode, "static") == 0) {
        std::puts("Ethernet: static IP mode requested");
        return configure_static_network(runtime_config);
    }

    last_error_ = "unsupported_ethernet_mode";
    return false;
}

bool EthernetManager::configure_static_network(const config::RuntimeConfig& runtime_config) {
    g_network_info.dhcp = NETINFO_STATIC;

    if (!parse_ipv4(runtime_config.ethernet.static_ip, g_network_info.ip)) {
        last_error_ = "invalid_static_ip";
        return false;
    }

    if (!parse_ipv4(runtime_config.ethernet.static_subnet, g_network_info.sn)) {
        last_error_ = "invalid_static_subnet";
        return false;
    }

    if (!parse_ipv4(runtime_config.ethernet.static_gateway, g_network_info.gw)) {
        last_error_ = "invalid_static_gateway";
        return false;
    }

    if (!parse_ipv4(runtime_config.ethernet.static_dns, g_network_info.dns)) {
        last_error_ = "invalid_static_dns";
        return false;
    }

    network_initialize(g_network_info);
    print_network_information(g_network_info);
    return true;
}

bool EthernetManager::run_dhcp_until_leased() {
    std::puts("Ethernet: waiting for DHCP lease");

    const absolute_time_t deadline = make_timeout_time_ms(kDhcpTimeoutMs);
    while (absolute_time_diff_us(get_absolute_time(), deadline) > 0) {
        const uint8_t dhcp_result = DHCP_run();
        if (dhcp_leased_ || dhcp_result == DHCP_IP_LEASED || dhcp_result == DHCP_IP_ASSIGN ||
            dhcp_result == DHCP_IP_CHANGED) {
            return true;
        }

        if (dhcp_conflict_) {
            last_error_ = "dhcp_ip_conflict";
            return false;
        }

        if (dhcp_result == DHCP_FAILED) {
            break;
        }

        sleep_ms(kDhcpPollIntervalMs);
    }

    last_error_ = "dhcp_timeout";
    return false;
}

void EthernetManager::handle_dhcp_assigned() {
    getIPfromDHCP(g_network_info.ip);
    getGWfromDHCP(g_network_info.gw);
    getSNfromDHCP(g_network_info.sn);
    getDNSfromDHCP(g_network_info.dns);

    g_network_info.dhcp = NETINFO_DHCP;
    network_initialize(g_network_info);
    print_network_information(g_network_info);
    std::printf("Ethernet: DHCP lease time %lu seconds\n",
                static_cast<unsigned long>(getDHCPLeasetime()));
    dhcp_leased_ = true;
}

void EthernetManager::handle_dhcp_conflict() {
    dhcp_conflict_ = true;
    std::puts("Ethernet: DHCP reported an IP conflict");
}

}  // namespace network
