#pragma once

#include <cstdint>

namespace config {

constexpr unsigned int kMaxPublishServers = 8;
constexpr unsigned int kMaxSubscribeTopics = 8;
constexpr unsigned int kMaxWiegandInterfaces = 8;

struct DeviceConfig {
    const char* name;
    const char* mac;
    const char* git_number;
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
    const char* subscribe_topics[kMaxSubscribeTopics];
    uint32_t subscribe_topic_count;
    const char* publish_to_server_ids[kMaxPublishServers];
    uint32_t publish_to_server_count;
};

struct LedRuntimeConfig {
    uint32_t healthy_on_ms;
    uint32_t healthy_off_ms;
};

struct WiegandRuntimeConfig {
    bool output_enabled;
    const char* output_formats[kMaxWiegandInterfaces];
    uint32_t output_facility_codes[kMaxWiegandInterfaces];
    uint32_t test_card_numbers[kMaxWiegandInterfaces];
};

struct RuntimeConfig {
    DeviceConfig device;
    EthernetConfig ethernet;
    MqttConfig mqtt;
    LedRuntimeConfig led;
    WiegandRuntimeConfig wiegand;
    const char* config_source;
    uint32_t config_crc32;
};

}  // namespace config
