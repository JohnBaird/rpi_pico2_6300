#include "mqtt/MqttManager.h"

#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"

extern "C" {
#include "MQTTClient.h"
#include "mqtt_interface.h"
}

namespace mqtt {
namespace {

constexpr int kMqttSocket = 1;
constexpr unsigned int kCommandTimeoutMs = 4000;
constexpr unsigned int kYieldTimeoutMs = 25;
constexpr unsigned int kSendBufferSize = 1024;
constexpr unsigned int kReadBufferSize = 1024;
constexpr const char* kControllerSerialPlaceholder = "<controller_serial>";

MqttManager* g_active_manager = nullptr;
Network g_network;
MQTTClient g_client;
unsigned char g_send_buffer[kSendBufferSize] = {0};
unsigned char g_read_buffer[kReadBufferSize] = {0};

void mqtt_message_arrived(MessageData* message_data) {
    if (g_active_manager == nullptr || message_data == nullptr || message_data->message == nullptr ||
        message_data->topicName == nullptr) {
        return;
    }

    char topic_buffer[128] = {0};
    const unsigned int topic_length =
        static_cast<unsigned int>(message_data->topicName->lenstring.len);
    const unsigned int bytes_to_copy =
        topic_length < sizeof(topic_buffer) - 1 ? topic_length : sizeof(topic_buffer) - 1;
    std::memcpy(topic_buffer, message_data->topicName->lenstring.data, bytes_to_copy);
    topic_buffer[bytes_to_copy] = '\0';

    const MQTTMessage* message = message_data->message;
    g_active_manager->handle_message(topic_buffer, static_cast<const char*>(message->payload),
                                     static_cast<unsigned int>(message->payloadlen));
}

}  // namespace

MqttManager::MqttManager()
    : initialized_(false),
      last_error_("not_initialized"),
      last_timer_tick_ms_(0),
      controller_serial_{},
      client_id_{},
      subscribe_topic_{},
      discovery_response_topic_{},
      status_topic_{},
      status_payload_{},
      payload_buffer_{} {}

bool MqttManager::init(const config::RuntimeConfig& runtime_config) {
    std::puts("MQTT: initializing client");

    g_active_manager = this;
    initialized_ = false;
    last_timer_tick_ms_ = to_ms_since_boot(get_absolute_time());

    if (!build_controller_serial_from_mac(runtime_config.device.mac)) {
        last_error_ = "invalid_device_mac_for_serial";
        return false;
    }

    if (std::snprintf(client_id_, sizeof(client_id_), "%s-%s", runtime_config.mqtt.client_id_prefix,
                      controller_serial_) < 0) {
        last_error_ = "client_id_format_failed";
        return false;
    }

    if (std::snprintf(discovery_response_topic_, sizeof(discovery_response_topic_),
                      "SPV1.0/system/stc_online_status_response/+/%s", controller_serial_) < 0) {
        last_error_ = "discovery_topic_format_failed";
        return false;
    }

    if (std::snprintf(status_topic_, sizeof(status_topic_),
                      "SPV1.0/system/stc_online_status_request/%s/%s", controller_serial_,
                      runtime_config.mqtt.broadcast_destination_id) < 0) {
        last_error_ = "status_topic_format_failed";
        return false;
    }

    if (!build_subscribe_topic(runtime_config.mqtt.subscribe_topic)) {
        last_error_ = "subscribe_topic_format_failed";
        return false;
    }

    if (!connect_client(runtime_config)) {
        return false;
    }

    if (!subscribe_topics(runtime_config)) {
        return false;
    }

    if (!publish_startup_status(runtime_config)) {
        return false;
    }

    initialized_ = true;
    last_error_ = "ok";
    std::puts("MQTT: initialization complete");
    return true;
}

void MqttManager::service() {
    if (!initialized_) {
        return;
    }

    const unsigned int now_ms = to_ms_since_boot(get_absolute_time());
    while (last_timer_tick_ms_ < now_ms) {
        MilliTimer_Handler();
        ++last_timer_tick_ms_;
    }

    const int mqtt_result = MQTTYield(&g_client, kYieldTimeoutMs);
    if (mqtt_result != SUCCESSS) {
        std::printf("MQTT: yield returned %d\n", mqtt_result);
    }
}

bool MqttManager::status() const { return initialized_; }

const char* MqttManager::last_error() const { return last_error_; }

void MqttManager::handle_message(const char* topic, const char* payload, unsigned int payload_length) {
    std::printf("MQTT: received topic=%s payload_len=%u\n", topic, payload_length);

    const unsigned int bytes_to_copy =
        payload_length < sizeof(payload_buffer_) - 1 ? payload_length : sizeof(payload_buffer_) - 1;
    if (payload != nullptr && bytes_to_copy > 0) {
        std::memcpy(payload_buffer_, payload, bytes_to_copy);
    }
    payload_buffer_[bytes_to_copy] = '\0';

    if (bytes_to_copy > 0) {
        std::printf("MQTT: payload=%s\n", payload_buffer_);
    }

    if (!parse_spv1_topic(topic)) {
        std::puts("MQTT: topic parser rejected message");
    }
}

bool MqttManager::connect_client(const config::RuntimeConfig& runtime_config) {
    unsigned char broker_ip[4] = {0, 0, 0, 0};
    if (!parse_ipv4(runtime_config.mqtt.broker, broker_ip)) {
        last_error_ = "mqtt_broker_must_be_ipv4";
        return false;
    }

    NewNetwork(&g_network, kMqttSocket);
    if (ConnectNetwork(&g_network, broker_ip, static_cast<uint16_t>(runtime_config.mqtt.port)) != 1) {
        last_error_ = "mqtt_tcp_connect_failed";
        return false;
    }

    MQTTClientInit(&g_client, &g_network, kCommandTimeoutMs, g_send_buffer, sizeof(g_send_buffer),
                   g_read_buffer, sizeof(g_read_buffer));

    MQTTPacket_connectData connect_data = MQTTPacket_connectData_initializer;
    connect_data.MQTTVersion = 4;
    connect_data.clientID.cstring = client_id_;
    connect_data.keepAliveInterval =
        static_cast<unsigned short>(runtime_config.mqtt.keep_alive_sec);
    connect_data.cleansession = 1;
    connect_data.username.cstring =
        runtime_config.mqtt.username[0] != '\0' ? const_cast<char*>(runtime_config.mqtt.username)
                                                : nullptr;
    connect_data.password.cstring =
        runtime_config.mqtt.password[0] != '\0' ? const_cast<char*>(runtime_config.mqtt.password)
                                                : nullptr;

    const int mqtt_result = MQTTConnect(&g_client, &connect_data);
    if (mqtt_result != SUCCESSS) {
        std::printf("MQTT: connect failed rc=%d\n", mqtt_result);
        last_error_ = "mqtt_connect_failed";
        return false;
    }

    std::printf("MQTT: connected client_id=%s broker=%s:%lu\n", client_id_,
                runtime_config.mqtt.broker, static_cast<unsigned long>(runtime_config.mqtt.port));
    return true;
}

bool MqttManager::subscribe_topics(const config::RuntimeConfig& runtime_config) {
    std::printf("MQTT: subscribe template=%s\n", runtime_config.mqtt.subscribe_topic);

    int mqtt_result = MQTTSubscribe(&g_client, subscribe_topic_, QOS0, mqtt_message_arrived);
    if (mqtt_result != SUCCESSS) {
        std::printf("MQTT: subscribe failed rc=%d topic=%s\n", mqtt_result, subscribe_topic_);
        last_error_ = "mqtt_subscribe_failed";
        return false;
    }

    std::printf("MQTT: subscribed topic=%s\n", subscribe_topic_);

    if (runtime_config.mqtt.discovery_enabled) {
        mqtt_result = MQTTSubscribe(&g_client, discovery_response_topic_, QOS0, mqtt_message_arrived);
        if (mqtt_result != SUCCESSS) {
            std::printf("MQTT: discovery subscribe failed rc=%d topic=%s\n", mqtt_result,
                        discovery_response_topic_);
            last_error_ = "mqtt_discovery_subscribe_failed";
            return false;
        }

        std::printf("MQTT: subscribed topic=%s\n", discovery_response_topic_);
    }

    return true;
}

bool MqttManager::publish_startup_status(const config::RuntimeConfig& runtime_config) {
    const int payload_length = std::snprintf(
        status_payload_, sizeof(status_payload_),
        "{\"controller_serial\":\"%s\",\"controller_name\":\"%s\",\"status\":\"online\","
        "\"milestone\":4,\"subscribe_topic\":\"%s\",\"port_in_payload\":true}",
        controller_serial_, runtime_config.device.name, subscribe_topic_);
    if (payload_length <= 0 || payload_length >= static_cast<int>(sizeof(status_payload_))) {
        last_error_ = "mqtt_status_payload_too_large";
        return false;
    }

    MQTTMessage message{};
    message.qos = QOS0;
    message.retained = 0;
    message.dup = 0;
    message.payload = status_payload_;
    message.payloadlen = static_cast<size_t>(payload_length);

    const int mqtt_result = MQTTPublish(&g_client, status_topic_, &message);
    if (mqtt_result != SUCCESSS) {
        std::printf("MQTT: publish failed rc=%d topic=%s\n", mqtt_result, status_topic_);
        last_error_ = "mqtt_status_publish_failed";
        return false;
    }

    std::printf("MQTT: published startup status topic=%s\n", status_topic_);
    return true;
}

bool MqttManager::build_controller_serial_from_mac(const char* mac_text) {
    unsigned int octets[6] = {0, 0, 0, 0, 0, 0};
    if (std::sscanf(mac_text, "%x:%x:%x:%x:%x:%x", &octets[0], &octets[1], &octets[2], &octets[3],
                    &octets[4], &octets[5]) != 6) {
        return false;
    }

    unsigned long long mac_value = 0;
    for (int i = 0; i < 6; ++i) {
        if (octets[i] > 255U) {
            return false;
        }
        mac_value = (mac_value << 8) | octets[i];
    }

    return std::snprintf(controller_serial_, sizeof(controller_serial_), "%llu", mac_value) > 0;
}

bool MqttManager::build_subscribe_topic(const char* configured_topic) {
    if (configured_topic == nullptr || configured_topic[0] == '\0') {
        return false;
    }

    const char* placeholder = std::strstr(configured_topic, kControllerSerialPlaceholder);
    if (placeholder == nullptr) {
        return std::snprintf(subscribe_topic_, sizeof(subscribe_topic_), "%s", configured_topic) >
               0;
    }

    const size_t prefix_length = static_cast<size_t>(placeholder - configured_topic);
    const size_t placeholder_length = std::strlen(kControllerSerialPlaceholder);
    const char* suffix = placeholder + placeholder_length;

    if (std::strstr(suffix, kControllerSerialPlaceholder) != nullptr) {
        return false;
    }

    const int written = std::snprintf(subscribe_topic_, sizeof(subscribe_topic_), "%.*s%s%s",
                                      static_cast<int>(prefix_length), configured_topic,
                                      controller_serial_, suffix);
    return written > 0 && written < static_cast<int>(sizeof(subscribe_topic_));
}

bool MqttManager::parse_ipv4(const char* text, unsigned char out[4]) const {
    unsigned int octets[4] = {0, 0, 0, 0};
    if (std::sscanf(text, "%u.%u.%u.%u", &octets[0], &octets[1], &octets[2], &octets[3]) != 4) {
        return false;
    }

    for (int i = 0; i < 4; ++i) {
        if (octets[i] > 255U) {
            return false;
        }
        out[i] = static_cast<unsigned char>(octets[i]);
    }

    return true;
}

bool MqttManager::parse_spv1_topic(const char* topic) const {
    char working_copy[128] = {0};
    if (std::strlen(topic) >= sizeof(working_copy)) {
        return false;
    }

    std::strcpy(working_copy, topic);
    char* segments[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    unsigned int segment_count = 0;

    char* token = std::strtok(working_copy, "/");
    while (token != nullptr && segment_count < 5) {
        segments[segment_count++] = token;
        token = std::strtok(nullptr, "/");
    }

    if (segment_count != 5 || token != nullptr) {
        return false;
    }

    std::printf(
        "MQTT: parsed topic sp_version=%s domain=%s command=%s source_id=%s destination_id=%s\n",
        segments[0], segments[1], segments[2], segments[3], segments[4]);

    if (std::strcmp(segments[0], "SPV1.0") != 0) {
        return false;
    }

    if (std::strcmp(segments[1], "access") == 0 &&
        std::strcmp(segments[2], "stc_access_request") == 0) {
        std::puts("MQTT: parsed access request topic; port is expected in payload");
    }

    return true;
}

}  // namespace mqtt
