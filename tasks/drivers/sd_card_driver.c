//
// Created by wolfboy on 6/7/2026.
//

#include "sd_card_driver.h"

#include <stdint.h>

#include "block_service.h"
#include "channel.h"
#include "com_channel_protocol.h"
#include "hw_config.h"
#include "scheduler.h"
#include "sd_card.h"
#include "kernel_config.h"
#include "error_codes.h"
#include "scheduler_internal.h"

static int16_t device_id_2_sd_id[BLOCK_SERVICE_MAX_DEVICES];
static struct sd_driver_details_t sd_driver_details[SD_CARD_MAX_DEVICES];

static uint32_t sd_bound_count(uint8_t sd_id, uint32_t start, uint32_t count) {
    if (sd_id >= SD_CARD_MAX_DEVICES) {
        return 0;
    }
    struct sd_driver_details_t* details = &sd_driver_details[sd_id];

    // Clamp start to valid range
    if (start >= details->block_count) {
        return 0;
    }

    uint32_t end = start + count;
    if (end <= details->block_count) {
        return count;
    }

    return details->block_count - start;
}

// Helper: extract a valid sd_card_t* from a request, or return NULL if invalid
static sd_card_t* get_sd_from_request(const char* data, int8_t* out_sd_id) {
    uint8_t device_id = data[0];
    if (device_id >= BLOCK_SERVICE_MAX_DEVICES) {
        return NULL;
    }
    int16_t sd_id = device_id_2_sd_id[device_id];
    if (sd_id < 0 || sd_id >= SD_CARD_MAX_DEVICES) {
        return NULL;
    }
    *out_sd_id = (int8_t)sd_id;
    return sd_get_by_num(sd_id);
}

// Extract a 32-bit big-endian value from a buffer
static inline uint32_t get_uint32_be(const char* data) {
    return ((uint8_t)data[0] << 24) | ((uint8_t)data[1] << 16) |
           ((uint8_t)data[2] << 8)  |  (uint8_t)data[3];
}

#define CHECK_FOR_OVERFLOW __asm__ volatile ("mov %0, sp" : "=r" (sp)); \
                           printf("%d pointer-stack dist: %lld, stack usage (bytes): %lu \n", __LINE__, (int64_t)sp - (int64_t)get_scheduler()->current_task->stack, (uint32_t)get_scheduler()->current_task->stack_base - sp);

kelp_error_t kelp_sd_handle_read_request(uint16_t channel_id, char data[CHANNEL_SIZE], uint16_t size) {
    uint32_t sp;
    CHECK_FOR_OVERFLOW;
    if (size != 9) {
        return KELP_PROTOCOL;
    }
    int8_t sd_id;
    sd_card_t* sd_card_p = get_sd_from_request(data, &sd_id);
    if (!sd_card_p) {
        return KELP_INVALID_ID;
    }
    CHECK_FOR_OVERFLOW;

    uint32_t start = get_uint32_be(&data[1]);
    uint32_t count = get_uint32_be(&data[5]);
    count = sd_bound_count(sd_id, start, count);
    if (count == 0) {
        return KELP_TOO_BIG;
    }
    uint32_t return_buffer_size = count * 512;
    CHECK_FOR_OVERFLOW;

    // Ensure stack is large enough before allocating the VLA
    kelp_error_t error = task_stack_fit_buffer(SD_CARD_STACK_HEADROOM, return_buffer_size, false);
    KELP_RETURN_ON_ERROR(error);
    CHECK_FOR_OVERFLOW;

    uint8_t buffer[return_buffer_size];
    CHECK_FOR_OVERFLOW;

    block_dev_err_t rc = sd_card_p->read_blocks(sd_card_p, buffer, start, count);
    if (rc != SD_BLOCK_DEVICE_ERROR_NONE) {
        return KELP_IO;
    }
    CHECK_FOR_OVERFLOW;

    error = com_send_char_array_blocking(channel_id, buffer, return_buffer_size, REASON_BLOCK_READ_BYTES);
    KELP_RETURN_ON_ERROR(error);
    CHECK_FOR_OVERFLOW;

    return KELP_OK;
}

