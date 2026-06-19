//
// Created by wolfboy on 6/7/2026.
//

#include "hw_config.h"

#include "sd_card_driver.h"

/* Configuration of RP2040 hardware SPI object */
static spi_t spi = {
    .hw_inst = spi0,    // RP2040 SPI component
    .sck_gpio = 18,     // GPIO number (not Pico pin number)
    .mosi_gpio = 19,
    .miso_gpio = 16,
    .baud_rate = 24 * 1000 * 1000   // Actual frequency: 10,416,666.
};

/* SPI Interface */
static sd_spi_if_t spi_if = {
    .spi = &spi,      // Pointer to the SPI driving this card
    .ss_gpio = 17      // The SPI slave select GPIO for this SD card
};

static sd_sdio_if_t sdio_if = {
    /*
    Pins CLK_gpio, D1_gpio, D2_gpio, and D3_gpio are at offsets from pin D0_gpio.
    The offsets are determined by sd_driver\SDIO\rp2040_sdio.pio.
        CLK_gpio = (D0_gpio + SDIO_CLK_PIN_D0_OFFSET) % 32;
        As of this writing, SDIO_CLK_PIN_D0_OFFSET is 30,
            which is -2 in mod32 arithmetic, so:
        CLK_gpio = D0_gpio -2.
        D1_gpio = D0_gpio + 1;
        D2_gpio = D0_gpio + 2;
        D3_gpio = D0_gpio + 3;
    */
    .CMD_gpio = 11,
    .D0_gpio = 12,
    .baud_rate = 125 * 1000 * 1000 / 6  // 20,833,333 Hz
};

/* Configuration of the SD Card socket object */
static sd_card_t sd_card = {
    .type = SD_IF_SDIO,
    .sdio_if_p = &sdio_if,
    .use_card_detect = true,
    .card_detect_gpio = 26,
    .card_detected_true = true
};

/* ********************************************************************** */

size_t sd_get_num() { return SD_CARD_MAX_DEVICES; }

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