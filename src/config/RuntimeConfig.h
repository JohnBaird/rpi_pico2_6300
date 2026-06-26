#pragma once

#include <cstdint>

namespace config {

struct DeviceConfig {
    const char* name;
    const char* mac;
};

struct EthernetConfig {
    const char* mode;
    const char* static_ip;
    const char* static_subnet;
    const char* static_gateway;
    const char* static_dns;
};

struct MqttConfig {
    const char* broker;
    uint32_t port;
    const char* client_id_prefix;
    const char* username;
    const char* password;
    uint32_t keep_alive_sec;
    bool discovery_enabled;
    const char* broadcast_destination_id;
    const char* subscribe_topic;
};

struct LedRuntimeConfig {
    uint32_t healthy_on_ms;
    uint32_t healthy_off_ms;
};

struct RuntimeConfig {
    DeviceConfig device;
    EthernetConfig ethernet;
    MqttConfig mqtt;
    LedRuntimeConfig led;
    const char* config_source;
    uint32_t config_crc32;
};

}  // namespace config
