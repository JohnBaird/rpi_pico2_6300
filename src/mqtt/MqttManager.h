#pragma once

#include "config/RuntimeConfig.h"

namespace mqtt {

class MqttManager {
  public:
    MqttManager();

    bool init(const config::RuntimeConfig& runtime_config);
    void service();
    bool status() const;
    const char* last_error() const;

    void handle_message(const char* topic, const char* payload, unsigned int payload_length);

  private:
    bool connect_client(const config::RuntimeConfig& runtime_config);
    bool subscribe_topics(const config::RuntimeConfig& runtime_config);
    bool publish_startup_status(const config::RuntimeConfig& runtime_config);
    bool build_controller_serial_from_mac(const char* mac_text);
    bool parse_ipv4(const char* text, unsigned char out[4]) const;
    bool parse_spv1_topic(const char* topic) const;

    bool initialized_;
    const char* last_error_;
    unsigned int last_timer_tick_ms_;
    char controller_serial_[24];
    char client_id_[64];
    char discovery_response_topic_[96];
    char status_topic_[96];
    char status_payload_[256];
    char payload_buffer_[512];
};

}  // namespace mqtt
