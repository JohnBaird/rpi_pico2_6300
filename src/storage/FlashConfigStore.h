#pragma once

#include <cstdint>

#include "storage/FlashLayout.h"

extern "C" {
#include "lfs.h"
}

namespace storage {

class FlashConfigStore {
  public:
    FlashConfigStore();

    bool init();
    bool is_mounted() const;
    const char* last_error() const;

    bool exists(const char* path);
    bool read_text_file(const char* path, char* buffer, unsigned int buffer_size,
                        unsigned int* out_length);
    bool write_text_file_atomic(const char* path, const char* text, unsigned int length);
    bool append_log_line(const char* path, const char* line);
    bool remove_file(const char* path);

  private:
    struct Context {
        uint32_t base_offset_bytes;
    };

    static int read_block(const struct lfs_config* cfg, lfs_block_t block, lfs_off_t off,
                          void* buffer, lfs_size_t size);
    static int prog_block(const struct lfs_config* cfg, lfs_block_t block, lfs_off_t off,
                          const void* buffer, lfs_size_t size);
    static int erase_block(const struct lfs_config* cfg, lfs_block_t block);
    static int sync_block(const struct lfs_config* cfg);

    bool mount();
    bool format_and_mount();
    bool check_partition_bounds() const;
    bool path_exists(const char* path, lfs_info* out_info);
    bool write_file(const char* path, const char* text, unsigned int length, int flags);

    Context context_;
    lfs_t lfs_;
    lfs_config lfs_config_;
    bool mounted_;
    const char* last_error_;
    uint8_t read_buffer_[kLittleFsCacheSizeBytes];
    uint8_t prog_buffer_[kLittleFsCacheSizeBytes];
    uint8_t lookahead_buffer_[kLittleFsLookaheadSizeBytes];
};

}  // namespace storage
