#include <stdio.h>
#include <stdlib.h>

#include "block_service.h"
#include "channel.h"
#include "com_channel_protocol.h"
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

        // printf("time: %u\n", (get_core_usage(0) + get_core_usage(1)) / 2);

        if (time_us_32() > end) {
            break;
        }
    }

    uint32_t bytes_read = sector * 512;
    double bytes_per_sec = (double) bytes_read / (double) duration;

    printf("Read %lu sectors\n", sector);
    printf("Bytes per second: %f\n", bytes_per_sec);
}

void disk_write_task(uint32_t pid, uint32_t* signals, char* args) {
    task_request_stack(4096);
    task_sleep_ms(5000);

    printf("Starting Write Test\n");

    uint8_t device_id = 0;
    uint32_t sector = 0;

    uint32_t block_size = 0;
    kelp_error_t error = kelp_block_get_block_size(device_id, &block_size);
    if (error != KELP_OK) {
        printf("Error getting block size: %ld\n", error);
        return;
    }
    printf("Device Block Size is %lu bytes\n", block_size);

    uint32_t block_count = 0;
    error = kelp_block_get_block_count(device_id, &block_count);
    if (error != KELP_OK) {
        printf("Error getting block count: %ld\n", error);
        return;
    }
    printf("Device Block Size is %lu bytes\n", block_count);

    uint8_t new_data[512] =
"He felt that his whole life was some kind of dream and he sometimes wondered whose it was and whether they were enjoying it.";
    uint8_t old_data[sizeof(new_data)];

    // get original data
    printf("\nReading original data\n");
    uint32_t bytes_read = 0;
    error = kelp_block_read_bytes(device_id, old_data, sizeof(old_data), &bytes_read, sector, sizeof(old_data) / 512);

    if (error != KELP_OK) {
        printf("Error reading original data: %ld\n", error);
        return;
    }

    if (bytes_read != sizeof(old_data)) {
        printf("Error reading original data: tried to read %u bytes but got %lu\n", sizeof(old_data), bytes_read);
        return;
    }

    // write new data
    printf("\nWriting new data\n");
    bytes_read = 0;
    error = kelp_block_write_bytes(device_id, new_data, sizeof(new_data), &bytes_read, sector, sizeof(new_data) / 512);

    if (error != KELP_OK) {
        printf("Error writing new data: %ld\n", error);
        return;
    }

    if (bytes_read != sizeof(new_data)) {
        printf("Error writing new data: tried to write %u bytes but got %lu\n", sizeof(new_data), bytes_read);
        return;
    }

    // check if new data was written
    printf("\nChecking if new data was written\n");
    bytes_read = 0;
    uint8_t data[sizeof(new_data)];
    error = kelp_block_read_bytes(device_id, data, sizeof(data), &bytes_read, sector, sizeof(data) / 512);

    if (error != KELP_OK) {
        printf("Error reading new data: %ld\n", error);
        return;
    }

    if (bytes_read != sizeof(data)) {
        printf("Error reading new data: tried to read %u bytes but got %lu\n", sizeof(data), bytes_read);
        return;
    }

    // validate data gainst what we tried to write
    for (uint32_t i = 0; i < sizeof(new_data); i++) {
        printf("%c", data[i]);
        if (data[i] != new_data[i]) {
            printf("\nError validating write, byte %lu does not match\n", i);
        }
    }

    // write back old data
    printf("\nRestoring old data\n");
    bytes_read = 0;
    error = kelp_block_write_bytes(device_id, old_data, sizeof(old_data), &bytes_read, sector, sizeof(old_data) / 512);

    if (error != KELP_OK) {
        printf("Error writing back original data: %ld\n", error);
        return;
    }

    if (bytes_read != sizeof(old_data)) {
        printf("Error writing back original data: tried to write %u bytes but got %lu\n", sizeof(old_data), bytes_read);
        return;
    }
}

