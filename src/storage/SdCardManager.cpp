#include "storage/SdCardManager.h"

#include <cstdio>
#include <cstring>

extern "C" {
#include "f_util.h"
#include "hw_config.h"
}

namespace storage {

SdCardManager::SdCardManager() : filesystem_{}, initialized_(false), last_error_("not_initialized") {}

bool SdCardManager::init() {
    std::puts("SD: initializing SPI SD driver");

    if (!sd_init_driver()) {
        last_error_ = "sd_init_driver_failed";
        std::puts("SD: driver initialization failed");
        return false;
    }

    FRESULT mount_result = f_mount(&filesystem_, "", 1);
    if (mount_result != FR_OK) {
        last_error_ = "mount_failed";
        std::printf("SD: f_mount failed: %s (%d)\n", FRESULT_str(mount_result), mount_result);
        return false;
    }

    initialized_ = true;
    last_error_ = "ok";
    std::puts("SD: card mounted successfully");
    return true;
}

bool SdCardManager::read_text_file(const char* path, char* buffer, unsigned int buffer_size,
                                   unsigned int* out_length) const {
    if (!initialized_ || path == nullptr || buffer == nullptr || buffer_size < 2) {
        if (out_length != nullptr) {
            *out_length = 0;
        }
        return false;
    }

    const char* file_path = path;
    if (path[0] == '/') {
        file_path = path + 1;
    }

    FIL file;
    FRESULT open_result = f_open(&file, file_path, FA_READ);
    if (open_result != FR_OK) {
        std::printf("SD: f_open(%s) failed: %s (%d)\n", file_path, FRESULT_str(open_result),
                    open_result);
        if (out_length != nullptr) {
            *out_length = 0;
        }
        return false;
    }

    UINT bytes_read = 0;
    FRESULT read_result = f_read(&file, buffer, buffer_size - 1, &bytes_read);
    FRESULT close_result = f_close(&file);

    if (read_result != FR_OK) {
        std::printf("SD: f_read(%s) failed: %s (%d)\n", file_path, FRESULT_str(read_result),
                    read_result);
        if (out_length != nullptr) {
            *out_length = 0;
        }
        return false;
    }

    if (close_result != FR_OK) {
        std::printf("SD: f_close(%s) failed: %s (%d)\n", file_path, FRESULT_str(close_result),
                    close_result);
        if (out_length != nullptr) {
            *out_length = 0;
        }
        return false;
    }

    buffer[bytes_read] = '\0';
    if (out_length != nullptr) {
        *out_length = static_cast<unsigned int>(bytes_read);
    }
    return true;
}

bool SdCardManager::status() const { return initialized_; }

const char* SdCardManager::last_error() const { return last_error_; }

}  // namespace storage
