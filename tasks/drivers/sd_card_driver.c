//
// Created by wolfboy on 6/7/2026.
//

#include "sd_card_driver.h"

#include <stdlib.h>

#include "block_service.h"
#include "channel.h"
#include "com_channel_protocol.h"
#include "hw_config.h"
#include "scheduler.h"
#include "sd_card.h"
#include "kernel_config.h"
#include "error_codes.h"

static int16_t device_id_2_sd_id[BLOCK_SERVICE_MAX_DEVICES];
static struct sd_driver_details_t sd_driver_details[SD_CARD_MAX_DEVICES];

static int16_t sd_id_2_device_id(uint8_t sd_id) {
    for (uint8_t device_id = 0; device_id < BLOCK_SERVICE_MAX_DEVICES; device_id++) {
        uint8_t temp_sd_id = device_id_2_sd_id[device_id];
        if (temp_sd_id == sd_id) {
            return device_id;
        }
    }
    return -1;
}

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
static sd_card_t* get_sd_from_request(const char* data, uint8_t* out_sd_id) {
    uint8_t device_id = data[0];
    if (device_id >= BLOCK_SERVICE_MAX_DEVICES) {
        return NULL;
    }
    int16_t sd_id = device_id_2_sd_id[device_id];
    if (sd_id < 0 || sd_id >= SD_CARD_MAX_DEVICES) {
        return NULL;
    }
    *out_sd_id = (uint8_t)sd_id;
    return sd_get_by_num(sd_id);
}

// Extract a 32-bit big-endian value from a buffer
static inline uint32_t get_uint32_be(const char* data) {
    return ((uint8_t)data[0] << 24) | ((uint8_t)data[1] << 16) |
           ((uint8_t)data[2] << 8)  |  (uint8_t)data[3];
}

static kelp_error_t kelp_sd_handle_read_request(uint16_t channel_id, char data[CHANNEL_SIZE], uint16_t size) {
    if (size != 9) {
        return KELP_PROTOCOL;
    }
    uint8_t sd_id;
    sd_card_t* sd_card_p = get_sd_from_request(data, &sd_id);
    if (!sd_card_p) {
        return KELP_INVALID_ID;
    }

    uint32_t start = get_uint32_be(&data[1]);
    uint32_t count = get_uint32_be(&data[5]);
    count = sd_bound_count(sd_id, start, count);
    if (count == 0) {
        return KELP_TOO_BIG;
    }
    uint32_t return_buffer_size = count * 512;

    uint8_t* buffer = malloc(return_buffer_size);
    if (!buffer) {
        return KELP_MEMORY;
    }

    block_dev_err_t rc = sd_card_p->read_blocks(sd_card_p, buffer, start, count);
    if (rc != SD_BLOCK_DEVICE_ERROR_NONE) {
        free(buffer);
        return KELP_IO;
    }

    kelp_error_t error = com_send_char_array_blocking(channel_id, buffer, return_buffer_size, REASON_BLOCK_READ_BYTES);
    free(buffer);
    return error;
}

static kelp_error_t kelp_sd_handle_write_request(uint16_t channel_id, char* data, uint16_t size) {
    if (size != 9) {
        return KELP_PROTOCOL;
    }
    uint8_t sd_id;
    sd_card_t* sd_card_p = get_sd_from_request(data, &sd_id);
    if (!sd_card_p) {
        return KELP_INVALID_ID;
    }

    uint32_t start = get_uint32_be(&data[1]);
    uint32_t count = get_uint32_be(&data[5]);
    count = sd_bound_count(sd_id, start, count);
    if (count == 0) {
        return KELP_TOO_BIG;
    }
    uint32_t buffer_size = count * 512;

    uint8_t* buffer = malloc(buffer_size);
    if (!buffer) {
        return KELP_MEMORY;
    }

    uint32_t received_buffer_size = 0;
    uint16_t reason;
    kelp_error_t error = com_get_char_array_blocking(channel_id, buffer, buffer_size, &received_buffer_size, &reason);
    if (error != KELP_OK) {
        free(buffer);
        return error;
    }
    if (reason != REASON_BLOCK_WRITE_BYTES) {
        free(buffer);
        return KELP_WRONG_REASON;
    }
    if (buffer_size != received_buffer_size) {
        free(buffer);
        return KELP_PROTOCOL;
    }

    block_dev_err_t rc = sd_card_p->write_blocks(sd_card_p, buffer, start, count);
    if (rc != SD_BLOCK_DEVICE_ERROR_NONE) {
        free(buffer);
        return KELP_IO;
    }

    error = com_send_uint32_blocking(channel_id, buffer_size, REASON_BLOCK_WRITE_BYTES);
    free(buffer);
    return error;
}

void kelp_task_sd_card_driver(uint32_t pid, uint32_t* signals, char* args) {
    task_request_stack(1024);

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
     *      b. if not, unmount
     * 2. check for channels much like a service
     *      b. send responses
     */

    while (true) {
        if (l >= 100) {
            // check signals
            if (*signals & TASK_SIGTERM) {
                return;
            }

            for (uint8_t id = 0; id < SD_CARD_MAX_DEVICES; id++) {
                sd_card_t* sd_card_p = sd_get_by_num(id);
                struct sd_driver_details_t* details = &sd_driver_details[id];
                if (!sd_card_p) {
                    return;
                }

                // check newly inserted drives
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

                // check newly removed drives
                if (!sd_card_detect(sd_card_p) && details->initialized) {

                    int16_t device_id = sd_id_2_device_id(id);

                    if (device_id == -1) {
                        continue;
                    }

                    kelp_error_t error = kelp_block_unmount_device(device_id);

                    if (error == KELP_OK) {
                        device_id_2_sd_id[device_id] = -1;
                        details->initialized = false;
                        details->block_count = 0;
                        details->size = 0;
                    }
                }
            }

            l = 0;
        }

        // get connected channels
        uint16_t channel_ids[NUM_CHANNELS];
        uint16_t num_connected = 0;
        kelp_error_t error = get_connected_channels(channel_ids, &num_connected, NUM_CHANNELS);

        if (error != KELP_OK) {
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
