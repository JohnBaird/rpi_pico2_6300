#include "wiegand/WiegandBuilder.h"

#include <cstdio>
#include <cstring>

#include "GeneratedWiegandConfig.h"

namespace wiegand {

bool WiegandBuilder::load_format_definition(const char* format_name,
                                            WiegandFormatDefinition* out_definition) const {
    if (format_name == nullptr || out_definition == nullptr) {
        return false;
    }

    char section_key[40] = {0};
    if (std::snprintf(section_key, sizeof(section_key), "\"%s\"", format_name) <= 0) {
        return false;
    }

    const char* section_start = std::strstr(config::kWiegandConfigJson, section_key);
    if (section_start == nullptr) {
        return false;
    }

    std::memset(out_definition, 0, sizeof(*out_definition));
    std::strncpy(out_definition->name, format_name, sizeof(out_definition->name) - 1U);
    if (!extract_uint_value(section_start, "\"bit_len\"", &out_definition->bit_len)) {
        return false;
    }
    if (!extract_string_value(section_start, "\"facility_code_mask\"", out_definition->facility_code_mask,
                              sizeof(out_definition->facility_code_mask))) {
        out_definition->facility_code_mask[0] = '\0';
    }
    if (!extract_string_value(section_start, "\"card_number_mask\"", out_definition->card_number_mask,
                              sizeof(out_definition->card_number_mask))) {
        return false;
    }

    for (uint32_t parity_index = 0; parity_index < 4U; ++parity_index) {
        char key[32] = {0};
        std::snprintf(key, sizeof(key), "\"parity_bit_%lu\"",
                      static_cast<unsigned long>(parity_index));
        if (!extract_uint_value(section_start, key, &out_definition->parity_bit_positions[parity_index])) {
            break;
        }

        std::snprintf(key, sizeof(key), "\"parity_type_bit_%lu\"",
                      static_cast<unsigned long>(parity_index));
        if (!extract_string_value(section_start, key, out_definition->parity_types[parity_index],
                                  sizeof(out_definition->parity_types[parity_index]))) {
            return false;
        }

        std::snprintf(key, sizeof(key), "\"parity_mask_bits_%lu\"",
                      static_cast<unsigned long>(parity_index));
        if (!extract_string_value(section_start, key, out_definition->parity_masks[parity_index],
                                  sizeof(out_definition->parity_masks[parity_index]))) {
            return false;
        }
        out_definition->parity_count++;
    }

    return true;
}

bool WiegandBuilder::build_bit_string(const WiegandFormatDefinition& definition, uint32_t facility_code,
                                      uint32_t card_number, char* destination,
                                      unsigned int destination_size) const {
    if (destination == nullptr || destination_size <= definition.bit_len) {
        return false;
    }

    uint8_t bits[64] = {0};
    if (!write_masked_field(bits, definition.facility_code_mask, definition.bit_len, facility_code)) {
        return false;
    }
    if (!write_masked_field(bits, definition.card_number_mask, definition.bit_len, card_number)) {
        return false;
    }

    for (uint32_t parity_index = 0; parity_index < definition.parity_count; ++parity_index) {
        if (!apply_parity(bits, definition, parity_index)) {
            return false;
        }
    }

    for (uint32_t bit_index = 0; bit_index < definition.bit_len; ++bit_index) {
        destination[bit_index] = bits[bit_index] != 0U ? '1' : '0';
    }
    destination[definition.bit_len] = '\0';
    return true;
}

bool WiegandBuilder::extract_string_value(const char* section_start, const char* key,
                                          char* destination, unsigned int destination_size) const {
    if (section_start == nullptr || key == nullptr || destination == nullptr || destination_size == 0U) {
        return false;
    }
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
    if (value_length + 1U > destination_size) {
        return false;
    }
    std::memcpy(destination, value_start, value_length);
    destination[value_length] = '\0';
    return true;
}

bool WiegandBuilder::extract_uint_value(const char* section_start, const char* key,
                                        uint32_t* out_value) const {
    if (section_start == nullptr || key == nullptr || out_value == nullptr) {
        return false;
    }
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

bool WiegandBuilder::mask_positions(const char* mask_text, uint32_t bit_len, uint32_t* positions,
                                    uint32_t* out_count) const {
    if (mask_text == nullptr || positions == nullptr || out_count == nullptr) {
        return false;
    }
    *out_count = 0U;
    if (mask_text[0] == '\0') {
        return true;
    }
    if (std::strlen(mask_text) != bit_len) {
        return false;
    }
    for (uint32_t index = 0; index < bit_len; ++index) {
        if (mask_text[index] == '1') {
            positions[*out_count] = index;
            (*out_count)++;
        } else if (mask_text[index] != '0') {
            return false;
        }
    }
    return true;
}

bool WiegandBuilder::write_masked_field(uint8_t* bits, const char* mask_text, uint32_t bit_len,
                                        uint32_t value) const {
    if (bits == nullptr) {
        return false;
    }
    if (mask_text == nullptr || mask_text[0] == '\0') {
        return true;
    }

    uint32_t positions[64] = {0};
    uint32_t count = 0U;
    if (!mask_positions(mask_text, bit_len, positions, &count)) {
        return false;
    }
    if (count == 0U) {
        return true;
    }
    if (count < 32U && value >= (1UL << count)) {
        return false;
    }
    for (uint32_t offset = 0; offset < count; ++offset) {
        const uint32_t shift = count - 1U - offset;
        bits[positions[offset]] = static_cast<uint8_t>((value >> shift) & 0x01U);
    }
    return true;
}

bool WiegandBuilder::apply_parity(uint8_t* bits, const WiegandFormatDefinition& definition,
                                  uint32_t parity_index) const {
    uint32_t positions[64] = {0};
    uint32_t count = 0U;
    if (!mask_positions(definition.parity_masks[parity_index], definition.bit_len, positions, &count)) {
        return false;
    }

    const uint32_t parity_array_index = definition.bit_len - 1U - definition.parity_bit_positions[parity_index];
    uint32_t ones_count = 0U;
    for (uint32_t i = 0; i < count; ++i) {
        if (positions[i] == parity_array_index) {
            continue;
        }
        ones_count += bits[positions[i]] != 0U ? 1U : 0U;
    }

    const bool even = std::strcmp(definition.parity_types[parity_index], "even") == 0;
    const bool odd = std::strcmp(definition.parity_types[parity_index], "odd") == 0;
    if (!even && !odd) {
        return false;
    }

    bits[parity_array_index] = even ? static_cast<uint8_t>(ones_count & 0x01U)
                                    : static_cast<uint8_t>((ones_count & 0x01U) == 0U ? 1U : 0U);
    return true;
}

}  // namespace wiegand
