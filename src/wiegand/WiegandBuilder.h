#pragma once

#include <cstdint>

namespace wiegand {

struct WiegandFormatDefinition {
    char name[32];
    uint32_t bit_len;
    char facility_code_mask[65];
    char card_number_mask[65];
    uint32_t parity_count;
    uint32_t parity_bit_positions[4];
    char parity_types[4][8];
    char parity_masks[4][65];
};

class WiegandBuilder {
  public:
    bool load_format_definition(const char* format_name, WiegandFormatDefinition* out_definition) const;
    bool build_bit_string(const WiegandFormatDefinition& definition, uint32_t facility_code,
                          uint32_t card_number, char* destination,
                          unsigned int destination_size) const;

  private:
    bool extract_string_value(const char* section_start, const char* key, char* destination,
                              unsigned int destination_size) const;
    bool extract_uint_value(const char* section_start, const char* key, uint32_t* out_value) const;
    bool mask_positions(const char* mask_text, uint32_t bit_len, uint32_t* positions,
                        uint32_t* out_count) const;
    bool write_masked_field(uint8_t* bits, const char* mask_text, uint32_t bit_len,
                            uint32_t value) const;
    bool apply_parity(uint8_t* bits, const WiegandFormatDefinition& definition,
                      uint32_t parity_index) const;
};

}  // namespace wiegand
