#include "mqtt/MqttManager.h"

#include <cstdio>
#include <cstring>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

extern "C" {
#include "MQTTClient.h"
#include "mqtt_interface.h"
#include "wizchip_conf.h"
}

namespace mqtt {
namespace {

constexpr int kMqttSocket = 1;
constexpr unsigned int kCommandTimeoutMs = 4000;
constexpr unsigned int kYieldTimeoutMs = 25;
constexpr unsigned int kSendBufferSize = 2048;
constexpr unsigned int kReadBufferSize = 2048;
constexpr const char* kControllerSerialPlaceholder = "<controller_serial>";
constexpr i2c_inst_t* kRtcI2cInstance = i2c0;
constexpr uint8_t kRtcAddress = 0x68;

bool parse_access_port_index(const char* text, uint8_t* out_port_index) {
    if (text == nullptr || out_port_index == nullptr) {
        return false;
    }

    unsigned int parsed_value = 0;
    if (std::sscanf(text, "%u", &parsed_value) != 1 || parsed_value > 7U) {
        return false;
    }

    *out_port_index = static_cast<uint8_t>(parsed_value);
    return true;
}

uint8_t bcd_to_decimal(uint8_t value) {
    return static_cast<uint8_t>(((value >> 4U) * 10U) + (value & 0x0FU));
}

bool is_leap_year(uint16_t year) {
    if ((year % 400U) == 0U) {
        return true;
    }
    if ((year % 100U) == 0U) {
        return false;
    }
    return (year % 4U) == 0U;
}

uint32_t unix_time_from_datetime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour,
                                 uint8_t minute, uint8_t second) {
    static constexpr uint8_t kDaysPerMonth[12] = {31, 28, 31, 30, 31, 30,
                                                  31, 31, 30, 31, 30, 31};

    uint32_t days = 0;
    for (uint16_t current_year = 1970; current_year < year; ++current_year) {
        days += is_leap_year(current_year) ? 366U : 365U;
    }

    for (uint8_t current_month = 1; current_month < month; ++current_month) {
        days += kDaysPerMonth[current_month - 1];
        if (current_month == 2 && is_leap_year(year)) {
            ++days;
        }
    }

    days += static_cast<uint32_t>(day - 1U);
    return (((days * 24U) + hour) * 60U + minute) * 60U + second;
}

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
      runtime_config_(nullptr),
      flash_config_store_(nullptr),
      temperature_sensor_(nullptr),
      cpu_temperature_sensor_(nullptr),
      last_error_("not_initialized"),
      last_timer_tick_ms_(0),
      controller_serial_{},
      client_id_{},
      subscribe_topics_{},
      status_payload_{},
      payload_buffer_{},
      config_file_buffer_{} {}

bool MqttManager::init(const config::RuntimeConfig& runtime_config,
                       storage::FlashConfigStore& flash_config_store,
                       devices::Mcp9808TemperatureSensor& temperature_sensor,
                       devices::Rp2350TemperatureSensor& cpu_temperature_sensor) {
    std::puts("MQTT: initializing client");

    g_active_manager = this;
    initialized_ = false;
    runtime_config_ = &runtime_config;
    flash_config_store_ = &flash_config_store;
    temperature_sensor_ = &temperature_sensor;
    cpu_temperature_sensor_ = &cpu_temperature_sensor;
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

    for (uint32_t i = 0; i < runtime_config.mqtt.subscribe_topic_count; ++i) {
        if (!build_subscribe_topic(runtime_config.mqtt.subscribe_topics[i], subscribe_topics_[i],
                                   sizeof(subscribe_topics_[i]))) {
            last_error_ = "subscribe_topic_format_failed";
            return false;
        }
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

    if (!handle_spv1_topic(topic)) {
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
    for (uint32_t i = 0; i < runtime_config.mqtt.subscribe_topic_count; ++i) {
        std::printf("MQTT: subscribe[%lu] template=%s\n", static_cast<unsigned long>(i),
                    runtime_config.mqtt.subscribe_topics[i]);
        std::printf("MQTT: subscribe[%u] resolved=%s\n", i, subscribe_topics_[i]);

        const int mqtt_result = MQTTSubscribe(&g_client, subscribe_topics_[i], QOS0,
                                              mqtt_message_arrived);
        if (mqtt_result != SUCCESSS) {
            std::printf("MQTT: subscribe failed rc=%d topic=%s\n", mqtt_result,
                        subscribe_topics_[i]);
            last_error_ = "mqtt_subscribe_failed";
            return false;
        }

        std::printf("MQTT: subscribed topic=%s\n", subscribe_topics_[i]);
    }

    std::printf("MQTT: discovery_enabled=%s (no extra subscription)\n",
                runtime_config.mqtt.discovery_enabled ? "true" : "false");

    return true;
}

bool MqttManager::publish_startup_status(const config::RuntimeConfig& runtime_config) {
    if (runtime_config.mqtt.publish_to_server_count == 0) {
        last_error_ = "mqtt_publish_to_servers_empty";
        return false;
    }

    for (uint32_t i = 0; i < runtime_config.mqtt.publish_to_server_count; ++i) {
        if (!publish_online_status_response(runtime_config,
                                            runtime_config.mqtt.publish_to_server_ids[i],
                                            "restarted")) {
            return false;
        }
    }
    return true;
}

bool MqttManager::publish_online_status_response(const config::RuntimeConfig& runtime_config,
                                                 const char* destination_id,
                                                 const char* reason) {
    char ip_address[20] = {0};
    uint8_t ip_last_octet = 0;
    if (!read_active_ip_address(ip_address, sizeof(ip_address), &ip_last_octet)) {
        last_error_ = "mqtt_ip_address_unavailable";
        return false;
    }

    char host_name[32] = {0};
    build_host_name(ip_last_octet, host_name, sizeof(host_name));

    RtcDateTime rtc_datetime{};
    rtc_datetime.valid = false;
    rtc_datetime.year = 0;
    rtc_datetime.month = 0;
    rtc_datetime.day = 0;
    rtc_datetime.hour = 0;
    rtc_datetime.minute = 0;
    rtc_datetime.second = 0;
    rtc_datetime.unix_time = 0;
    read_rtc_datetime(&rtc_datetime);

    if (!build_status_payload(runtime_config, rtc_datetime, ip_address, host_name, reason)) {
        last_error_ = "mqtt_status_payload_too_large";
        return false;
    }

    const int payload_length = static_cast<int>(std::strlen(status_payload_));
    char status_topic[96] = {0};
    const int topic_length =
        std::snprintf(status_topic, sizeof(status_topic),
                      "SPV1.0/access/stc_online_status_response/%s/%s", controller_serial_,
                      destination_id);
    if (topic_length <= 0 || topic_length >= static_cast<int>(sizeof(status_topic))) {
        last_error_ = "status_topic_format_failed";
        return false;
    }

    return publish_payload_to_topic(status_topic, payload_length, "online status");
}

bool MqttManager::publish_access_response(const char* request_source_id,
                                          const AccessRequestFields& request_fields) {
    if (runtime_config_ == nullptr || request_source_id == nullptr) {
        last_error_ = "mqtt_access_response_context_invalid";
        return false;
    }

    if (runtime_config_->mqtt.publish_to_server_count == 0) {
        last_error_ = "mqtt_publish_to_servers_empty";
        return false;
    }

    uint8_t access_port_index = 0;
    const bool access_port_valid =
        parse_access_port_index(request_fields.access_port, &access_port_index);
    const unsigned int i2c_address = 0x30U + access_port_index;

    RtcDateTime rtc_datetime{};
    rtc_datetime.valid = false;
    rtc_datetime.year = 0;
    rtc_datetime.month = 0;
    rtc_datetime.day = 0;
    rtc_datetime.hour = 0;
    rtc_datetime.minute = 0;
    rtc_datetime.second = 0;
    rtc_datetime.unix_time = 0;
    read_rtc_datetime(&rtc_datetime);

    char date_time[24] = "0000/00/00 00:00:00";
    if (rtc_datetime.valid) {
        std::snprintf(date_time, sizeof(date_time), "%04u/%02u/%02u %02u:%02u:%02u",
                      rtc_datetime.year, rtc_datetime.month, rtc_datetime.day, rtc_datetime.hour,
                      rtc_datetime.minute, rtc_datetime.second);
    }

    char full_name[128] = {0};
    build_full_name(request_fields, full_name, sizeof(full_name));

    const int payload_length = std::snprintf(
        status_payload_, sizeof(status_payload_),
        "{"
        "\"_iD\":\"%s\","
        "\"dateTime\":\"%s\","
        "\"latency\":\"\","
        "\"fullName\":\"%s\","
        "\"transactionType\":\"roc_access_request\","
        "\"badgeId\":\"%s\","
        "\"serialSource\":\"%s\","
        "\"accessPort\":\"%s\","
        "\"timestamp\":%llu,"
        "\"response\":\"accepted\","
        "\"reason\":\"wiegand_queued\""
        "}",
        request_fields.request_id, date_time, full_name, request_fields.badge_id, request_source_id,
        request_fields.access_port, rtc_datetime.valid ? rtc_datetime.unix_time : 0ULL);
    if (payload_length <= 0 || payload_length >= static_cast<int>(sizeof(status_payload_))) {
        last_error_ = "mqtt_access_response_payload_too_large";
        return false;
    }

    for (uint32_t i = 0; i < runtime_config_->mqtt.publish_to_server_count; ++i) {
        const char* monitor_id = runtime_config_->mqtt.publish_to_server_ids[i];
        char response_topic[96] = {0};
        const int topic_length =
            std::snprintf(response_topic, sizeof(response_topic),
                          "SPV1.0/access/stc_access_response/%s/%s", controller_serial_,
                          monitor_id);
        if (topic_length <= 0 || topic_length >= static_cast<int>(sizeof(response_topic))) {
            last_error_ = "mqtt_access_response_topic_format_failed";
            return false;
        }

        std::printf("MQTT TX: topic=%s\n", response_topic);
        if (access_port_valid) {
            std::printf(
                "Access Response: response=accepted reason=wiegand_queued monitor=%s serialSource=%s \"accessPort\": \"%s\" i2c_address=0x%02X\n",
                monitor_id, request_source_id, request_fields.access_port, i2c_address);
        } else {
            std::printf(
                "Access Response: response=accepted reason=wiegand_queued monitor=%s serialSource=%s \"accessPort\": \"%s\" i2c_address=invalid\n",
                monitor_id, request_source_id, request_fields.access_port);
        }

        if (!publish_payload_to_topic(response_topic, payload_length, "access response")) {
            return false;
        }
    }

    return true;
}

bool MqttManager::publish_input_status_response(const char* request_source_id,
                                                const GenericRequestFields& request_fields) {
    if (runtime_config_ == nullptr || request_source_id == nullptr) {
        last_error_ = "mqtt_input_status_response_context_invalid";
        return false;
    }

    char ip_address[20] = {0};
    uint8_t ip_last_octet = 0;
    if (!read_active_ip_address(ip_address, sizeof(ip_address), &ip_last_octet)) {
        last_error_ = "mqtt_ip_address_unavailable";
        return false;
    }

    char host_name[32] = {0};
    build_host_name(ip_last_octet, host_name, sizeof(host_name));

    RtcDateTime rtc_datetime{};
    rtc_datetime.valid = false;
    rtc_datetime.year = 0;
    rtc_datetime.month = 0;
    rtc_datetime.day = 0;
    rtc_datetime.hour = 0;
    rtc_datetime.minute = 0;
    rtc_datetime.second = 0;
    rtc_datetime.unix_time = 0;
    read_rtc_datetime(&rtc_datetime);

    char date_time[24] = "0000/00/00 00:00:00";
    if (rtc_datetime.valid) {
        std::snprintf(date_time, sizeof(date_time), "%04u/%02u/%02u %02u:%02u:%02u",
                      rtc_datetime.year, rtc_datetime.month, rtc_datetime.day, rtc_datetime.hour,
                      rtc_datetime.minute, rtc_datetime.second);
    }

    const int payload_length = std::snprintf(
        status_payload_, sizeof(status_payload_),
        "{"
        "\"_iD\":\"%s\","
        "\"clientId\":\"%s\","
        "\"programVersion\":\"W6300 Access Gateway\","
        "\"hostName\":\"%s\","
        "\"ipAddress\":\"%s\","
        "\"dateTime\":\"%s\","
        "\"unixTime\":%lu,"
        "\"inputLevelsBitmap\":0,"
        "\"inputs\":{},"
        "\"response\":\"ok\","
        "\"reason\":\"requested\""
        "}",
        request_fields.request_id, controller_serial_, host_name, ip_address, date_time,
        static_cast<unsigned long>(rtc_datetime.unix_time));
    if (payload_length <= 0 || payload_length >= static_cast<int>(sizeof(status_payload_))) {
        last_error_ = "mqtt_input_status_response_payload_too_large";
        return false;
    }

    char response_topic[96] = {0};
    const int topic_length =
        std::snprintf(response_topic, sizeof(response_topic),
                      "SPV1.0/access/stc_input_status_response/%s/%s", controller_serial_,
                      request_source_id);
    if (topic_length <= 0 || topic_length >= static_cast<int>(sizeof(response_topic))) {
        last_error_ = "mqtt_input_status_response_topic_format_failed";
        return false;
    }

    std::printf("MQTT TX: topic=%s\n", response_topic);
    std::printf("Input Status Response: response=ok reason=requested inputLevelsBitmap=0 inputs={}\n");
    return publish_payload_to_topic(response_topic, payload_length, "input status response");
}

bool MqttManager::publish_config_file_response(const char* request_source_id,
                                               const GenericRequestFields& request_fields) {
    if (runtime_config_ == nullptr || flash_config_store_ == nullptr || request_source_id == nullptr) {
        last_error_ = "mqtt_config_file_response_context_invalid";
        return false;
    }

    if (!flash_config_store_->is_mounted() && !flash_config_store_->mount_read_only()) {
        const int payload_length = std::snprintf(
            status_payload_, sizeof(status_payload_),
            "{"
            "\"_iD\":\"%s\","
            "\"clientId\":\"%s\","
            "\"fileName\":\"config.json\","
            "\"fileSource\":\"littlefs\","
            "\"content\":\"\","
            "\"response\":\"error\","
            "\"reason\":\"%s\""
            "}",
            request_fields.request_id, controller_serial_, flash_config_store_->last_error());
        if (payload_length <= 0 || payload_length >= static_cast<int>(sizeof(status_payload_))) {
            last_error_ = "mqtt_config_file_error_payload_too_large";
            return false;
        }

        char response_topic[96] = {0};
        const int topic_length =
            std::snprintf(response_topic, sizeof(response_topic),
                          "SPV1.0/access/stc_config_file_response/%s/%s", controller_serial_,
                          request_source_id);
        if (topic_length <= 0 || topic_length >= static_cast<int>(sizeof(response_topic))) {
            last_error_ = "mqtt_config_file_response_topic_format_failed";
            return false;
        }

        std::printf("MQTT TX: topic=%s\n", response_topic);
        std::printf("Config File Response: response=error reason=%s fileName=config.json fileSource=littlefs\n",
                    flash_config_store_->last_error());
        return publish_payload_to_topic(response_topic, payload_length, "config file response");
    }

    unsigned int config_length = 0;
    if (!flash_config_store_->read_text_file("/config.json", config_file_buffer_,
                                             sizeof(config_file_buffer_), &config_length)) {
        const int payload_length = std::snprintf(
            status_payload_, sizeof(status_payload_),
            "{"
            "\"_iD\":\"%s\","
            "\"clientId\":\"%s\","
            "\"fileName\":\"config.json\","
            "\"fileSource\":\"littlefs\","
            "\"content\":\"\","
            "\"response\":\"error\","
            "\"reason\":\"config_not_found\""
            "}",
            request_fields.request_id, controller_serial_);
        if (payload_length <= 0 || payload_length >= static_cast<int>(sizeof(status_payload_))) {
            last_error_ = "mqtt_config_file_error_payload_too_large";
            return false;
        }

        char response_topic[96] = {0};
        const int topic_length =
            std::snprintf(response_topic, sizeof(response_topic),
                          "SPV1.0/access/stc_config_file_response/%s/%s", controller_serial_,
                          request_source_id);
        if (topic_length <= 0 || topic_length >= static_cast<int>(sizeof(response_topic))) {
            last_error_ = "mqtt_config_file_response_topic_format_failed";
            return false;
        }

        std::printf("MQTT TX: topic=%s\n", response_topic);
        std::printf("Config File Response: response=error reason=config_not_found fileName=config.json fileSource=littlefs\n");
        return publish_payload_to_topic(response_topic, payload_length, "config file response");
    }

    char ip_address[20] = {0};
    uint8_t ip_last_octet = 0;
    if (!read_active_ip_address(ip_address, sizeof(ip_address), &ip_last_octet)) {
        last_error_ = "mqtt_ip_address_unavailable";
        return false;
    }

    char host_name[32] = {0};
    build_host_name(ip_last_octet, host_name, sizeof(host_name));

    RtcDateTime rtc_datetime{};
    rtc_datetime.valid = false;
    rtc_datetime.year = 0;
    rtc_datetime.month = 0;
    rtc_datetime.day = 0;
    rtc_datetime.hour = 0;
    rtc_datetime.minute = 0;
    rtc_datetime.second = 0;
    rtc_datetime.unix_time = 0;
    read_rtc_datetime(&rtc_datetime);

    char date_time[24] = "0000/00/00 00:00:00";
    if (rtc_datetime.valid) {
        std::snprintf(date_time, sizeof(date_time), "%04u/%02u/%02u %02u:%02u:%02u",
                      rtc_datetime.year, rtc_datetime.month, rtc_datetime.day, rtc_datetime.hour,
                      rtc_datetime.minute, rtc_datetime.second);
    }

    const int payload_length = std::snprintf(
        status_payload_, sizeof(status_payload_),
        "{"
        "\"_iD\":\"%s\","
        "\"clientId\":\"%s\","
        "\"programVersion\":\"W6300 Access Gateway\","
        "\"hostName\":\"%s\","
        "\"ipAddress\":\"%s\","
        "\"dateTime\":\"%s\","
        "\"unixTime\":%lu,"
        "\"fileName\":\"config.json\","
        "\"fileSource\":\"littlefs\","
        "\"content\":%s,"
        "\"response\":\"ok\","
        "\"reason\":\"requested\""
        "}",
        request_fields.request_id, controller_serial_, host_name, ip_address, date_time,
        static_cast<unsigned long>(rtc_datetime.unix_time), config_file_buffer_);
    if (payload_length <= 0 || payload_length >= static_cast<int>(sizeof(status_payload_))) {
        last_error_ = "mqtt_config_file_response_payload_too_large";
        return false;
    }

    char response_topic[96] = {0};
    const int topic_length =
        std::snprintf(response_topic, sizeof(response_topic),
                      "SPV1.0/access/stc_config_file_response/%s/%s", controller_serial_,
                      request_source_id);
    if (topic_length <= 0 || topic_length >= static_cast<int>(sizeof(response_topic))) {
        last_error_ = "mqtt_config_file_response_topic_format_failed";
        return false;
    }

    std::printf("MQTT TX: topic=%s\n", response_topic);
    std::printf("Config File Response: response=ok reason=requested fileName=config.json fileSource=littlefs bytes=%u\n",
                config_length);
    return publish_payload_to_topic(response_topic, payload_length, "config file response");
}

bool MqttManager::publish_temperature_response(const char* request_source_id,
                                               const GenericRequestFields& request_fields) {
    if (runtime_config_ == nullptr || temperature_sensor_ == nullptr ||
        cpu_temperature_sensor_ == nullptr || request_source_id == nullptr) {
        last_error_ = "mqtt_temperature_response_context_invalid";
        return false;
    }

    bool published_any = false;

    float cpu_temperature_c = 0.0f;
    if (cpu_temperature_sensor_->read_temperature_c(&cpu_temperature_c)) {
        if (!publish_single_temperature_response(request_source_id, request_fields,
                                                 cpu_temperature_sensor_->sensor_name(),
                                                 cpu_temperature_c)) {
            return false;
        }
        published_any = true;
    } else {
        std::printf("Temperature Response: sensor=%s error=%s\n",
                    cpu_temperature_sensor_->sensor_name(), cpu_temperature_sensor_->last_error());
    }

    float sensor_temperature_c = 0.0f;
    if (temperature_sensor_->read_temperature_c(&sensor_temperature_c)) {
        if (!publish_single_temperature_response(request_source_id, request_fields,
                                                 temperature_sensor_->sensor_name(),
                                                 sensor_temperature_c)) {
            return false;
        }
        published_any = true;
    } else {
        std::printf("Temperature Response: sensor=%s error=%s\n",
                    temperature_sensor_->sensor_name(), temperature_sensor_->last_error());
    }

    if (!published_any) {
        last_error_ = "mqtt_temperature_read_failed";
        return false;
    }

    return true;
}

bool MqttManager::publish_single_temperature_response(const char* request_source_id,
                                                      const GenericRequestFields& request_fields,
                                                      const char* sensor_name,
                                                      float temperature_c) {
    if (request_source_id == nullptr || sensor_name == nullptr) {
        last_error_ = "mqtt_single_temperature_response_context_invalid";
        return false;
    }

    char ip_address[20] = {0};
    uint8_t ip_last_octet = 0;
    if (!read_active_ip_address(ip_address, sizeof(ip_address), &ip_last_octet)) {
        last_error_ = "mqtt_ip_address_unavailable";
        return false;
    }

    char host_name[32] = {0};
    build_host_name(ip_last_octet, host_name, sizeof(host_name));

    RtcDateTime rtc_datetime{};
    rtc_datetime.valid = false;
    rtc_datetime.year = 0;
    rtc_datetime.month = 0;
    rtc_datetime.day = 0;
    rtc_datetime.hour = 0;
    rtc_datetime.minute = 0;
    rtc_datetime.second = 0;
    rtc_datetime.unix_time = 0;
    read_rtc_datetime(&rtc_datetime);

    char date_time[24] = "0000/00/00 00:00:00";
    if (rtc_datetime.valid) {
        std::snprintf(date_time, sizeof(date_time), "%04u/%02u/%02u %02u:%02u:%02u",
                      rtc_datetime.year, rtc_datetime.month, rtc_datetime.day, rtc_datetime.hour,
                      rtc_datetime.minute, rtc_datetime.second);
    }

    const int payload_length = std::snprintf(
        status_payload_, sizeof(status_payload_),
        "{"
        "\"_iD\":\"%s\","
        "\"clientId\":\"%s\","
        "\"programVersion\":\"W6300 Access Gateway\","
        "\"hostName\":\"%s\","
        "\"ipAddress\":\"%s\","
        "\"dateTime\":\"%s\","
        "\"unixTime\":%lu,"
        "\"sensor_name\":\"%s\","
        "\"sensor_value\":%.2f,"
        "\"temperature_units\":\"C\""
        "}",
        request_fields.request_id, controller_serial_, host_name, ip_address, date_time,
        static_cast<unsigned long>(rtc_datetime.unix_time), sensor_name, temperature_c);
    if (payload_length <= 0 || payload_length >= static_cast<int>(sizeof(status_payload_))) {
        last_error_ = "mqtt_temperature_response_payload_too_large";
        return false;
    }

    char response_topic[96] = {0};
    const int topic_length =
        std::snprintf(response_topic, sizeof(response_topic),
                      "SPV1.0/access/stc_temperature_response/%s/%s", controller_serial_,
                      request_source_id);
    if (topic_length <= 0 || topic_length >= static_cast<int>(sizeof(response_topic))) {
        last_error_ = "mqtt_temperature_response_topic_format_failed";
        return false;
    }

    std::printf("MQTT TX: topic=%s\n", response_topic);
    std::printf("Temperature Response: sensor=%s value=%.2fC\n", sensor_name, temperature_c);
    return publish_payload_to_topic(response_topic, payload_length, "temperature response");
}

bool MqttManager::publish_payload_to_topic(const char* topic, int payload_length,
                                           const char* log_label) {
    MQTTMessage message{};
    message.qos = QOS0;
    message.retained = 0;
    message.dup = 0;
    message.payload = status_payload_;
    message.payloadlen = static_cast<size_t>(payload_length);

    const int mqtt_result = MQTTPublish(&g_client, topic, &message);
    if (mqtt_result != SUCCESSS) {
        std::printf("MQTT: publish failed rc=%d topic=%s\n", mqtt_result, topic);
        last_error_ = "mqtt_status_publish_failed";
        return false;
    }

    if (log_label != nullptr && log_label[0] != '\0') {
        std::printf("MQTT: published %s topic=%s\n", log_label, topic);
    } else {
        std::printf("MQTT: published topic=%s\n", topic);
    }
    return true;
}

bool MqttManager::read_active_ip_address(char* destination, unsigned int destination_size,
                                         uint8_t* out_last_octet) const {
    if (destination == nullptr || destination_size == 0) {
        return false;
    }

    wiz_NetInfo network_info{};
    wizchip_getnetinfo(&network_info);
    const int written =
        std::snprintf(destination, destination_size, "%u.%u.%u.%u", network_info.ip[0],
                      network_info.ip[1], network_info.ip[2], network_info.ip[3]);
    if (written <= 0 || written >= static_cast<int>(destination_size)) {
        return false;
    }

    if (out_last_octet != nullptr) {
        *out_last_octet = network_info.ip[3];
    }
    return true;
}

bool MqttManager::read_rtc_datetime(RtcDateTime* out_datetime) const {
    if (out_datetime == nullptr) {
        return false;
    }

    out_datetime->valid = false;
    uint8_t register_index = 0x00;
    if (i2c_write_timeout_us(kRtcI2cInstance, kRtcAddress, &register_index, 1, true, 4000) != 1) {
        return false;
    }

    uint8_t rtc_data[7] = {0};
    if (i2c_read_timeout_us(kRtcI2cInstance, kRtcAddress, rtc_data, sizeof(rtc_data), false, 4000) !=
        static_cast<int>(sizeof(rtc_data))) {
        return false;
    }

    out_datetime->second = bcd_to_decimal(static_cast<uint8_t>(rtc_data[0] & 0x7FU));
    out_datetime->minute = bcd_to_decimal(static_cast<uint8_t>(rtc_data[1] & 0x7FU));
    out_datetime->hour = bcd_to_decimal(static_cast<uint8_t>(rtc_data[2] & 0x3FU));
    out_datetime->day = bcd_to_decimal(static_cast<uint8_t>(rtc_data[4] & 0x3FU));
    out_datetime->month = bcd_to_decimal(static_cast<uint8_t>(rtc_data[5] & 0x1FU));
    out_datetime->year = static_cast<uint16_t>(2000U + bcd_to_decimal(rtc_data[6]));
    out_datetime->unix_time =
        unix_time_from_datetime(out_datetime->year, out_datetime->month, out_datetime->day,
                                out_datetime->hour, out_datetime->minute, out_datetime->second);
    out_datetime->valid = true;
    return true;
}

bool MqttManager::build_status_payload(const config::RuntimeConfig& runtime_config,
                                       RtcDateTime rtc_datetime, const char* ip_address,
                                       const char* host_name, const char* reason) {
    char status_id[25] = {0};
    build_status_identifier(rtc_datetime.unix_time, status_id, sizeof(status_id));

    char date_time[24] = "0000/00/00 00:00:00";
    if (rtc_datetime.valid) {
        std::snprintf(date_time, sizeof(date_time), "%04u/%02u/%02u %02u:%02u:%02u",
                      rtc_datetime.year, rtc_datetime.month, rtc_datetime.day, rtc_datetime.hour,
                      rtc_datetime.minute, rtc_datetime.second);
    }

    const int payload_length = std::snprintf(
        status_payload_, sizeof(status_payload_),
        "{"
        "\"_iD\":\"%s\","
        "\"clientId\":\"%s\","
        "\"programVersion\":\"W6300 Access Gateway\","
        "\"lastUpdated\":\"%s %s\","
        "\"git_number\":\"%s\","
        "\"controller_name\":\"%s\","
        "\"hostName\":\"%s\","
        "\"ipAddress\":\"%s\","
        "\"dateTime\":\"%s\","
        "\"unixTime\":%lu,"
        "\"response\":\"online\","
        "\"reason\":\"%s\""
        "}",
        status_id, client_id_, __DATE__, __TIME__, runtime_config.device.git_number,
        runtime_config.device.name, host_name, ip_address, date_time,
        static_cast<unsigned long>(rtc_datetime.unix_time), reason);
    return payload_length > 0 && payload_length < static_cast<int>(sizeof(status_payload_));
}

void MqttManager::build_host_name(uint8_t ip_last_octet, char* destination,
                                  unsigned int destination_size) const {
    if (destination == nullptr || destination_size == 0) {
        return;
    }

    std::snprintf(destination, destination_size, "w6300-%u", static_cast<unsigned int>(ip_last_octet));
}

void MqttManager::build_status_identifier(uint32_t unix_time, char* destination,
                                          unsigned int destination_size) const {
    if (destination == nullptr || destination_size < 25) {
        return;
    }

    unsigned long long serial_value = 0;
    std::sscanf(controller_serial_, "%llu", &serial_value);
    std::snprintf(destination, destination_size, "%08lX%08lX%08lX",
                  static_cast<unsigned long>(unix_time),
                  static_cast<unsigned long>(serial_value & 0xFFFFFFFFULL),
                  static_cast<unsigned long>((serial_value >> 16U) & 0xFFFFFFFFULL));
}

void MqttManager::build_full_name(const AccessRequestFields& request_fields, char* destination,
                                  unsigned int destination_size) const {
    if (destination == nullptr || destination_size == 0) {
        return;
    }

    if (request_fields.first_name[0] != '\0' && request_fields.last_name[0] != '\0') {
        std::snprintf(destination, destination_size, "%s %s", request_fields.first_name,
                      request_fields.last_name);
        return;
    }

    if (request_fields.first_name[0] != '\0') {
        std::snprintf(destination, destination_size, "%s", request_fields.first_name);
        return;
    }

    if (request_fields.last_name[0] != '\0') {
        std::snprintf(destination, destination_size, "%s", request_fields.last_name);
        return;
    }

    destination[0] = '\0';
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

bool MqttManager::build_subscribe_topic(const char* configured_topic, char* destination,
                                        unsigned int destination_size) {
    if (configured_topic == nullptr || configured_topic[0] == '\0' || destination == nullptr ||
        destination_size == 0) {
        return false;
    }

    const char* placeholder = std::strstr(configured_topic, kControllerSerialPlaceholder);
    if (placeholder == nullptr) {
        const int written = std::snprintf(destination, destination_size, "%s", configured_topic);
        return written > 0 && written < static_cast<int>(destination_size);
    }

    const size_t prefix_length = static_cast<size_t>(placeholder - configured_topic);
    const size_t placeholder_length = std::strlen(kControllerSerialPlaceholder);
    const char* suffix = placeholder + placeholder_length;

    if (std::strstr(suffix, kControllerSerialPlaceholder) != nullptr) {
        return false;
    }

    const int written = std::snprintf(destination, destination_size, "%.*s%s%s",
                                      static_cast<int>(prefix_length), configured_topic,
                                      controller_serial_, suffix);
    return written > 0 && written < static_cast<int>(destination_size);
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

bool MqttManager::extract_json_string_value(const char* json_text, const char* key,
                                            char* destination,
                                            unsigned int destination_size) const {
    if (json_text == nullptr || key == nullptr || destination == nullptr || destination_size == 0) {
        return false;
    }

    const char* key_start = std::strstr(json_text, key);
    if (key_start == nullptr) {
        return false;
    }

    const char* colon = std::strchr(key_start, ':');
    if (colon == nullptr) {
        return false;
    }

    const char* value_start = std::strchr(colon, '"');
    if (value_start == nullptr) {
        return false;
    }
    ++value_start;

    const char* value_end = std::strchr(value_start, '"');
    if (value_end == nullptr) {
        return false;
    }

    const unsigned int value_length = static_cast<unsigned int>(value_end - value_start);
    if (value_length + 1 > destination_size) {
        return false;
    }

    std::memcpy(destination, value_start, value_length);
    destination[value_length] = '\0';
    return true;
}

bool MqttManager::extract_json_uint64_value(const char* json_text, const char* key,
                                            unsigned long long* out_value) const {
    if (json_text == nullptr || key == nullptr || out_value == nullptr) {
        return false;
    }

    const char* key_start = std::strstr(json_text, key);
    if (key_start == nullptr) {
        return false;
    }

    const char* colon = std::strchr(key_start, ':');
    if (colon == nullptr) {
        return false;
    }

    unsigned long long parsed_value = 0;
    if (std::sscanf(colon + 1, " %llu", &parsed_value) != 1) {
        return false;
    }

    *out_value = parsed_value;
    return true;
}

bool MqttManager::parse_generic_request_payload(const char* payload,
                                                GenericRequestFields* out_fields) const {
    if (payload == nullptr || out_fields == nullptr) {
        return false;
    }

    out_fields->request_id[0] = '\0';
    return extract_json_string_value(payload, "\"_iD\"", out_fields->request_id,
                                     sizeof(out_fields->request_id));
}

bool MqttManager::parse_access_request_payload(const char* payload, AccessRequestFields* out_fields) const {
    if (payload == nullptr || out_fields == nullptr) {
        return false;
    }

    out_fields->request_id[0] = '\0';
    out_fields->badge_id[0] = '\0';
    out_fields->access_port[0] = '\0';
    out_fields->first_name[0] = '\0';
    out_fields->last_name[0] = '\0';
    out_fields->request_timestamp = 0;

    if (!extract_json_string_value(payload, "\"_iD\"", out_fields->request_id,
                                   sizeof(out_fields->request_id))) {
        return false;
    }
    if (!extract_json_string_value(payload, "\"badgeId\"", out_fields->badge_id,
                                   sizeof(out_fields->badge_id))) {
        return false;
    }
    if (!extract_json_string_value(payload, "\"accessPort\"", out_fields->access_port,
                                   sizeof(out_fields->access_port))) {
        return false;
    }
    if (!extract_json_string_value(payload, "\"firstname\"", out_fields->first_name,
                                   sizeof(out_fields->first_name))) {
        return false;
    }
    if (!extract_json_string_value(payload, "\"lastname\"", out_fields->last_name,
                                   sizeof(out_fields->last_name))) {
        return false;
    }
    if (!extract_json_uint64_value(payload, "\"timestamp\"", &out_fields->request_timestamp)) {
        return false;
    }

    return true;
}

bool MqttManager::handle_spv1_topic(const char* topic) {
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

    return dispatch_spv1_command(segments[1], segments[2], segments[3], segments[4]);
}

bool MqttManager::dispatch_spv1_command(const char* domain, const char* command,
                                        const char* source_id, const char* destination_id) {
    if (runtime_config_ == nullptr) {
        return false;
    }

    std::printf("MQTT RX: domain=%s command=%s source=%s destination=%s\n", domain, command,
                source_id, destination_id);

    if (std::strcmp(destination_id, controller_serial_) != 0) {
        std::puts("MQTT: message not addressed to this controller");
        return true;
    }

    if (std::strcmp(domain, "ident") == 0 && std::strcmp(command, "roc_access_request") == 0) {
        AccessRequestFields request_fields{};
        if (!parse_access_request_payload(payload_buffer_, &request_fields)) {
            std::puts("MQTT: ident roc_access_request payload parse failed");
            last_error_ = "mqtt_ident_access_request_parse_failed";
            return false;
        }

        char full_name[128] = {0};
        build_full_name(request_fields, full_name, sizeof(full_name));
        uint8_t access_port_index = 0;
        const bool access_port_valid =
            parse_access_port_index(request_fields.access_port, &access_port_index);
        if (access_port_valid) {
            std::printf(
                "Access Request: id=%s badgeId=%s \"accessPort\": \"%s\" i2c_address=0x%02X fullName=%s timestamp=%llu\n",
                request_fields.request_id, request_fields.badge_id, request_fields.access_port,
                static_cast<unsigned int>(0x30U + access_port_index), full_name,
                request_fields.request_timestamp);
        } else {
            std::printf(
                "Access Request: id=%s badgeId=%s \"accessPort\": \"%s\" i2c_address=invalid fullName=%s timestamp=%llu\n",
                request_fields.request_id, request_fields.badge_id, request_fields.access_port,
                full_name, request_fields.request_timestamp);
        }
        return publish_access_response(source_id, request_fields);
    }

    if (std::strcmp(domain, "access") == 0 && std::strcmp(command, "stc_access_request") == 0) {
        std::puts("MQTT: parsed access request topic; port is expected in payload");
        return true;
    }

    if (std::strcmp(domain, "access") == 0 &&
        std::strcmp(command, "stc_config_file_request") == 0) {
        GenericRequestFields request_fields{};
        if (!parse_generic_request_payload(payload_buffer_, &request_fields)) {
            std::puts("MQTT: access stc_config_file_request payload parse failed");
            last_error_ = "mqtt_config_file_request_parse_failed";
            return false;
        }

        std::printf("Config File Request: id=%s fileName=config.json fileSource=littlefs\n",
                    request_fields.request_id);
        return publish_config_file_response(source_id, request_fields);
    }

    if (std::strcmp(domain, "access") == 0 &&
        std::strcmp(command, "stc_input_status_request") == 0) {
        GenericRequestFields request_fields{};
        if (!parse_generic_request_payload(payload_buffer_, &request_fields)) {
            std::puts("MQTT: access stc_input_status_request payload parse failed");
            last_error_ = "mqtt_input_status_request_parse_failed";
            return false;
        }

        std::printf("Input Status Request: id=%s\n", request_fields.request_id);
        return publish_input_status_response(source_id, request_fields);
    }

    if (std::strcmp(domain, "access") == 0 &&
        std::strcmp(command, "stc_temperature_request") == 0) {
        GenericRequestFields request_fields{};
        if (!parse_generic_request_payload(payload_buffer_, &request_fields)) {
            std::puts("MQTT: access stc_temperature_request payload parse failed");
            last_error_ = "mqtt_temperature_request_parse_failed";
            return false;
        }

        std::printf("Temperature Request: id=%s sensor=%s\n", request_fields.request_id,
                    temperature_sensor_ != nullptr ? temperature_sensor_->sensor_name() : "unknown");
        return publish_temperature_response(source_id, request_fields);
    }

    if (std::strcmp(domain, "access") == 0 &&
        std::strcmp(command, "stc_online_status_request") == 0) {
        std::printf("MQTT: responding to online status request from %s\n", source_id);
        return publish_online_status_response(*runtime_config_, source_id, "requested");
    }

    return true;
}

}  // namespace mqtt
