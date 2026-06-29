#include "storage/FlashConfigStore.h"

#include <cstdio>
#include <cstring>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/regs/addressmap.h"
#include "storage/FlashLayout.h"

extern "C" char __flash_binary_end;

namespace storage {
namespace {

constexpr const char* kScratchPath = "/config.tmp";

uint32_t block_to_offset_bytes(uint32_t base_offset_bytes, lfs_block_t block, lfs_off_t off) {
    return base_offset_bytes + (block * kLittleFsBlockSizeBytes) + off;
}

}  // namespace

FlashConfigStore::FlashConfigStore()
    : context_{kLittleFsPartitionOffsetBytes},
      lfs_{},
      lfs_config_{},
      mounted_(false),
      last_error_("not_initialized"),
      read_buffer_{},
      prog_buffer_{},
      lookahead_buffer_{} {
    lfs_config_.context = &context_;
    lfs_config_.read = &FlashConfigStore::read_block;
    lfs_config_.prog = &FlashConfigStore::prog_block;
    lfs_config_.erase = &FlashConfigStore::erase_block;
    lfs_config_.sync = &FlashConfigStore::sync_block;
    lfs_config_.read_size = kLittleFsReadSizeBytes;
    lfs_config_.prog_size = kLittleFsProgSizeBytes;
    lfs_config_.block_size = kLittleFsBlockSizeBytes;
    lfs_config_.block_count = kLittleFsBlockCount;
    lfs_config_.block_cycles = 500;
    lfs_config_.cache_size = kLittleFsCacheSizeBytes;
    lfs_config_.lookahead_size = kLittleFsLookaheadSizeBytes;
    lfs_config_.read_buffer = read_buffer_;
    lfs_config_.prog_buffer = prog_buffer_;
    lfs_config_.lookahead_buffer = lookahead_buffer_;
    lfs_config_.name_max = 64;
    lfs_config_.file_max = 16 * 1024;
    lfs_config_.attr_max = 0;
    lfs_config_.metadata_max = kLittleFsBlockSizeBytes;
    lfs_config_.inline_max = 0;
}

bool FlashConfigStore::init() {
    std::printf("LittleFS: preparing partition offset=0x%08lx size=%lu bytes\n",
                static_cast<unsigned long>(kLittleFsPartitionOffsetBytes),
                static_cast<unsigned long>(kLittleFsPartitionSizeBytes));

    if (!check_partition_bounds()) {
        last_error_ = "flash_partition_overlaps_firmware";
        return false;
    }

    if (mount()) {
        last_error_ = "ok";
        return true;
    }

    std::printf("LittleFS: mount failed (%s), formatting partition\n", last_error_);
    return format_and_mount();
}

bool FlashConfigStore::mount_read_only() {
    std::printf("LittleFS: read-only mount request offset=0x%08lx size=%lu bytes\n",
                static_cast<unsigned long>(kLittleFsPartitionOffsetBytes),
                static_cast<unsigned long>(kLittleFsPartitionSizeBytes));

    if (!check_partition_bounds()) {
        last_error_ = "flash_partition_overlaps_firmware";
        return false;
    }

    if (mounted_) {
        last_error_ = "ok";
        return true;
    }

    return mount();
}

bool FlashConfigStore::is_mounted() const { return mounted_; }

const char* FlashConfigStore::last_error() const { return last_error_; }

bool FlashConfigStore::exists(const char* path) { return path_exists(path, nullptr); }

bool FlashConfigStore::read_text_file(const char* path, char* buffer, unsigned int buffer_size,
                                      unsigned int* out_length) {
    if (!mounted_ || buffer == nullptr || buffer_size == 0) {
        return false;
    }

    lfs_file_t file{};
    const int open_result = lfs_file_open(&lfs_, &file, path, LFS_O_RDONLY);
    if (open_result < 0) {
        return false;
    }

    const lfs_soff_t size = lfs_file_size(&lfs_, &file);
    if (size < 0 || static_cast<unsigned int>(size) + 1 > buffer_size) {
        lfs_file_close(&lfs_, &file);
        return false;
    }

    const lfs_ssize_t bytes_read =
        lfs_file_read(&lfs_, &file, buffer, static_cast<lfs_size_t>(size));
    lfs_file_close(&lfs_, &file);
    if (bytes_read != size) {
        return false;
    }

    buffer[size] = '\0';
    if (out_length != nullptr) {
        *out_length = static_cast<unsigned int>(size);
    }
    return true;
}

bool FlashConfigStore::write_text_file_atomic(const char* path, const char* text,
                                              unsigned int length) {
    if (!mounted_ || path == nullptr || text == nullptr) {
        return false;
    }

    if (!write_file(kScratchPath, text, length, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC)) {
        return false;
    }

    if (exists(path) && lfs_remove(&lfs_, path) < 0) {
        last_error_ = "remove_before_rename_failed";
        lfs_remove(&lfs_, kScratchPath);
        return false;
    }

    if (lfs_rename(&lfs_, kScratchPath, path) < 0) {
        last_error_ = "rename_failed";
        lfs_remove(&lfs_, kScratchPath);
        return false;
    }

    last_error_ = "ok";
    return true;
}

bool FlashConfigStore::append_log_line(const char* path, const char* line) {
    if (!mounted_ || path == nullptr || line == nullptr) {
        return false;
    }

    lfs_info info{};
    if (path_exists(path, &info) &&
        info.size + static_cast<lfs_size_t>(std::strlen(line)) > kLittleFsErrorLogMaxBytes) {
        lfs_remove(&lfs_, path);
    }

    return write_file(path, line, static_cast<unsigned int>(std::strlen(line)),
                      LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND);
}

bool FlashConfigStore::remove_file(const char* path) {
    if (!mounted_ || path == nullptr) {
        return false;
    }

    if (!exists(path)) {
        return true;
    }

    if (lfs_remove(&lfs_, path) < 0) {
        last_error_ = "remove_failed";
        return false;
    }

    last_error_ = "ok";
    return true;
}

int FlashConfigStore::read_block(const struct lfs_config* cfg, lfs_block_t block, lfs_off_t off,
                                 void* buffer, lfs_size_t size) {
    const Context* context = static_cast<const Context*>(cfg->context);
    const uint32_t offset = block_to_offset_bytes(context->base_offset_bytes, block, off);
    const uint8_t* flash_address =
        reinterpret_cast<const uint8_t*>(XIP_BASE + offset);
    std::memcpy(buffer, flash_address, size);
    return 0;
}

int FlashConfigStore::prog_block(const struct lfs_config* cfg, lfs_block_t block, lfs_off_t off,
                                 const void* buffer, lfs_size_t size) {
    const Context* context = static_cast<const Context*>(cfg->context);
    const uint32_t offset = block_to_offset_bytes(context->base_offset_bytes, block, off);
    const uint32_t interrupts = save_and_disable_interrupts();
    flash_range_program(offset, static_cast<const uint8_t*>(buffer), size);
    restore_interrupts(interrupts);
    return 0;
}

int FlashConfigStore::erase_block(const struct lfs_config* cfg, lfs_block_t block) {
    const Context* context = static_cast<const Context*>(cfg->context);
    const uint32_t offset = block_to_offset_bytes(context->base_offset_bytes, block, 0);
    const uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(offset, kLittleFsBlockSizeBytes);
    restore_interrupts(interrupts);
    return 0;
}

int FlashConfigStore::sync_block(const struct lfs_config* cfg) {
    (void)cfg;
    return 0;
}

bool FlashConfigStore::mount() {
    const int mount_result = lfs_mount(&lfs_, &lfs_config_);
    if (mount_result < 0) {
        last_error_ = "mount_failed";
        mounted_ = false;
        return false;
    }

    mounted_ = true;
    std::puts("LittleFS: mounted successfully");
    return true;
}

bool FlashConfigStore::format_and_mount() {
    const int format_result = lfs_format(&lfs_, &lfs_config_);
    if (format_result < 0) {
        last_error_ = "format_failed";
        mounted_ = false;
        return false;
    }

    return mount();
}

bool FlashConfigStore::check_partition_bounds() const {
    const auto binary_end_offset = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&__flash_binary_end) - XIP_BASE);
    return binary_end_offset <= kLittleFsPartitionOffsetBytes;
}

