//
// Created by wolfboy on 3/9/2026.
//

#include "include/serial_driver.h"

void kelp_serial_driver(uint32_t pid) {
    while (1) {
        // send input
        int c = getchar_timeout_us(5);

        if (c != PICO_ERROR_TIMEOUT) {
            kelp_text_send_input_char((char) c);
        }

        // get output
        c = kelp_text_read_output_char();

        if (c != '\0') {
            putchar(c);
        }

        sleep_ms(1);
    }
}