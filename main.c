#include <stdio.h>

#include "channel.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include "scheduler.h"
#include "scheduler_internal.h"
#include "governor.h"
#include "usb_keyboard.h"
#include "com_channel_protocol.h"

void test_task(const uint32_t pid) {
    volatile uint32_t iterations = 0;
    volatile uint32_t result = 1;

    for (int d = 0; d < 1000000; d += 20000) {
        for (int u = 0; u < 40; ++u) {
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

        for (uint8_t c = 0; c < CORE_COUNT; c++) {
            printf("Core %u: [", c);

            uint8_t usage = get_core_usage(c);
            for (int u = 0; u < 100; u += 100 / length) {
                if (u <= usage) {
                    printf("█");
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
                    printf("░");
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
                    printf("░");
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
    int64_t value = -4642831095572139008;
    uint16_t reason = 8;
    uint32_t rx_task_pid = pid + 1;

    while (!task_exists(rx_task_pid)) {
        task_yield();
    }

    task_sleep_ms(2000);

    uint16_t channel_id = com_channel_request(rx_task_pid);

    com_send_int64(channel_id, value, reason);

    while (!is_channel_ready_to_write(channel_id)) {
        task_yield();
    }

    task_sleep_ms(1000);

    com_channel_free(channel_id);
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
                int64_t value;
                uint16_t reason;

                com_get_int64(channel_ids[i], &value, &reason);

                printf("Value: %lld, Reason: %u\n", value, reason);
            }
        }

        num_connected = get_connected_channels(channel_ids, NUM_CHANNELS);
    }
}

int main() {
    stdio_init_all();

    kelp_governor_set_power_mode(GOVERNOR_POWER_SAVE);

    // task_add(kelp_governor, 8, 128);
    // task_add(kelp_usb_keyboard, 11, 128);
    // task_add(monitor_task, 12, 127);
    // task_add(test_task, 9, 127);
    // task_add(test_task, 10, 127);

    task_add(task_tx, 4, 127);
    task_add(task_rx, 5, 127);

    kernel_start();

    while (true) {
        tight_loop_contents();
    }
}
