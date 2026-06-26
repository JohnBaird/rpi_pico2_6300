#include "hw_config.h"

// Single SPI-attached SD card:
// SPI1 SCK=GP10, MOSI=GP11, MISO=GP12, CS=GP13

static spi_t sd_spi = {
    .hw_inst = spi1,
    .miso_gpio = 12,
    .mosi_gpio = 11,
    .sck_gpio = 10,
    .baud_rate = 10 * 1000 * 1000,
    .spi_mode = 0,
    .no_miso_gpio_pull_up = false,
};

static sd_spi_if_t sd_spi_if = {
    .spi = &sd_spi,
    .ss_gpio = 13,
};

static sd_card_t sd_card = {
    .type = SD_IF_SPI,
    .spi_if_p = &sd_spi_if,
};

size_t sd_get_num() { return 1; }

sd_card_t* sd_get_by_num(size_t num) {
    if (num == 0) {
        return &sd_card;
    }
    return NULL;
}
