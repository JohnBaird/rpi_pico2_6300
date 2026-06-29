#pragma once

#include "config/RuntimeConfig.h"
#include "devices/Mcp9808TemperatureSensor.h"
#include "devices/Rp2350TemperatureSensor.h"
#include "storage/FlashConfigStore.h"

namespace mqtt {

struct RtcDateTime {
    bool valid;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint32_t unix_time;
};

class MqttManager {
  public:
    MqttManager();

    bool init(const config::RuntimeConfig& runtime_config,
              storage::FlashConfigStore& flash_config_store,
              devices::Mcp9808TemperatureSensor& temperature_sensor,
              devices::Rp2350TemperatureSensor& cpu_temperature_sensor);
    void service();
    bool status() const;
    const char* last_error() const;

    void handle_message(const char* topic, const char* payload, unsigned int payload_length);

  private:
    struct GenericRequestFields {
        char request_id[48];
    };

    struct AccessRequestFields {
        char request_id[48];
        char badge_id[48];
        char access_port[8];
        char first_name[64];
        char last_name[64];
        unsigned long long request_timestamp;
    };

    bool connect_client(const config::RuntimeConfig& runtime_config);
    bool subscribe_topics(const config::RuntimeConfig& runtime_config);
    bool publish_startup_status(const config::RuntimeConfig& runtime_config);
    bool publish_online_status_response(const config::RuntimeConfig& runtime_config,
                                        const char* destination_id, const char* reason);
    bool publish_access_response(const char* request_source_id,
                                 const AccessRequestFields& request_fields);
    bool publish_input_status_response(const char* request_source_id,
                                       const GenericRequestFields& request_fields);
    bool publish_config_file_response(const char* request_source_id,
                                      const GenericRequestFields& request_fields);
    bool publish_temperature_response(const char* request_source_id,
                                      const GenericRequestFields& request_fields);
    bool publish_single_temperature_response(const char* request_source_id,
                                             const GenericRequestFields& request_fields,
                                             const char* sensor_name, float temperature_c);
    bool publish_payload_to_topic(const char* topic, int payload_length, const char* log_label);
    bool build_controller_serial_from_mac(const char* mac_text);
    bool build_subscribe_topic(const char* configured_topic, char* destination,
                               unsigned int destination_size);
    bool read_active_ip_address(char* destination, unsigned int destination_size,
                                uint8_t* out_last_octet) const;
    bool read_rtc_datetime(RtcDateTime* out_datetime) const;
    bool build_status_payload(const config::RuntimeConfig& runtime_config, RtcDateTime rtc_datetime,
                              const char* ip_address, const char* host_name, const char* reason);
    void build_host_name(uint8_t ip_last_octet, char* destination,
                         unsigned int destination_size) const;
    void build_status_identifier(uint32_t unix_time, char* destination,
                                 unsigned int destination_size) const;
    void build_full_name(const AccessRequestFields& request_fields, char* destination,
                         unsigned int destination_size) const;
    bool parse_ipv4(const char* text, unsigned char out[4]) const;
    bool extract_json_string_value(const char* json_text, const char* key, char* destination,
                                   unsigned int destination_size) const;
    bool extract_json_uint64_value(const char* json_text, const char* key,
                                   unsigned long long* out_value) const;
    bool parse_generic_request_payload(const char* payload, GenericRequestFields* out_fields) const;
    bool parse_access_request_payload(const char* payload, AccessRequestFields* out_fields) const;
    bool handle_spv1_topic(const char* topic);
    bool dispatch_spv1_command(const char* domain, const char* command, const char* source_id,
                               const char* destination_id);

    bool initialized_;
    const config::RuntimeConfig* runtime_config_;
    storage::FlashConfigStore* flash_config_store_;
    devices::Mcp9808TemperatureSensor* temperature_sensor_;
    devices::Rp2350TemperatureSensor* cpu_temperature_sensor_;
    const char* last_error_;
    unsigned int last_timer_tick_ms_;
    char controller_serial_[24];
    char client_id_[64];
    char subscribe_topics_[config::kMaxSubscribeTopics][128];
    char status_payload_[2048];
    char payload_buffer_[1024];
    char config_file_buffer_[1536];
};

}  // namespace mqtt
