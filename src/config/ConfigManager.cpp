#include "config/ConfigManager.h"

#include <cstdio>
#include <cstring>

#include "GeneratedFactoryConfig.h"

namespace config {
namespace {

constexpr const char* kActiveConfigPath = "/config.json";
constexpr const char* kBackupConfigPath = "/config.bak.json";
constexpr const char* kConfigMetaPath = "/config.meta";
constexpr const char* kErrorLogPath = "/error.log";

bool copy_string(char* destination, unsigned int destination_size, const char* source,
                 unsigned int source_length) {
    if (destination == nullptr || destination_size == 0 || source == nullptr) {
        return false;
    }

    if (source_length + 1 > destination_size) {
        return false;
    }

    std::memcpy(destination, source, source_length);
    destination[source_length] = '\0';
    return true;
}

bool write_hex32(char* destination, uint32_t value) {
    char temporary[9] = {0};
    if (std::snprintf(temporary, sizeof(temporary), "%08lX",
                      static_cast<unsigned long>(value)) != 8) {
        return false;
    }

    std::memcpy(destination, temporary, 8);
    return true;
}

}  // namespace

ConfigManager::ConfigManager(storage::FlashConfigStore& flash_config_store)
    : flash_config_store_(flash_config_store),
      runtime_config_{},
      initialized_(false),
      device_name_{},
      device_mac_{},
      ethernet_mode_{},
      ethernet_static_ip_{},
      ethernet_static_subnet_{},
      ethernet_static_gateway_{},
      ethernet_static_dns_{},
      mqtt_broker_{},
      mqtt_client_id_prefix_{},
      mqtt_username_{},
      mqtt_password_{},
      mqtt_broadcast_destination_id_{},
      mqtt_subscribe_topic_{},
      config_source_{},
      config_crc32_(0),
      config_buffer_{},
      scratch_buffer_{},
      last_error_("not_initialized") {
    runtime_config_.device.name = device_name_;
    runtime_config_.device.mac = device_mac_;
    runtime_config_.ethernet.mode = ethernet_mode_;
    runtime_config_.ethernet.static_ip = ethernet_static_ip_;
    runtime_config_.ethernet.static_subnet = ethernet_static_subnet_;
    runtime_config_.ethernet.static_gateway = ethernet_static_gateway_;
    runtime_config_.ethernet.static_dns = ethernet_static_dns_;
    runtime_config_.mqtt.broker = mqtt_broker_;
    runtime_config_.mqtt.client_id_prefix = mqtt_client_id_prefix_;
    runtime_config_.mqtt.username = mqtt_username_;
    runtime_config_.mqtt.password = mqtt_password_;
    runtime_config_.mqtt.broadcast_destination_id = mqtt_broadcast_destination_id_;
    runtime_config_.mqtt.subscribe_topic = mqtt_subscribe_topic_;
    runtime_config_.config_source = config_source_;
    runtime_config_.config_crc32 = 0;
}

bool ConfigManager::init() {
    std::puts("Config: resolving active configuration");

    initialized_ = false;
    config_source_[0] = '\0';
    config_crc32_ = 0;

    if (load_active_config_from_littlefs()) {
        initialized_ = true;
    } else {
        std::printf("Config: LittleFS active config unavailable (%s)\n", last_error_);
        if (!load_factory_default()) {
            return false;
        }
        initialized_ = true;
    }

    last_error_ = "ok";
    std::puts("Config: active configuration ready");
    return true;
}

bool ConfigManager::status() const { return initialized_; }

const char* ConfigManager::last_error() const { return last_error_; }

const RuntimeConfig* ConfigManager::runtime_config() const {
    return initialized_ ? &runtime_config_ : nullptr;
}

void ConfigManager::print_summary() const {
    if (!initialized_) {
        std::puts("Config: summary unavailable");
        return;
    }

    std::printf("Config: source=%s\n", runtime_config_.config_source);
    std::printf("Config: crc32=%08lX\n", static_cast<unsigned long>(runtime_config_.config_crc32));
    std::printf("Config: device.name=%s\n", runtime_config_.device.name);
    std::printf("Config: device.mac=%s\n", runtime_config_.device.mac);
    std::printf("Config: ethernet.mode=%s\n", runtime_config_.ethernet.mode);
    if (runtime_config_.ethernet.static_ip[0] != '\0') {
        std::printf("Config: ethernet.static.ip=%s\n", runtime_config_.ethernet.static_ip);
        std::printf("Config: ethernet.static.subnet=%s\n", runtime_config_.ethernet.static_subnet);
        std::printf("Config: ethernet.static.gateway=%s\n", runtime_config_.ethernet.static_gateway);
        std::printf("Config: ethernet.static.dns=%s\n", runtime_config_.ethernet.static_dns);
    }
    std::printf("Config: mqtt.broker=%s\n", runtime_config_.mqtt.broker);
    std::printf("Config: mqtt.port=%lu\n", static_cast<unsigned long>(runtime_config_.mqtt.port));
    std::printf("Config: mqtt.client_id_prefix=%s\n", runtime_config_.mqtt.client_id_prefix);
    std::printf("Config: mqtt.keep_alive_sec=%lu\n",
                static_cast<unsigned long>(runtime_config_.mqtt.keep_alive_sec));
    std::printf("Config: mqtt.discovery_enabled=%s\n",
                runtime_config_.mqtt.discovery_enabled ? "true" : "false");
    std::printf("Config: mqtt.broadcast_destination_id=%s\n",
                runtime_config_.mqtt.broadcast_destination_id);
    std::printf("Config: mqtt.subscribe_topic=%s\n", runtime_config_.mqtt.subscribe_topic);
    std::printf("Config: led.healthy_on_ms=%lu\n",
                static_cast<unsigned long>(runtime_config_.led.healthy_on_ms));
    std::printf("Config: led.healthy_off_ms=%lu\n",
                static_cast<unsigned long>(runtime_config_.led.healthy_off_ms));
}

bool ConfigManager::load_active_config_from_littlefs() {
    if (!flash_config_store_.is_mounted()) {
        last_error_ = "littlefs_unavailable";
        return false;
    }

    unsigned int config_length = 0;
    if (!flash_config_store_.read_text_file(kActiveConfigPath, config_buffer_, sizeof(config_buffer_),
                                            &config_length)) {
        std::puts("Config: /config.json missing in LittleFS, seeding factory default");
        return seed_littlefs_from_factory("active config missing");
    }

    if (!load_and_apply_config_text(config_buffer_, "littlefs", true)) {
        const char* failed_reason = last_error_;
        record_littlefs_event("active config invalid; attempting backup restore\n");

        if (flash_config_store_.read_text_file(kBackupConfigPath, config_buffer_, sizeof(config_buffer_),
                                               &config_length) &&
            load_and_apply_config_text(config_buffer_, "littlefs-backup", true)) {
            std::puts("Config: restored valid backup config from LittleFS");
            flash_config_store_.write_text_file_atomic(kActiveConfigPath, config_buffer_,
                                                       static_cast<unsigned int>(std::strlen(config_buffer_)));
            write_config_meta("littlefs-backup", "backup restore");
            return true;
        }

        last_error_ = failed_reason;
        return seed_littlefs_from_factory("active and backup configs invalid");
    }

    write_config_meta("littlefs", "startup");
    std::puts("Config: loaded /config.json from LittleFS");
    return true;
}

bool ConfigManager::load_factory_default() {
    unsigned int config_length = 0;
    if (!prepare_factory_config_text(config_buffer_, sizeof(config_buffer_), &config_length)) {
        last_error_ = "factory_config_prepare_failed";
        return false;
    }

    if (!load_and_apply_config_text(config_buffer_, "factory", true)) {
        return false;
    }

    std::puts("Config: using factory default config");
    return true;
}

bool ConfigManager::seed_littlefs_from_factory(const char* reason) {
    if (!load_factory_default()) {
        return false;
    }

    if (!flash_config_store_.is_mounted()) {
        return true;
    }

    if (!flash_config_store_.write_text_file_atomic(kActiveConfigPath, config_buffer_,
                                                    static_cast<unsigned int>(std::strlen(config_buffer_)))) {
        std::printf("Config: failed to seed LittleFS active config (%s)\n",
                    flash_config_store_.last_error());
        write_config_meta("factory-ram-only", reason);
        return true;
    }

    flash_config_store_.write_text_file_atomic(kBackupConfigPath, config_buffer_,
                                               static_cast<unsigned int>(std::strlen(config_buffer_)));
    record_littlefs_event("factory config installed into LittleFS\n");
    write_config_meta("factory-seeded", reason);
    return true;
}

bool ConfigManager::load_and_apply_config_text(const char* json_text, const char* source_name,
                                               bool verify_crc) {
    const unsigned int json_length = static_cast<unsigned int>(std::strlen(json_text));
    if (!copy_string(scratch_buffer_, sizeof(scratch_buffer_), json_text, json_length)) {
        last_error_ = "config_too_large";
        return false;
    }

    uint32_t crc32 = 0;
    if (verify_crc && !verify_crc32(scratch_buffer_, &crc32)) {
        return false;
    }

    if (!parse_runtime_config(scratch_buffer_)) {
        return false;
    }

    if (!validate_runtime_config()) {
        last_error_ = "invalid_runtime_config";
        return false;
    }

    copy_string(config_source_, sizeof(config_source_), source_name,
                static_cast<unsigned int>(std::strlen(source_name)));
    config_crc32_ = crc32;
    runtime_config_.config_crc32 = config_crc32_;
    return true;
}

bool ConfigManager::prepare_factory_config_text(char* destination, unsigned int destination_size,
                                                unsigned int* out_length) {
    const unsigned int factory_length = static_cast<unsigned int>(std::strlen(kFactoryConfigJson));
    if (!copy_string(destination, destination_size, kFactoryConfigJson, factory_length)) {
        return false;
    }

    if (!populate_crc32_field(destination)) {
        return false;
    }

    if (out_length != nullptr) {
        *out_length = static_cast<unsigned int>(std::strlen(destination));
    }
    return true;
}

bool ConfigManager::verify_crc32(const char* json_text, uint32_t* out_crc32) {
    const unsigned int json_length = static_cast<unsigned int>(std::strlen(json_text));
    if (!copy_string(scratch_buffer_, sizeof(scratch_buffer_), json_text, json_length)) {
        last_error_ = "config_too_large";
        return false;
    }

    uint32_t stored_crc32 = 0;
    char* crc_value_start = nullptr;
    if (!extract_crc32_value(scratch_buffer_, &stored_crc32, &crc_value_start)) {
        last_error_ = "missing_crc32";
        return false;
    }

    std::memset(crc_value_start, '0', 8);
    const uint32_t computed_crc32 = compute_crc32(scratch_buffer_, json_length);
    if (stored_crc32 != computed_crc32) {
        std::printf("Config: CRC mismatch stored=%08lX computed=%08lX\n",
                    static_cast<unsigned long>(stored_crc32),
                    static_cast<unsigned long>(computed_crc32));
        last_error_ = "crc32_mismatch";
        return false;
    }

    if (out_crc32 != nullptr) {
        *out_crc32 = computed_crc32;
    }
    return true;
}

bool ConfigManager::populate_crc32_field(char* json_text) {
    uint32_t ignored_crc32 = 0;
    char* crc_value_start = nullptr;
    if (!extract_crc32_value(json_text, &ignored_crc32, &crc_value_start)) {
        return false;
    }

    std::memset(crc_value_start, '0', 8);
    const uint32_t computed_crc32 =
        compute_crc32(json_text, static_cast<unsigned int>(std::strlen(json_text)));
    return write_hex32(crc_value_start, computed_crc32);
}

bool ConfigManager::extract_crc32_value(char* json_text, uint32_t* out_stored_crc32,
                                        char** out_crc_value_start) {
    const char* crc_key = "\"crc32\"";
    char* key_start = std::strstr(json_text, crc_key);
    if (key_start == nullptr) {
        return false;
    }

    char* colon = std::strchr(key_start, ':');
    if (colon == nullptr) {
        return false;
    }

    char* value_start = std::strchr(colon, '"');
    if (value_start == nullptr) {
        return false;
    }
    ++value_start;

    char* value_end = std::strchr(value_start, '"');
    if (value_end == nullptr || value_end - value_start != 8) {
        return false;
    }

    unsigned long parsed_value = 0;
    if (std::sscanf(value_start, "%08lx", &parsed_value) != 1) {
        return false;
    }

    if (out_stored_crc32 != nullptr) {
        *out_stored_crc32 = static_cast<uint32_t>(parsed_value);
    }
    if (out_crc_value_start != nullptr) {
        *out_crc_value_start = value_start;
    }
    return true;
}

uint32_t ConfigManager::compute_crc32(const char* data, unsigned int length) const {
    uint32_t crc = 0xFFFFFFFFu;
    for (unsigned int i = 0; i < length; ++i) {
        crc ^= static_cast<uint8_t>(data[i]);
        for (unsigned int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1u)));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

void ConfigManager::record_littlefs_event(const char* message) {
    if (!flash_config_store_.is_mounted() || message == nullptr) {
        return;
    }

    flash_config_store_.append_log_line(kErrorLogPath, message);
}

bool ConfigManager::write_config_meta(const char* active_source, const char* last_event) {
    if (!flash_config_store_.is_mounted()) {
        return false;
    }

    const int bytes_written = std::snprintf(
        scratch_buffer_, sizeof(scratch_buffer_),
        "{\n"
        "  \"revision\": 1,\n"
        "  \"active_source\": \"%s\",\n"
        "  \"config_crc32\": \"%08lX\",\n"
        "  \"last_event\": \"%s\"\n"
        "}\n",
        active_source, static_cast<unsigned long>(config_crc32_), last_event);
    if (bytes_written <= 0 || bytes_written >= static_cast<int>(sizeof(scratch_buffer_))) {
        return false;
    }

    return flash_config_store_.write_text_file_atomic(kConfigMetaPath, scratch_buffer_,
                                                      static_cast<unsigned int>(bytes_written));
}

bool ConfigManager::parse_runtime_config(const char* json_text) {
    const char* device_section = find_section(json_text, "\"device\"");
    const char* ethernet_section = find_section(json_text, "\"ethernet\"");
    const char* ethernet_static_section = find_section(json_text, "\"static\"");
    const char* mqtt_section = find_section(json_text, "\"mqtt\"");
    const char* led_section = find_section(json_text, "\"led\"");

    if (device_section == nullptr || ethernet_section == nullptr || mqtt_section == nullptr ||
        led_section == nullptr) {
        last_error_ = "missing_required_section";
        return false;
    }

    if (!extract_string_value(device_section, "\"name\"", device_name_, sizeof(device_name_))) {
        last_error_ = "missing_device_name";
        return false;
    }

    if (!extract_string_value(device_section, "\"mac\"", device_mac_, sizeof(device_mac_))) {
        last_error_ = "missing_device_mac";
        return false;
    }

    if (!extract_string_value(ethernet_section, "\"mode\"", ethernet_mode_,
                              sizeof(ethernet_mode_))) {
        last_error_ = "missing_ethernet_mode";
        return false;
    }

    ethernet_static_ip_[0] = '\0';
    ethernet_static_subnet_[0] = '\0';
    ethernet_static_gateway_[0] = '\0';
    ethernet_static_dns_[0] = '\0';

    if (std::strcmp(ethernet_mode_, "static") == 0) {
        if (ethernet_static_section == nullptr) {
            last_error_ = "missing_ethernet_static_section";
            return false;
        }

        if (!extract_string_value(ethernet_static_section, "\"ip\"", ethernet_static_ip_,
                                  sizeof(ethernet_static_ip_))) {
            last_error_ = "missing_ethernet_static_ip";
            return false;
        }

        if (!extract_string_value(ethernet_static_section, "\"subnet\"",
                                  ethernet_static_subnet_, sizeof(ethernet_static_subnet_))) {
            last_error_ = "missing_ethernet_static_subnet";
            return false;
        }

        if (!extract_string_value(ethernet_static_section, "\"gateway\"",
                                  ethernet_static_gateway_,
                                  sizeof(ethernet_static_gateway_))) {
            last_error_ = "missing_ethernet_static_gateway";
            return false;
        }

        if (!extract_string_value(ethernet_static_section, "\"dns\"", ethernet_static_dns_,
                                  sizeof(ethernet_static_dns_))) {
            last_error_ = "missing_ethernet_static_dns";
            return false;
        }
    }

    if (!extract_string_value(mqtt_section, "\"broker\"", mqtt_broker_, sizeof(mqtt_broker_))) {
        last_error_ = "missing_mqtt_broker";
        return false;
    }

    if (!extract_uint_value(mqtt_section, "\"port\"", &runtime_config_.mqtt.port)) {
        last_error_ = "missing_mqtt_port";
        return false;
    }

    if (!extract_string_value(mqtt_section, "\"client_id_prefix\"", mqtt_client_id_prefix_,
                              sizeof(mqtt_client_id_prefix_))) {
        last_error_ = "missing_mqtt_client_id_prefix";
        return false;
    }

    if (!extract_string_value(mqtt_section, "\"username\"", mqtt_username_,
                              sizeof(mqtt_username_))) {
        last_error_ = "missing_mqtt_username";
        return false;
    }

    if (!extract_string_value(mqtt_section, "\"password\"", mqtt_password_,
                              sizeof(mqtt_password_))) {
        last_error_ = "missing_mqtt_password";
        return false;
    }

    if (!extract_uint_value(mqtt_section, "\"keep_alive_sec\"",
                            &runtime_config_.mqtt.keep_alive_sec)) {
        last_error_ = "missing_mqtt_keep_alive_sec";
        return false;
    }

    if (!extract_bool_value(mqtt_section, "\"discovery_enabled\"",
                            &runtime_config_.mqtt.discovery_enabled)) {
        last_error_ = "missing_mqtt_discovery_enabled";
        return false;
    }

    if (!extract_string_value(mqtt_section, "\"broadcast_destination_id\"",
                              mqtt_broadcast_destination_id_,
                              sizeof(mqtt_broadcast_destination_id_))) {
        last_error_ = "missing_mqtt_broadcast_destination_id";
        return false;
    }

    if (!extract_string_value(mqtt_section, "\"subscribe_topic\"", mqtt_subscribe_topic_,
                              sizeof(mqtt_subscribe_topic_))) {
        last_error_ = "missing_mqtt_subscribe_topic";
        return false;
    }

    if (!extract_uint_value(led_section, "\"healthy_on_ms\"", &runtime_config_.led.healthy_on_ms)) {
        last_error_ = "missing_led_healthy_on_ms";
        return false;
    }

    if (!extract_uint_value(led_section, "\"healthy_off_ms\"",
                            &runtime_config_.led.healthy_off_ms)) {
        last_error_ = "missing_led_healthy_off_ms";
        return false;
    }

    return true;
}

bool ConfigManager::validate_runtime_config() const {
    if (runtime_config_.device.name[0] == '\0' || runtime_config_.device.mac[0] == '\0') {
        return false;
    }

    if (runtime_config_.ethernet.mode[0] == '\0' || runtime_config_.mqtt.broker[0] == '\0') {
        return false;
    }

    if (std::strcmp(runtime_config_.ethernet.mode, "dhcp") != 0 &&
        std::strcmp(runtime_config_.ethernet.mode, "static") != 0) {
        return false;
    }

    if (std::strcmp(runtime_config_.ethernet.mode, "static") == 0 &&
        (runtime_config_.ethernet.static_ip[0] == '\0' ||
         runtime_config_.ethernet.static_subnet[0] == '\0' ||
         runtime_config_.ethernet.static_gateway[0] == '\0' ||
         runtime_config_.ethernet.static_dns[0] == '\0')) {
        return false;
    }

    if (runtime_config_.mqtt.port == 0 || runtime_config_.mqtt.keep_alive_sec == 0) {
        return false;
    }

    if (runtime_config_.mqtt.client_id_prefix[0] == '\0' ||
        runtime_config_.mqtt.broadcast_destination_id[0] == '\0' ||
        runtime_config_.mqtt.subscribe_topic[0] == '\0') {
        return false;
    }

    if (runtime_config_.led.healthy_on_ms == 0 || runtime_config_.led.healthy_off_ms == 0) {
        return false;
    }

    return true;
}

const char* ConfigManager::find_section(const char* json_text, const char* section_name) const {
    return std::strstr(json_text, section_name);
}

bool ConfigManager::extract_string_value(const char* section_start, const char* key,
                                         char* destination, unsigned int destination_size) {
    const char* key_start = std::strstr(section_start, key);
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
    return copy_string(destination, destination_size, value_start, value_length);
}

bool ConfigManager::extract_uint_value(const char* section_start, const char* key,
                                       uint32_t* out_value) {
    const char* key_start = std::strstr(section_start, key);
    if (key_start == nullptr) {
        return false;
    }

    const char* colon = std::strchr(key_start, ':');
    if (colon == nullptr) {
        return false;
    }

    unsigned long parsed_value = 0;
    if (std::sscanf(colon + 1, " %lu", &parsed_value) != 1) {
        return false;
    }

    *out_value = static_cast<uint32_t>(parsed_value);
    return true;
}

bool ConfigManager::extract_bool_value(const char* section_start, const char* key, bool* out_value) {
    const char* key_start = std::strstr(section_start, key);
    if (key_start == nullptr) {
        return false;
    }

    const char* colon = std::strchr(key_start, ':');
    if (colon == nullptr) {
        return false;
    }

    if (std::strncmp(colon + 1, " true", 5) == 0 || std::strncmp(colon + 1, "true", 4) == 0) {
        *out_value = true;
        return true;
    }

    if (std::strncmp(colon + 1, " false", 6) == 0 || std::strncmp(colon + 1, "false", 5) == 0) {
        *out_value = false;
        return true;
    }

    return false;
}

}  // namespace config
