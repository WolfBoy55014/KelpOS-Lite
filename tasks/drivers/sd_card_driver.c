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

static int16_t device_id_2_sd_id[BLOCK_SERVICE_MAX_DEVICES];
static struct sd_driver_details_t sd_driver_details[SD_CARD_MAX_DEVICES];

uint32_t sd_bound_count(uint8_t sd_id, uint32_t start, uint32_t count) {
    struct sd_driver_details_t* details = &sd_driver_details[sd_id];

    uint32_t end = start + count;
    if (end <= details->block_count) {
        return count;
    } else {
        return count - (end - details->block_count);
    }
}

kelp_error_t kelp_sd_handle_read_request(uint16_t channel_id, char data[CHANNEL_SIZE], uint16_t size) {
    if (size != 9) {
        return KELP_PROTOCOL;
    } else {
        uint8_t device_id = data[0];
        uint8_t sd_id = device_id_2_sd_id[device_id];
        sd_card_t* sd_card_p = sd_get_by_num(sd_id);

        uint32_t start = data[1] | data[2] << 8 | data[3] << 16 | data[4] << 24;    // starting block
        uint32_t count = data[5] | data[6] << 8 | data[7] << 16 | data[8] << 24;    // number of requested blocks
        count = sd_bound_count(sd_id, start, count);                                // confine count to size of sd card
        uint32_t return_buffer_size = count * 512;                                  // the size of the return buffer in bytes

        // get data from sd card
        uint8_t buffer[return_buffer_size];

        block_dev_err_t rc = sd_card_p->read_blocks(sd_card_p, buffer, start, count);

        if (rc != SD_BLOCK_DEVICE_ERROR_NONE) {
            // printf("Error reading sector %lu\n", start);
            return KELP_IO;
        }

        // send response to task
        kelp_error_t error = com_send_char_array_blocking(channel_id, buffer, return_buffer_size, REASON_BLOCK_READ_BYTES);
        KELP_RETURN_ON_ERROR(error);
    }
    return KELP_OK;
}

void kelp_task_sd_card_driver(uint32_t pid, uint32_t* signals, char* args) {

    task_request_stack(2048);

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

                    // do they want to mount a device?
                    switch (reason) {
                    case REASON_BLOCK_READ_BYTES:
                        // send the requested bytes
                        error = kelp_sd_handle_read_request(channel_id, data, size);
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

    // uint8_t drive_id = 0;
    // sd_card_t *sd_card_p = sd_get_by_num(drive_id);
    //
    // if (!sd_card_p) {
    //     return;
    // }
    //
    // while (!sd_card_detect(sd_card_p)) {
    //     task_sleep_ms(1000);
    // }
    //
    // if (!sd_card_p->init(sd_card_p)) {
    //     return;
    // }
    //
    // const uint32_t num_sectors = sd_card_p->get_num_sectors(sd_card_p);
    //
    // printf("\nNumber of sectors on card: %lu\n", num_sectors);
    // printf("Starting Speed Test\n");
    //
    // uint32_t sector = 0;
    // uint32_t duration = 10;
    // uint32_t start = time_us_32();
    // uint32_t end = start + duration * 1000000;
    // for (sector = 0; sector < num_sectors; sector += 8) {
    //     uint8_t buffer[4096];
    //     block_dev_err_t rc = sd_card_p->read_blocks(sd_card_p, buffer, sector, 8);
    //
    //     if (rc != SD_BLOCK_DEVICE_ERROR_NONE) {
    //         printf("Error reading sector %lu\n", sector);
    //         return;
    //     }
    //
    //     if (time_us_32() > end) {
    //         break;
    //     }
    // }
    //
    // uint32_t bytes_read = sector * 512;
    // double bytes_per_sec = (double) bytes_read / (double) duration;
    //
    // printf("Read %lu sectors\n", sector);
    // printf("Bytes per second: %f\n", bytes_per_sec);
}
