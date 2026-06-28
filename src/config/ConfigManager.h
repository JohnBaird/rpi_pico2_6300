#pragma once

#include "config/RuntimeConfig.h"
#include "storage/FlashConfigStore.h"

namespace config {

class ConfigManager {
  public:
    explicit ConfigManager(storage::FlashConfigStore& flash_config_store);

    bool init();
    bool status() const;
    const char* last_error() const;
    const RuntimeConfig* runtime_config() const;
    void print_summary() const;
    bool override_device_mac(const char* mac_text);

  private:
    bool load_active_config_from_littlefs();
    bool load_factory_default();
    bool seed_littlefs_from_factory(const char* reason);
    bool load_and_apply_config_text(const char* json_text, const char* source_name,
                                    bool verify_crc);
    bool prepare_factory_config_text(char* destination, unsigned int destination_size,
                                     unsigned int* out_length);
    bool verify_crc32(const char* json_text, uint32_t* out_crc32);
    bool populate_crc32_field(char* json_text);
    bool extract_crc32_value(char* json_text, uint32_t* out_stored_crc32, char** out_crc_value_start);
    uint32_t compute_crc32(const char* data, unsigned int length) const;
    void record_littlefs_event(const char* message);
    bool write_config_meta(const char* active_source, const char* last_event);
    bool parse_runtime_config(const char* json_text);
    bool validate_runtime_config() const;
    const char* find_section(const char* json_text, const char* section_name) const;
    bool extract_string_value(const char* section_start, const char* key, char* destination,
                              unsigned int destination_size);
    bool extract_uint_value(const char* section_start, const char* key, uint32_t* out_value);
    bool extract_bool_value(const char* section_start, const char* key, bool* out_value);

    storage::FlashConfigStore& flash_config_store_;
    RuntimeConfig runtime_config_;
    bool initialized_;
    char device_name_[64];
    char device_mac_[32];
    char ethernet_mode_[16];
    char ethernet_static_ip_[32];
    char ethernet_static_subnet_[32];
    char ethernet_static_gateway_[32];
    char ethernet_static_dns_[32];
    char mqtt_broker_[64];
    char mqtt_client_id_prefix_[32];
    char mqtt_username_[64];
    char mqtt_password_[64];
    char mqtt_broadcast_destination_id_[32];
    char mqtt_subscribe_topic_[128];
    char config_source_[16];
    uint32_t config_crc32_;
    char config_buffer_[4096];
    char scratch_buffer_[4096];
    const char* last_error_;
};

}  // namespace config
