#pragma once

#include <cstdint>

#include "hardware/flash.h"

namespace storage {

constexpr uint32_t kLittleFsPartitionSizeBytes = 256u * 1024u;
constexpr uint32_t kLittleFsBlockSizeBytes = FLASH_SECTOR_SIZE;
constexpr uint32_t kLittleFsReadSizeBytes = 16u;
constexpr uint32_t kLittleFsProgSizeBytes = FLASH_PAGE_SIZE;
constexpr uint32_t kLittleFsCacheSizeBytes = FLASH_PAGE_SIZE;
constexpr uint32_t kLittleFsLookaheadSizeBytes = 32u;
constexpr uint32_t kLittleFsErrorLogMaxBytes = 4096u;
constexpr uint32_t kLittleFsPartitionOffsetBytes = PICO_FLASH_SIZE_BYTES - kLittleFsPartitionSizeBytes;
constexpr uint32_t kLittleFsBlockCount = kLittleFsPartitionSizeBytes / kLittleFsBlockSizeBytes;

static_assert(kLittleFsPartitionSizeBytes % kLittleFsBlockSizeBytes == 0,
              "LittleFS partition must align to flash sectors");
static_assert(kLittleFsBlockSizeBytes % kLittleFsProgSizeBytes == 0,
              "LittleFS block size must align to flash page size");

}  // namespace storage
