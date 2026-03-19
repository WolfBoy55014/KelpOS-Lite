//
// Created by wolfboy on 3/9/2026.
//

#include "include/serial_driver.h"

#include "kernel_config.h"
#include "scheduler.h"

void kelp_serial_driver(uint32_t pid, uint32_t* signals, char* args) {
    uint32_t l = 0;

    while (1) {
        if (l >= 100) {
            // check signals
            if (*signals & TASK_SIGTERM) {
                return;
            }
        }

        // send input
        int c = 0;
        int num_chars = 0;
        char chars[CHANNEL_SIZE - 3];

        for (uint16_t i = 0; i < CHANNEL_SIZE - 3; i++) {
            c = getchar_timeout_us(0);
            if (c == PICO_ERROR_TIMEOUT) {
                break;
            }
            chars[i] = c;
            num_chars++;
        }

        if (num_chars == 1) {
            kelp_text_send_input_char(chars[0]);
        } else if (num_chars > 1) {
            kelp_text_send_input_string(chars, num_chars);
        }

        // get output
        c = kelp_text_read_output_char();

        while (c != '\0') {
            putchar(c);
            c = kelp_text_read_output_char();
        }

        l++;

        task_sleep_ms(1);
    }
}
