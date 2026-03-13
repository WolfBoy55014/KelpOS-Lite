//
// Created by wolfboy on 3/9/2026.
//

#include "include/serial_driver.h"

#include "scheduler.h"

void kelp_serial_driver(uint32_t pid, uint32_t signals, char* args) {
    while (1) {
        // send input
        int c = getchar_timeout_us(0);

        // TODO: Use input_str
        while (c != PICO_ERROR_TIMEOUT) {
            kelp_text_send_input_char((char) c);
            c = getchar_timeout_us(0);
        }

        // get output
        c = kelp_text_read_output_char();
        
        while (c != '\0') {
            putchar(c);
            c = kelp_text_read_output_char();
        }

        sleep_ms(1);
    }
}
