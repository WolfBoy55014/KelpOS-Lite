#include <stdio.h>

#include "block_service.h"
#include "channel.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include "scheduler.h"
#include "scheduler_internal.h"
#include "governor.h"
#include "sd_card_driver.h"
#include "usb_hid_driver.h"
#include "serial_driver.h"
#include "hardware/clocks.h"
#include "text_service.h"
#include "shell.h"

void monitor_task(uint32_t pid, uint32_t* signals, char* args) {
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

uint32_t use_stack(uint32_t i) {
    task_sleep_ms(2);
    uint32_t o = use_stack(i);
    return ++o;
}

void use_stack_task(uint32_t pid, uint32_t* signals, char* args) {
    uint32_t v = 0;
    v = use_stack(0);
    printf("%lu\n", v);
}

void disk_speed_task(uint32_t pid, uint32_t* signals, char* args) {

    task_sleep_ms(5000);

    printf("Starting Speed Test\n");

    uint32_t sector = 0;
    uint32_t duration = 10;
    uint32_t start = time_us_32();
    uint32_t end = start + duration * 1000000;
    uint32_t num_sectors = 100000;
    for (sector = 0; sector < num_sectors; sector += 8) {
        uint8_t buffer[4096];
        uint32_t bytes_read = 0;
        kelp_error_t error = kelp_block_read_bytes(0, buffer, sizeof(buffer), &bytes_read, sector, 8);

        if (error != KELP_OK) {
            printf("Error reading: %ld\n", error);
            return;
        }

        if (bytes_read != 4096) {
            printf("Error reading: tried to read %u bytes but got %lu\n", sizeof(buffer), bytes_read);
            return;
        }

        printf("time: %u\n", (get_core_usage(0) + get_core_usage(1)) / 2);

        if (time_us_32() > end) {
            break;
        }
    }

    uint32_t bytes_read = sector * 512;
    double bytes_per_sec = (double) bytes_read / (double) duration;

    printf("Read %lu sectors\n", sector);
    printf("Bytes per second: %f\n", bytes_per_sec);
}

void system_task(uint32_t pid, uint32_t* signals, char* args) {
    task_request_stack(256);

    // start services
    task_add(kelp_task_text_service, TEXT_SERVICE_PID, 88);
    task_add(kelp_task_block_service, BLOCK_SERVICE_PID, 89);

    // start drivers
    task_add(kelp_task_usb_hid_driver, USB_HID_DRIVER_PID, 88);
    task_add(kelp_serial_driver, SERIAL_DRIVER_PID, 88);
    task_add(kelp_task_sd_card_driver, SD_CARD_DRIVER_PID, 89);

    // start shell
    task_add(kelp_task_shell, KELP_SHELL_PID, 88);

    task_add(disk_speed_task, 12, 88);

    while (1) {
        task_sleep_ms(1000);
    }
}

int main() {
    stdio_init_all();

    task_add(system_task, 10, 255);
    // task_add(monitor_task, 11, 88);

    kernel_start();

    governor_set_mode(GOVERNOR_PERFORMANCE);

    while (true) {
        tight_loop_contents();
    }
}