kelp_error_t kelp_sd_handle_write_request(uint16_t channel_id, char* data, uint16_t size) {
    if (size != 9) {
        return KELP_PROTOCOL;
    }
    int8_t sd_id;
    sd_card_t* sd_card_p = get_sd_from_request(data, &sd_id);
    if (!sd_card_p) {
        return KELP_INVALID_ID;
    }

    uint32_t start = get_uint32_be(&data[1]);
    uint32_t count = get_uint32_be(&data[5]);
    uint32_t buffer_size = count * 512;

    // Ensure stack is large enough before allocating the VLA
    kelp_error_t error = task_stack_fit_buffer(SD_CARD_STACK_HEADROOM, buffer_size, false);
    KELP_RETURN_ON_ERROR(error);

    uint8_t buffer[buffer_size];
    uint32_t received_buffer_size = 0;
    uint16_t reason;
    error = com_get_char_array_blocking(channel_id, buffer, buffer_size, &received_buffer_size, &reason);
    KELP_RETURN_ON_ERROR(error);
    if (reason != REASON_BLOCK_WRITE_BYTES) {
        return KELP_WRONG_REASON;
    }
    if (buffer_size != received_buffer_size) {
        return KELP_PROTOCOL;
    }

    block_dev_err_t rc = sd_card_p->write_blocks(sd_card_p, buffer, start, count);
    if (rc != SD_BLOCK_DEVICE_ERROR_NONE) {
        return KELP_IO;
    }

    int64_t response = buffer_size;
    error = com_send_int64_blocking(channel_id, response, REASON_BLOCK_WRITE_BYTES);
    KELP_RETURN_ON_ERROR(error);

    return KELP_OK;
}

void kelp_task_sd_card_driver(uint32_t pid, uint32_t* signals, char* args) {
    task_request_stack(SD_CARD_STACK_HEADROOM);

    uint32_t l = 0;

    // initialize id mappings
    for (uint8_t id = 0; id < BLOCK_SERVICE_MAX_DEVICES; id++) {
        device_id_2_sd_id[id] = -1;
    }

    // set all sd cards to unmounted
    for (uint8_t id = 0; id < SD_CARD_MAX_DEVICES; id++) {
        sd_driver_details[id].initialized = false;
    }

    // initialize sd card driver
    bool driver_initialised = false;

    for (uint32_t attempt = 0; attempt < 10; attempt++) {
        if (sd_init_driver()) {
            driver_initialised = true;
            break;
        }
        task_sleep_ms(10);
    }

    if (!driver_initialised) {
        return;
    }

    /*
     * main driver loop:
     * 1. loop through drives and check if they are connected
     *      a. if so initialize them
     * 2. check for channels much like a service
     *      b. send responses
     */

    while (true) {
        if (l >= 100) {
            // check signals
            if (*signals & TASK_SIGTERM) {
                return;
            }

            l = 0;
        }

        for (uint8_t id = 0; id < SD_CARD_MAX_DEVICES; id++) {
            sd_card_t* sd_card_p = sd_get_by_num(id);
            struct sd_driver_details_t* details = &sd_driver_details[id];
            if (!sd_card_p) {
                return;
            }

            if (sd_card_detect(sd_card_p) && !details->initialized) {
                if (sd_card_p->init(sd_card_p)) {
                    uint8_t device_id;
                    kelp_error_t error = kelp_block_mount_device(&device_id, 512, sd_card_p->get_num_sectors(sd_card_p));

                    if (error == KELP_OK) {
                        device_id_2_sd_id[device_id] = id;
                        details->initialized = true;
                        details->block_count = sd_card_p->get_num_sectors(sd_card_p);
                        details->size = details->block_count * 512;
                    }
                }
            }
        }

        // get connected channels
        uint16_t channel_ids[NUM_CHANNELS];
        uint16_t num_connected = 0;
        kelp_error_t error = get_connected_channels(channel_ids, &num_connected, NUM_CHANNELS);

        if (error != KELP_OK) {
            task_sleep_ms(5);
            continue;
        }

        // check for messages
        for (uint32_t c = 0; c < num_connected; c++) {
            uint16_t channel_id = channel_ids[c];

            // can we read from this channel?
            if (is_channel_ready_to_read(channel_id)) {
                uint8_t content_type = 0;
                error = com_channel_peek(channel_id, &content_type);
                if (error != KELP_OK) {
                    continue;
                }

                // does this channel contain an array?
                if (content_type == COM_TYPE_ARRAY) {
                    // get the reason
                    uint16_t reason = 0;

                    char data[CHANNEL_SIZE];
                    uint16_t size;

                    error = com_get_char_array_fast(channel_id, &data, &size, &reason);
                    if (error != KELP_OK) {
                        continue;
                    }

                    // handle block device operations
                    switch (reason) {
                    case REASON_BLOCK_READ_BYTES:
                        // send the requested bytes
                        error = kelp_sd_handle_read_request(channel_id, data, size);
                        if (error != KELP_OK) {
                            com_send_int32_blocking(channel_id, error, REASON_BLOCK_ERROR);
                        }
                        break;
                    case REASON_BLOCK_WRITE_BYTES:
                        // write the provided bytes
                        error = kelp_sd_handle_write_request(channel_id, data, size);
                        if (error != KELP_OK) {
                            com_send_int32_blocking(channel_id, error, REASON_BLOCK_ERROR);
                        }
                        break;
                    default: continue;
                    }
                }
            }
        }

        l++;

        task_sleep_us(250);
    }
}