#define TEST_DATA_SIZE 4096
#define BUFFER_SIZE TEST_DATA_SIZE

static uint8_t tx_buffer[BUFFER_SIZE];
static uint8_t rx_buffer[BUFFER_SIZE];

// Sender task
void channel_sender_task(uint32_t pid, uint32_t* signals, char* args) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(pid + 1, false, &channel_id);
    if (error != KELP_OK) {
        printf("Error requesting channel id: %ld\n", error);
        return;
    }
    uint32_t total_sent = 0;
    uint32_t start_us = time_us_32();

    printf("Channel speed test on channel %u\n", channel_id);
    printf("Sending 1000 x 4096 byte messages\n\n");

    for (uint32_t i = 0; i < 1000; i++) {
        // Fill data
        for (uint32_t b = 0; b < TEST_DATA_SIZE; b++) {
            tx_buffer[b] = (uint8_t)(b & 0xFF);
        }

        // Send using array protocol (adds 7-byte header: type + reason + size)
        // uint32_t debug_start_us = time_us_32();
        kelp_error_t result = com_send_char_array_blocking(channel_id, tx_buffer, TEST_DATA_SIZE, 0x0001);
        // printf("%s: %d took %f ms\n", __FUNCTION__, __LINE__, (float)(time_us_32() - debug_start_us) / 1000.0);
        if (result != KELP_OK) {
            printf("Sender error at iteration %lu: %d\n", i, result);
            break;
        }
        total_sent += TEST_DATA_SIZE;
    }

    uint32_t end_us = time_us_32();
    uint32_t duration_us = end_us - start_us;
    if (duration_us > 0) {
        float throughput = (float)total_sent / (duration_us / 1000000.0f);
        printf("Sender: %lu bytes in %lu us (%.2f KB/s, %.2f MB/s)\n",
               total_sent, duration_us,
               throughput / 1024.0f,
               throughput / 1048576.0f);
    }
}

// Receiver task
void channel_receiver_task(uint32_t pid, uint32_t* signals, char* args) {
    task_sleep_ms(20);
    uint16_t channel_ids[NUM_CHANNELS];
    uint16_t num_channels = 0;
    kelp_error_t error = get_connected_channels(channel_ids, &num_channels, NUM_CHANNELS);
    if (error != KELP_OK || num_channels == 0) {
        printf("Error requesting channel id: %ld\n", error);
        return;
    }

    uint16_t channel_id = channel_ids[0];
    uint32_t total_recv = 0;
    uint32_t start_us = time_us_32();

    for (uint32_t i = 0; i < 1000; i++) {
        uint32_t data_size = 0;
        uint16_t reason = 0;
        // Receive using array protocol (reads 7-byte header + data)
        // uint32_t debug_start_us = time_us_32();
        kelp_error_t result = com_get_char_array_blocking(channel_id, rx_buffer, BUFFER_SIZE, &data_size, &reason);
        // printf("%s: %d took %f ms\n", __FUNCTION__, __LINE__, (float)(time_us_32() - debug_start_us) / 1000.0);
        if (result != KELP_OK) {
            printf("Receiver error at iteration %lu: %d\n", i, result);
            break;
        }
        total_recv += data_size;
    }

    uint32_t end_us = time_us_32();
    uint32_t duration_us = end_us - start_us;
    if (duration_us > 0) {
        float throughput = (float)total_recv / (duration_us / 1000000.0f);
        printf("Receiver: %lu bytes in %lu us (%.2f KB/s, %.2f MB/s)\n",
               total_recv, duration_us,
               throughput / 1024.0f,
               throughput / 1048576.0f);
    }
}

// Create the test
void channel_speed_test_task(uint32_t pid, uint32_t* signals, char* args) {
    task_sleep_ms(2000);
    task_add(channel_sender_task, 14, 89);
    task_add(channel_receiver_task, 15, 89);
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

    task_add(channel_speed_test_task, 12, 88);

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
