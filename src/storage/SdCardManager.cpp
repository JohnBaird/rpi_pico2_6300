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

void SdCardManager::print_directory(const char* path) const {
    if (!initialized_) {
        std::puts("SD: directory listing skipped because card is not mounted");
        return;
    }

    const char* directory_path = path;
    if (directory_path == nullptr || directory_path[0] == '\0' ||
        (directory_path[0] == '/' && directory_path[1] == '\0')) {
        directory_path = "";
    } else if (directory_path[0] == '/') {
        directory_path = path + 1;
    }

    DIR directory;
    FRESULT open_result = f_opendir(&directory, directory_path);
    if (open_result != FR_OK) {
        std::printf("SD: f_opendir(%s) failed: %s (%d)\n",
                    directory_path[0] != '\0' ? directory_path : "/",
                    FRESULT_str(open_result), open_result);
        return;
    }

    std::printf("SD: listing %s\n", directory_path[0] != '\0' ? directory_path : "/");

    while (true) {
        FILINFO file_info;
        std::memset(&file_info, 0, sizeof(file_info));
        const FRESULT read_result = f_readdir(&directory, &file_info);
        if (read_result != FR_OK) {
            std::printf("SD: f_readdir(%s) failed: %s (%d)\n",
                        directory_path[0] != '\0' ? directory_path : "/",
                        FRESULT_str(read_result), read_result);
            break;
        }

        if (file_info.fname[0] == '\0') {
            break;
        }

        const bool is_directory = (file_info.fattrib & AM_DIR) != 0;
        std::printf("SD:   %s %s", is_directory ? "[DIR ]" : "[FILE]", file_info.fname);
        if (!is_directory) {
            std::printf(" (%lu bytes)", static_cast<unsigned long>(file_info.fsize));
        }
        std::putchar('\n');
    }

    const FRESULT close_result = f_closedir(&directory);
    if (close_result != FR_OK) {
        std::printf("SD: f_closedir(%s) failed: %s (%d)\n",
                    directory_path[0] != '\0' ? directory_path : "/",
                    FRESULT_str(close_result), close_result);
    }
}

bool SdCardManager::status() const { return initialized_; }

const char* SdCardManager::last_error() const { return last_error_; }

}  // namespace storage
