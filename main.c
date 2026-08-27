#include <stdio.h>
#include <stdlib.h>

#include "block_service.h"
#include "channel.h"
#include "com_channel_protocol.h"
#include "file_service.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include "scheduler.h"
#include "scheduler_internal.h"
#include "governor.h"
#include "sd_card_driver.h"
#include "serial_driver.h"
#include "hardware/clocks.h"
#include "text_service.h"
#include "shell.h"
#include "usb_host_driver.h"

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

    for (uint8_t device_id = 0; device_id < BLOCK_SERVICE_MAX_DEVICES; device_id++) {
        printf("+--------- Device %u ---------+\n", device_id);
        uint32_t sector = 0;
        uint32_t duration = 10;
        uint32_t start = time_us_32();
        uint32_t end = start + duration * 1000000;
        uint32_t num_sectors = 100000;
        for (sector = 0; sector < num_sectors; sector += 8) {
            uint8_t buffer[4096];
            uint32_t bytes_read = 0;
            kelp_error_t error = kelp_block_read_bytes(device_id, buffer, sizeof(buffer), &bytes_read, sector, 8);

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
        printf("+----------------------------+\n");
    }
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

void nullfs_format_task(uint32_t pid, uint32_t* signals, char* args) {
    task_request_stack(4096);
    task_sleep_ms(5000);

    printf("Writing Magic for NullFS\n");

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

    if (block_size < 5) {
        printf("Block Size too small for NullFS\n");
    }

    uint8_t* buffer = malloc(block_size);

    strcpy(buffer, "null");

    // write magic
    printf("\nWriting NullFS Magic to Device\n");
    uint32_t bytes_written = 0;
    error = kelp_block_write_bytes(device_id, buffer, block_size, &bytes_written, sector, 1);

    free(buffer);
    if (error != KELP_OK) {
        printf("Error writing Magic: %ld\n", error);
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

// File Service Test Suite
static int test_pass = 0;
static int test_fail = 0;

#define TEST(name) printf("\n--- Test: " #name " ---\n");
#define PASS() do { test_pass++; printf("  [PASS]\n"); } while(0)
#define FAIL(msg) do { test_fail++; printf("  [FAIL]: " msg "\n"); } while(0)

static void print_dirent(const char* label, kelp_fs_dirent_t* entry) {
    printf("  %s: name='%s', type=%d, size=%u, modified=%u\n",
           label, entry->name, entry->type, entry->size, entry->modified);
}

static void test_mount_unmount(void) {
    TEST(mount_unmount)
    kelp_error_t err = kelp_fs_mount(0);
    if (err == KELP_OK) { PASS(); } else { FAIL("mount returned error"); return; }
    err = kelp_fs_unmount(0);
    if (err == KELP_OK) { PASS(); } else { FAIL("unmount returned error"); }
    err = kelp_fs_mount(0);
    if (err != KELP_OK) { FAIL("remount failed - cannot continue tests"); }
}

static void test_stat(void) {
    TEST(stat)
    kelp_fs_dirent_t entry;
    kelp_error_t err;
    err = kelp_fs_stat("/0/", &entry);
    if (err == KELP_OK) {
        print_dirent("root", &entry);
        if (entry.type == FS_DT_DIR) { PASS(); } else { FAIL("root should be a directory"); }
    } else { FAIL("stat root failed"); }
    err = kelp_fs_stat("/0/nonexistent.txt", &entry);
    if (err != KELP_OK) { PASS(); } else { FAIL("stat nonexistent should fail"); }
}

static void test_file_open_close(void) {
    TEST(file_open_close)
    uint32_t handle;
    kelp_error_t err;
    err = kelp_fs_file_open("/0/test_open.txt", FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC, &handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("open for write failed"); return; }
    err = kelp_fs_file_close(handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("close failed"); }
    err = kelp_fs_file_open("/0/test_open.txt", FS_O_RDONLY, &handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("open for read failed"); return; }
    err = kelp_fs_file_close(handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("close after read open failed"); }
    err = kelp_fs_file_open("/0/nonexistent.txt", FS_O_RDONLY, &handle);
    if (err != KELP_OK) { PASS(); } else { FAIL("open non-existent should fail"); }
}

static void test_file_write(void) {
    TEST(file_write)
    uint32_t handle;
    kelp_error_t err;
    uint32_t bytes_written;
    err = kelp_fs_file_open("/0/test_write.txt", FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC, &handle);
    if (err != KELP_OK) { FAIL("open for write failed"); return; }
    const char* test_data = "Hello, KelpOS!";
    err = kelp_fs_file_write(handle, (const uint8_t*)test_data, strlen(test_data), &bytes_written);
    if (err == KELP_OK && bytes_written == strlen(test_data)) { PASS(); } else { FAIL("write failed or wrong byte count"); }
    const char* more_data = " More data appended.";
    err = kelp_fs_file_open("/0/test_write.txt", FS_O_WRONLY | FS_O_APPEND, &handle);
    if (err == KELP_OK) {
        err = kelp_fs_file_write(handle, (const uint8_t*)more_data, strlen(more_data), &bytes_written);
        if (err == KELP_OK && bytes_written == strlen(more_data)) { PASS(); } else { FAIL("append write failed"); }
        kelp_fs_file_close(handle);
    } else { FAIL("open for append failed"); }
    kelp_fs_file_flush(handle);
    err = kelp_fs_file_close(handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("close after append failed"); }
}

static void test_file_read(void) {
    TEST(file_read)
    uint32_t handle;
    kelp_error_t err;
    uint32_t bytes_read;
    char buffer[64] = {0};
    err = kelp_fs_file_open("/0/test_write.txt", FS_O_RDONLY, &handle);
    if (err != KELP_OK) { FAIL("open for read failed"); return; }
    err = kelp_fs_file_read(handle, (uint8_t*)buffer, sizeof(buffer) - 1, &bytes_read);
    if (err == KELP_OK && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("  Read: '%s'\n", buffer);
        PASS();
    } else { FAIL("read failed"); }
    err = kelp_fs_file_close(handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("close after read failed"); }
    err = kelp_fs_file_open("/0/test_open.txt", FS_O_RDONLY, &handle);
    if (err == KELP_OK) {
        err = kelp_fs_file_read(handle, (uint8_t*)buffer, sizeof(buffer), &bytes_read);
        if (err == KELP_OK && bytes_read == 0) { PASS(); }
        else if (err == KELP_OK && bytes_read > 0) {
            printf("  Empty file had %u bytes (previous test data)\n", bytes_read);
            PASS();
        } else { FAIL("read from empty file failed"); }
        kelp_fs_file_close(handle);
    }
}

static void test_file_seek(void) {
    TEST(file_seek)
    uint32_t handle;
    kelp_error_t err;
    err = kelp_fs_file_open("/0/test_write.txt", FS_O_RDONLY, &handle);
    if (err != KELP_OK) { FAIL("open for seek test failed"); return; }
    err = kelp_fs_file_seek(handle, 0, FS_SEEK_SET);
    if (err == KELP_OK) { PASS(); } else { FAIL("seek SET failed"); }
    err = kelp_fs_file_seek(handle, 0, FS_SEEK_END);
    if (err == KELP_OK) { PASS(); } else { FAIL("seek END failed"); }
    err = kelp_fs_file_seek(handle, 0, FS_SEEK_SET);
    if (err == KELP_OK) {
        err = kelp_fs_file_seek(handle, 5, FS_SEEK_CUR);
        if (err == KELP_OK) { PASS(); } else { FAIL("seek CUR failed"); }
    }
    err = kelp_fs_file_close(handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("close after seek test failed"); }
}

static void test_file_tell(void) {
    TEST(file_tell)
    uint32_t handle;
    kelp_error_t err;
    uint32_t pos;
    err = kelp_fs_file_open("/0/test_write.txt", FS_O_RDONLY, &handle);
    if (err != KELP_OK) { FAIL("open for tell test failed"); return; }
    err = kelp_fs_file_tell(handle, &pos);
    if (err == KELP_OK) {
        printf("  Position at start: %u\n", pos);
        if (pos == 0) { PASS(); } else { FAIL("position should be 0 at start"); }
    } else { FAIL("tell failed"); }
    kelp_fs_file_seek(handle, 10, FS_SEEK_SET);
    err = kelp_fs_file_tell(handle, &pos);
    if (err == KELP_OK && pos == 10) { PASS(); } else { FAIL("tell after seek failed"); }
    err = kelp_fs_file_close(handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("close after tell test failed"); }
}

static void test_file_size(void) {
    TEST(file_size)
    uint32_t handle;
    kelp_error_t err;
    uint32_t size;
    err = kelp_fs_file_open("/0/test_write.txt", FS_O_RDONLY, &handle);
    if (err != KELP_OK) { FAIL("open for size test failed"); return; }
    err = kelp_fs_file_size(handle, &size);
    if (err == KELP_OK) {
        printf("  File size: %u bytes\n", size);
        if (size > 0) { PASS(); } else { FAIL("file should have content"); }
    } else { FAIL("file_size failed"); }
    err = kelp_fs_file_close(handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("close after size test failed"); }
}

static void test_file_flush(void) {
    TEST(file_flush)
    uint32_t handle;
    kelp_error_t err;
    err = kelp_fs_file_open("/0/test_flush.txt", FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC, &handle);
    if (err != KELP_OK) { FAIL("open for flush test failed"); return; }
    err = kelp_fs_file_flush(handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("flush failed"); }
    err = kelp_fs_file_close(handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("close after flush test failed"); }
}

static void test_file_truncate(void) {
    TEST(file_truncate)
    uint32_t handle;
    kelp_error_t err;
    uint32_t size_before, size_after;
    err = kelp_fs_file_open("/0/test_flush.txt", FS_O_RDWR, &handle);
    if (err != KELP_OK) { FAIL("open for truncate test failed"); return; }
    err = kelp_fs_file_size(handle, &size_before);
    if (err != KELP_OK) { FAIL("size before truncate failed"); kelp_fs_file_close(handle); return; }
    printf("  Size before truncate: %u\n", size_before);
    err = kelp_fs_file_truncate(handle, 0);
    if (err == KELP_OK) { PASS(); } else { FAIL("truncate to 0 failed"); }
    err = kelp_fs_file_size(handle, &size_after);
    if (err == KELP_OK && size_after == 0) { PASS(); } else { FAIL("size after truncate should be 0"); }
    err = kelp_fs_file_close(handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("close after truncate test failed"); }
}

static void test_file_rename(void) {
    TEST(file_rename)
    uint32_t handle;
    kelp_error_t err;
    kelp_fs_dirent_t entry;
    err = kelp_fs_file_open("/0/test_rename_src.txt", FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC, &handle);
    if (err != KELP_OK) { FAIL("create rename source failed"); return; }
    kelp_fs_file_close(handle);
    err = kelp_fs_stat("/0/test_rename_src.txt", &entry);
    if (err == KELP_OK) { PASS(); } else { FAIL("stat source before rename failed"); }
    err = kelp_fs_file_rename("/0/test_rename_src.txt", "/0/test_rename_dst.txt");
    if (err == KELP_OK) { PASS(); } else { FAIL("rename failed"); }
    err = kelp_fs_stat("/0/test_rename_src.txt", &entry);
    if (err != KELP_OK) { PASS(); } else { FAIL("source should not exist after rename"); }
    err = kelp_fs_stat("/0/test_rename_dst.txt", &entry);
    if (err == KELP_OK) {
        print_dirent("renamed file", &entry);
        PASS();
    } else { FAIL("destination should exist after rename"); }
}

static void test_dir_mkdir(void) {
    TEST(dir_mkdir)
    kelp_error_t err;
    kelp_fs_dirent_t entry;
    err = kelp_fs_dir_mkdir("/0/test_dir");
    if (err == KELP_OK) { PASS(); } else { FAIL("mkdir failed"); }
    err = kelp_fs_stat("/0/test_dir/", &entry);
    if (err == KELP_OK && entry.type == FS_DT_DIR) { PASS(); } else { FAIL("stat new directory failed"); }
    err = kelp_fs_dir_mkdir("/0/test_dir");
    if (err != KELP_OK) { PASS(); } else { FAIL("mkdir existing dir should fail"); }
}

static void test_dir_opendir_readdir(void) {
    TEST(dir_opendir_readdir)
    uint32_t dir_handle;
    kelp_error_t err;
    kelp_fs_dirent_t entry;
    int entry_count = 0;
    err = kelp_fs_dir_opendir("/0/", &dir_handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("opendir failed"); return; }
    do {
        err = kelp_fs_dir_readdir(dir_handle, &entry);
        if (err == KELP_OK) {
            print_dirent("entry", &entry);
            entry_count++;
        }
    } while (err == KELP_OK);
    printf("  Total entries: %d\n", entry_count);
    if (entry_count > 0) { PASS(); } else { FAIL("directory should have entries"); }
    err = kelp_fs_dir_closedir(dir_handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("closedir failed"); }
}

static void test_dir_rewinddir(void) {
    TEST(dir_rewinddir)
    uint32_t dir_handle;
    kelp_error_t err;
    kelp_fs_dirent_t entry;
    int first_count = 0, second_count = 0;
    err = kelp_fs_dir_opendir("/0/", &dir_handle);
    if (err != KELP_OK) { FAIL("opendir for rewind test failed"); return; }
    while (kelp_fs_dir_readdir(dir_handle, &entry) == KELP_OK) { first_count++; }
    err = kelp_fs_dir_rewinddir(dir_handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("rewinddir failed"); }
    while (kelp_fs_dir_readdir(dir_handle, &entry) == KELP_OK) { second_count++; }
    if (first_count == second_count) {
        printf("  Both passes: %d entries\n", first_count);
        PASS();
    } else { FAIL("entry counts don't match after rewind"); }
    err = kelp_fs_dir_closedir(dir_handle);
    if (err == KELP_OK) { PASS(); } else { FAIL("closedir after rewind test failed"); }
}

void test_file_service_task(uint32_t pid) {
    printf("\n========================================\n");
    printf("  FILE SERVICE TEST SUITE\n");
    printf("========================================\n");
    test_pass = 0;
    test_fail = 0;
    test_mount_unmount();
    test_stat();
    test_file_open_close();
    test_file_write();
    test_file_read();
    test_file_seek();
    test_file_tell();
    test_file_size();
    test_file_flush();
    test_file_truncate();
    test_file_rename();
    test_dir_mkdir();
    test_dir_opendir_readdir();
    test_dir_rewinddir();
    printf("\n========================================\n");
    printf("  RESULTS: %d passed, %d failed\n", test_pass, test_fail);
    printf("========================================\n");
    while (true) { task_yield(); }
}

// Create the test
void channel_speed_test_task(uint32_t pid, uint32_t* signals, char* args) {
    task_sleep_ms(2000);
    task_add(channel_sender_task, 14, 89);
    task_add(channel_receiver_task, 15, 89);
}

void system_task(uint32_t pid, uint32_t* signals, char* args) {
    task_request_stack(512);

    // start services
    task_add(kelp_task_text_service, TEXT_SERVICE_PID, 88);
    task_add(kelp_task_block_service, BLOCK_SERVICE_PID, 89);
    task_add(kelp_task_file_service, FILE_SERVICE_PID, 89);

    // start drivers
    task_add(kelp_serial_driver, SERIAL_DRIVER_PID, 88);
    task_add(kelp_task_sd_card_driver, SD_CARD_DRIVER_PID, 89);
    task_add(kelp_task_usb_host_driver, USB_HOST_DRIVER_PID, 89);

    // start shell
    task_add(kelp_task_shell, KELP_SHELL_PID, 88);

    // task_add(disk_speed_task, 12, 88);

    task_sleep_ms(1000);
    printf("Starting Test\n");

    uint32_t handle;
    uint32_t boot_count;
    kelp_error_t error = kelp_fs_file_open("/0/boot_count", FS_O_CREAT | FS_O_RDWR, &handle);
    printf("kelp_fs_open returned with code %ld\n", error);

    uint32_t bytes_read;
    error = kelp_fs_file_read(handle, (uint8_t*)&boot_count, 4, &bytes_read);
    printf("kelp_fs_read returned with code %ld\n", error);
    printf("boot count is %lu\n", boot_count);

    boot_count++;

    error = kelp_fs_file_seek(handle, 0, SEEK_SET);
    printf("kelp_fs_seek returned with code %ld\n", error);

    uint32_t bytes_written;
    error = kelp_fs_file_write(handle, (uint8_t*)&boot_count, 4, &bytes_written);
    printf("kelp_fs_write returned with code %ld\n", error);

    error = kelp_fs_file_seek(handle, 4, SEEK_SET);
    printf("kelp_fs_seek returned with code %ld\n", error);

    uint32_t position;
    error = kelp_fs_file_tell(handle, &position);
    printf("kelp_fs_tell returned with code %ld\n", error);
    printf("cursor is at %lu\n", position);

    uint32_t size_bytes;
    error = kelp_fs_file_size(handle, &size_bytes);
    printf("kelp_fs_size returned with code %ld\n", error);
    printf("size is %lu\n", size_bytes);

    error = kelp_fs_file_close(handle);
    printf("kelp_fs_close returned with code %ld\n", error);

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
