//
// Created by wolfboy on 6/7/2026.
//

#include "hw_config.h"

/* Configuration of RP2040 hardware SPI object */
static spi_t spi = {
    .hw_inst = spi0,    // RP2040 SPI component
    .sck_gpio = 18,     // GPIO number (not Pico pin number)
    .mosi_gpio = 19,
    .miso_gpio = 16,
    .baud_rate = 12 * 1000 * 1000   // Actual frequency: 10416666.
};

/* SPI Interface */
static sd_spi_if_t spi_if = {
    .spi = &spi,      // Pointer to the SPI driving this card
    .ss_gpio = 17      // The SPI slave select GPIO for this SD card
};

/* Configuration of the SD Card socket object */
static sd_card_t sd_card = {
    .type = SD_IF_SPI,
    .spi_if_p = &spi_if,  // Pointer to the SPI interface driving this card
    .use_card_detect = true,
    .card_detect_gpio = 27,
};

/* ********************************************************************** */

size_t sd_get_num() { return 1; }

/**
 * Return a pointer to a sd_card_t object associated with the given physical
 * drive number.
 *
 * \param[in] num The physical drive number.
 *
 * \return A pointer to a sd_card_t object associated with the given physical
 * drive number, or NULL if the physical drive number is invalid.
 */
sd_card_t *sd_get_by_num(size_t num) {
    if (0 == num) {
        // The physical drive number is valid. Return a pointer to the
        // associated sd_card_t object.
        return &sd_card;
    } else {
        // The physical drive number is invalid. Return NULL.
        return NULL;
    }
}