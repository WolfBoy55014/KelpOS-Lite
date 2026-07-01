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

void disk_write_task(uint32_t pid, uint32_t* signals, char* args) {
    task_request_stack(4096);
    task_sleep_ms(5000);

    printf("Starting Write Test\n");

    uint8_t device_id = 0;
    uint32_t sector = 0;

    uint8_t new_data[3072] =
"O Deep Thought computer,\" he said, \"the task we have designed you to perform is this. We want you to tell us....\" he paused, \"The Answer.\"\
\"The Answer?\" said Deep Thought. \"The Answer to what?\"\
\"Life!\" urged Fook.\
\"The Universe!\" said Lunkwill.\
\"Everything!\" they said in chorus.\
Deep Thought paused for a moment's reflection.\
\"Tricky,\" he said finally.\
\"But can you do it?\"\
Again, a significant pause.\
\"Yes,\" said Deep Thought, \"I can do it.\"\
\"There is an answer?\" said Fook with breathless excitement.\
\"Yes,\" said Deep Thought. \"Life, the Universe, and Everything. There is an answer. But, I'll have to think about it.\"\
...\
Fook glanced impatiently at his watch.\
\"How long?\" he said.\
\"Seven and a half million years,\" said Deep Thought.\
Lunkwill and Fook blinked at each other.\
\"Seven and a half million years...!\" they cried in chorus.\
\"Yes,\" declaimed Deep Thought, \"I said I’d have to think about it, didn’t I?\"\
\
[Seven and a half million years later.... Fook and Lunkwill are long gone, but their descendents continue what they started]\
\
\"We are the ones who will hear,\" said Phouchg, \"the answer to the great question of Life....!\"\
\"The Universe...!\" said Loonquawl.\
\"And Everything...!\"\
\"Shhh,\" said Loonquawl with a slight gesture. \"I think Deep Thought is preparing to speak!\"\
There was a moment's expectant pause while panels slowly came to life on the front of the console. Lights flashed on and off experimentally and settled down into a businesslike pattern. A soft low hum came from the communication channel.\
\
\"Good Morning,\" said Deep Thought at last.\
\"Er..good morning, O Deep Thought\" said Loonquawl nervously, \"do you have...er, that is...\"\
\"An Answer for you?\" interrupted Deep Thought majestically. \"Yes, I have.\"\
The two men shivered with expectancy. Their waiting had not been in vain.\
\"There really is one?\" breathed Phouchg.\
\"There really is one,\" confirmed Deep Thought.\
\"To Everything? To the great Question of Life, the Universe and everything?\"\
\"Yes.\"\
Both of the men had been trained for this moment, their lives had been a preparation for it, they had been selected at birth as those who would witness the answer, but even so they found themselves gasping and squirming like excited children.\
\"And you're ready to give it to us?\" urged Loonsuawl.\
\"I am.\"\
\"Now?\"\
\"Now,\" said Deep Thought.\
They both licked their dry lips.\
\"Though I don't think,\" added Deep Thought. \"that you're going to like it.\"\
\"Doesn't matter!\" said Phouchg. \"We must know it! Now!\"\
\"Now?\" inquired Deep Thought.\
\"Yes! Now...\"\
\"All right,\" said the computer, and settled into silence again. The two men fidgeted. The tension was unbearable.\
\"You're really not going to like it,\" observed Deep Thought.\
\"Tell us!\"\
\"All right,\" said Deep Thought. \"The Answer to the Great Question...\"\
\"Yes..!\"\
\"Of Life, the Universe and Everything...\" said Deep Thought.\
\"Yes...!\"\
\"Is...\" said Deep Thought, and paused.\
\"Yes...!\"\
\"Is...\"\
\"Yes...!!!...?\"\
\"Forty-two,\" said Deep Thought, with infinite majesty and calm.\"";
    uint8_t old_data[sizeof(new_data)];

    // get original data
    printf("\nReading original data\n");
    uint32_t bytes_read = 0;
    kelp_error_t error = kelp_block_read_bytes(device_id, old_data, sizeof(old_data), &bytes_read, sector, sizeof(old_data) / 512);

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
