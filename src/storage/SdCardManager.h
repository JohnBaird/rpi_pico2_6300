#pragma once

#include "ff.h"

namespace storage {

class SdCardManager {
  public:
    SdCardManager();

    bool init();
    bool read_text_file(const char* path, char* buffer, unsigned int buffer_size,
                        unsigned int* out_length) const;
    void print_directory(const char* path) const;
    bool status() const;
    const char* last_error() const;

  private:
    FATFS filesystem_;
    bool initialized_;
    const char* last_error_;
};

}  // namespace storage