bool FlashConfigStore::path_exists(const char* path, lfs_info* out_info) {
    if (!mounted_ || path == nullptr) {
        return false;
    }

    lfs_info info{};
    const int stat_result = lfs_stat(&lfs_, path, &info);
    if (stat_result < 0) {
        return false;
    }

    if (out_info != nullptr) {
        *out_info = info;
    }
    return true;
}

bool FlashConfigStore::write_file(const char* path, const char* text, unsigned int length,
                                  int flags) {
    lfs_file_t file{};
    const int open_result = lfs_file_open(&lfs_, &file, path, flags);
    if (open_result < 0) {
        last_error_ = "open_write_failed";
        return false;
    }

    if (length > 0) {
        const lfs_ssize_t bytes_written = lfs_file_write(&lfs_, &file, text, length);
        if (bytes_written != static_cast<lfs_ssize_t>(length)) {
            lfs_file_close(&lfs_, &file);
            last_error_ = "write_failed";
            return false;
        }
    }

    if (lfs_file_sync(&lfs_, &file) < 0) {
        lfs_file_close(&lfs_, &file);
        last_error_ = "sync_failed";
        return false;
    }

    if (lfs_file_close(&lfs_, &file) < 0) {
        last_error_ = "close_failed";
        return false;
    }

    last_error_ = "ok";
    return true;
}

}  // namespace storage
