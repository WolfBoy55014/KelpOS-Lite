//
// Created by wolfboy on 5/1/2026.
//

#include "block_service.h"

#include "channel.h"
#include "com_channel_protocol.h"
#include "scheduler.h"

static struct block_driver_t block_drivers[BLOCK_SERVICE_NUM_DEVICES];

kelp_error_t kelp_block_mount_device() {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(BLOCK_SERVICE_PID, true, &channel_id);
    if (error != KELP_OK) {
        return error;
    }

    error = com_send_request_blocking(channel_id, REASON_BLOCK_MOUNT);
    if (error != KELP_OK) {
        return error;
    }

    // get confirmation

    int32_t return_code;
    uint16_t reason;

    error = com_get_int32_blocking(channel_id, &return_code, &reason);
    if (error != KELP_OK) {
        return error;
    }

    if (reason != REASON_BLOCK_MOUNT) {
        return KELP_PROTOCOL;
    }

    if (return_code == -1) {
        return KELP_NONE_FREE;
    }

    return KELP_OK;
}

kelp_error_t kelp_block_unmount_device() {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(BLOCK_SERVICE_PID, true, &channel_id);
    if (error != KELP_OK) {
        return error;
    }

    error = com_send_request_blocking(channel_id, REASON_BLOCK_UNMOUNT);
    if (error != KELP_OK) {
        return error;
    }

    return KELP_OK;
}

kelp_error_t kelp_block_read_bytes(uint8_t device_id, uint8_t* buffer, uint32_t *size, uint32_t address, uint32_t count) {
    // send request

    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(BLOCK_SERVICE_PID, true, &channel_id);
    if (error != KELP_OK) {
        return error;
    }

    char packet[9];

    packet[0] = device_id;
    packet[1] = address;
    packet[2] = address >> 8;
    packet[3] = address >> 16;
    packet[4] = address >> 24;
    packet[5] = count;
    packet[6] = count >> 8;
    packet[7] = count >> 16;
    packet[8] = count >> 24;

    error = com_send_char_array_fast_blocking(channel_id, packet, 9, REASON_BLOCK_READ_BYTES);
    if (error != KELP_OK) {
        return error;
    }

    // get result

    uint16_t reason;

    error = com_get_char_array_blocking(channel_id, buffer, count, size, &reason);
    if (error != KELP_OK) {
        return error;
    }

    if (reason != REASON_BLOCK_READ_BYTES) {
        return KELP_PROTOCOL;
    }

    return KELP_OK;
}

kelp_error_t kelp_block_clear_driver(struct block_driver_t* driver) {
    driver->pid = 0;
    driver->block_size = 0;
    driver->block_count = 0;
    driver->size = 0;
    return KELP_OK;
}

kelp_error_t kelp_block_mount(uint32_t pid) {
    for (uint8_t bd = 0; bd < BLOCK_SERVICE_NUM_DEVICES; bd++) {
        struct block_driver_t* driver = &block_drivers[bd];
        if (driver->pid == 0) {
            driver->pid = pid;
            return KELP_OK;
        }
    }
    return KELP_NONE_FREE;
}

kelp_error_t kelp_block_unmount(uint32_t pid) {
    for (uint8_t bd = 0; bd < BLOCK_SERVICE_NUM_DEVICES; bd++) {
        struct block_driver_t* driver = &block_drivers[bd];
        if (driver->pid == pid) {
            kelp_block_clear_driver(driver);
            return KELP_OK;
        }
    }
    return KELP_NO_TASK;
}

void kelp_task_block_service(uint32_t pid, uint32_t* signals, char* args) {

    uint32_t l = 0;

    for (uint32_t i = 0; i < BLOCK_SERVICE_NUM_DEVICES; i++) {
        struct block_driver_t* driver = &block_drivers[i];
        kelp_block_clear_driver(driver);
    }

    while (1) {
        if (l >= 100) {
            // check signals
            if (*signals & TASK_SIGTERM) {
                return;
            }

            l = 0;
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
        for (int c = 0; c < num_connected; c++) {
            uint16_t channel_id = channel_ids[c];

            // can we read from this channel?
            if (is_channel_ready_to_read(channel_id)) {
                uint8_t content_type = 0;
                error = com_channel_peek(channel_id, &content_type);
                if (error != KELP_OK) {
                    continue;
                }

                // does this channel contain a request?
                if (content_type == COM_TYPE_REQ) {
                    // get the reason
                    uint16_t reason = 0;

                    error = com_get_request(channel_id, &reason);
                    if (error != KELP_OK) {
                        continue;
                    }

                    // do they want to mount a device?
                    if (reason == REASON_BLOCK_MOUNT) {
                        error = kelp_block_mount(get_channel_partner_pid(channel_id));

                        // send confirmation of mounting
                        int32_t return_code = 0;
                        if (error == KELP_NONE_FREE) {
                            return_code = -1;
                        }

                        error = com_send_int32_blocking(channel_id, return_code, REASON_BLOCK_MOUNT);
                        if (error != KELP_OK) {
                            continue;
                        }
                    }

                    // do they want to unmount a device?
                    else if (reason == REASON_BLOCK_UNMOUNT) {
                        kelp_block_unmount(get_channel_partner_pid(channel_id));
                    }
                }
            }
        }

        l++;

        task_yield();
    }
}