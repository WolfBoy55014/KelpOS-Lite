#include <stdio.h>

#include "channel.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include "scheduler.h"
#include "scheduler_internal.h"
#include "governor.h"
#include "usb_hid.h"
#include "com_channel_protocol.h"
#include "hardware/clocks.h"
#include "text_service.h"
#include "shell.h"

void test_task(const uint32_t pid) {
    volatile uint32_t iterations = 0;
    volatile uint32_t result = 1;

    for (int d = 0; d < 1000000; d += 20000) {
        for (int u = 0; u < 20; ++u) {
            uint64_t start = time_us_64();

            // Do some actual computation (not optimized away)
            for (uint32_t i = 0; i < d; i++) {
                result = (result * 1103515245 + 12345) & 0x7fffffff; // LCG PRNG
                iterations++;
            }

            // Optional: store result somewhere so compiler doesn't optimize it away
            (void)result;

            uint64_t time_taken = time_us_64() - start;

            task_sleep_us(100000 - time_taken);
        }
    }
}

void monitor_task(uint32_t pid) {
    const uint8_t length = 20;

    while (true) {
        printf("========= System Report =========\n");

        printf("System Clock: %lu kHz\n", clock_get_hz(clk_sys) / 1000);

        for (uint8_t c = 0; c < CORE_COUNT; c++) {
            printf("Core %u: [", c);

            uint8_t usage = get_core_usage(c);
            for (int u = 0; u < 100; u += 100 / length) {
                if (u <= usage) {
                    printf("=");
                } else {
                    printf(" ");
                }
            }

            printf("] %u%%\n", usage);
        }

        printf("\n        --- CPU Usage ---\n");

        for (uint32_t t = 0; t < MAX_TASKS; t++) {
            task_t *task = &tasks[t];

            if (task->state == TASK_FREE) {
                continue;
            }

            printf("Task %lu: [", task->id);

            uint8_t usage = task->cpu_usage;
            for (int u = 0; u < 100; u += 100 / length) {
                if (u <= usage) {
                    printf("=");
                } else {
                    printf(" ");
                }
            }

            printf("] %u%%\n", usage);
        }

        printf("\n       --- Stack Usage ---\n");

        for (uint32_t t = 0; t < MAX_TASKS; t++) {
            task_t *task = &tasks[t];

            if (task->state == TASK_FREE) {
                continue;
            }

            printf("Task %lu: [", task->id);

            uint8_t usage = task->stack_usage;
            for (int u = 0; u < 100; u += 100 / length) {
                if (u <= usage) {
                    printf("=");
                } else {
                    printf(" ");
                }
            }

            printf("] %u%% (%lu bytes)\n", usage, task->stack_size * 4);
        }

        printf("=================================\n\n");

        task_sleep_ms(1000);
    }
}

void task_tx(uint32_t pid) {
    // char value[] = {
    //     3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3, 2, 3, 8, 4, 6, 2, 6, 4, 3, 3, 8, 3, 2, 7, 9, 5, 0, 2, 8, 8, 4,
    //     1, 9, 7, 1, 6, 9, 3, 9, 9, 3, 7, 5, 1, 0, 5, 8, 2, 0, 9, 7, 4, 9, 4, 4, 5, 9, 2, 3, 0, 7, 8, 1, 6, 4, 0, 6, 2,
    //     8, 6, 2, 0, 8, 9, 9, 8, 6, 2, 8, 0, 3, 4, 8, 2, 5, 3, 4, 2, 1, 1, 7, 0, 6, 7, 9, 8, 2, 1, 4, 8, 0, 8, 6, 5, 1,
    //     3, 2, 8, 2, 3, 0, 6, 6, 4, 7, 0, 9, 3, 8, 4, 4, 6, 0, 9, 5, 5, 0, 5, 8, 2, 2, 3, 1, 7, 2, 5, 3, 5, 9, 4, 0, 8,
    //     1, 2, 8, 4, 8, 1, 1, 1, 7, 4, 5, 0, 2, 8, 4, 1, 0, 2, 7, 0, 1, 9, 3, 8, 5, 2, 1, 1, 0, 5, 5, 5, 9, 6, 4, 4, 6,
    //     2, 2, 9, 4, 8, 9, 5, 4, 9, 3, 0, 3, 8, 1, 9, 6, 4, 4, 2, 8, 8, 1, 0, 9, 7, 5, 6, 6, 5, 9, 3, 3, 4, 4, 6, 1, 2,
    //     8, 4, 7, 5, 6, 4, 8, 2, 3, 3, 7, 8, 6, 7, 8, 3, 1, 6, 5, 2, 7, 1, 2, 0, 1, 9, 0, 9, 1, 4, 5, 6, 4, 8, 5, 6, 6,
    //     9, 2, 3, 4, 6, 0, 3, 4, 8, 6, 1, 0, 4, 5, 4, 3, 2, 6, 6, 4, 8, 2, 1
    // };
    char value[] = "Space is big. You just won't believe how vastly, hugely, mind-bogglingly big it is. I mean, you may think it's a long way down the road to the chemist's, but that's just peanuts to space.\0";
    uint16_t size = 188;
    uint16_t reason = 76;
    uint32_t rx_task_pid = pid + 1;

    while (!task_exists(rx_task_pid)) {
        task_yield();
    }

    task_sleep_ms(2000);

    printf("Value: %s\n", value);

    uint16_t channel_id = com_channel_request(rx_task_pid, true);

    com_send_char_array(channel_id, value, size, reason);

    while (!is_channel_ready_to_write(channel_id)) {
        task_yield();
    }

    task_sleep_ms(1000);
}

void task_rx(uint32_t pid) {
    uint32_t tx_task_pid = pid - 1;

    while (!task_exists(tx_task_pid)) {
        task_yield();
    }

    uint16_t channel_ids[NUM_CHANNELS];
    uint32_t num_connected = get_connected_channels(channel_ids, NUM_CHANNELS);

    while (true) {
        for (int i = 0; i < num_connected; ++i) {

            if (is_channel_ready_to_read(channel_ids[i])) {
                char value[300];
                uint32_t size = 0;
                uint16_t reason;

                com_get_char_array(channel_ids[i], value, sizeof(value), &size, &reason);

                printf("Value: %s Reason: %u\n", value, reason);
            }
        }

        num_connected = get_connected_channels(channel_ids, NUM_CHANNELS);
    }
}

void task_output(uint32_t pid) {
    while (true) {
        char c = kelp_text_read_char();

        if (c != '\0') {
            printf("%c", c);
        }

        task_sleep_ms(1);
    }
}

int main() {
    stdio_init_all();

    // task_add(kelp_governor, 8, 128);
    // task_add(kelp_usb_keyboard, 11, 128);
    // task_add(monitor_task, 12, 127);
    // task_add(test_task, 9, 127);
    // task_add(test_task, 10, 127);

    // task_add(task_tx, 4, 127);
    // task_add(task_rx, 5, 127);

    task_add(kelp_task_shell, KELP_SHELL_PID, 127);
    task_add(kelp_task_text_service, TEXT_SERVICE_PID, 127);
    task_add(kelp_task_usb_hid, USB_HID_DRIVER_PID, 127);

    kernel_start();

    governor_set_mode(GOVERNOR_POWER_SAVE);

    while (true) {
        tight_loop_contents();
    }
}
