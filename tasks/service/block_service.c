//
// Created by wolfboy on 5/1/2026.
//

#include "block_service.h"

#include <string.h>

#include "channel.h"
#include "com_channel_protocol.h"
#include "scheduler.h"
#include "error_codes.h"

static struct block_device_t block_devices[BLOCK_SERVICE_MAX_DEVICES];

uint32_t kelp_block_get_driver_pid(uint8_t device_id) {
    // send request
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(BLOCK_SERVICE_PID, true, &channel_id);
    if (error != KELP_OK) {
        return 0;
    }

    // send request to service
    error = com_send_uint32_blocking(channel_id, device_id, REASON_BLOCK_GET_DRIVER_PID);
    if (error != KELP_OK) {
        return 0;
    }

    // get response
    uint32_t response;
    uint16_t reason;
    error = com_get_uint32_blocking(channel_id, &response, &reason);
    if (error != KELP_OK) {
        return 0;
    }

    if (reason != REASON_BLOCK_GET_DRIVER_PID) {
        return 0;
    }

    return response;
}

kelp_error_t kelp_block_mount_device(uint8_t* device_id, uint32_t block_size, uint32_t block_count) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(BLOCK_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    char data[8] = {
        block_size >> 24,
        block_size >> 16,
        block_size >> 8,
        block_size,
        block_count >> 24,
        block_count >> 16,
        block_count >> 8,
        block_count
    };

    error = com_send_char_array_fast_blocking(channel_id, data, 8, REASON_BLOCK_MOUNT);
    KELP_RETURN_ON_ERROR(error);

    // get confirmation

    int32_t return_code;
    uint16_t reason;

    error = com_get_int32_blocking(channel_id, &return_code, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason == REASON_BLOCK_ERROR) {
          return (kelp_error_t)return_code; // enums are just ints and can be sent through the channels as such
    }

    if (reason != REASON_BLOCK_MOUNT) {
        return KELP_WRONG_REASON;
    }

    *device_id = (uint8_t)return_code;

    return KELP_OK;
}

kelp_error_t kelp_block_unmount_device(uint8_t device_id) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(BLOCK_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, device_id, REASON_BLOCK_UNMOUNT);
    KELP_RETURN_ON_ERROR(error);

    return KELP_OK;
}

kelp_error_t kelp_block_read_bytes(uint8_t device_id, uint8_t* buffer, uint32_t buffer_size, uint32_t *bytes_read, uint32_t start, uint32_t count) {
    uint32_t driver_pid = kelp_block_get_driver_pid(device_id);
    if (driver_pid == 0) {
        driver_pid = BLOCK_SERVICE_PID;
    }

    // send request
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(driver_pid, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    char packet[9];

    packet[0] = device_id;
    packet[1] = start;
    packet[2] = start >> 8;
    packet[3] = start >> 16;
    packet[4] = start >> 24;
    packet[5] = count;
    packet[6] = count >> 8;
    packet[7] = count >> 16;
    packet[8] = count >> 24;

    error = com_send_char_array_fast_blocking(channel_id, packet, 9, REASON_BLOCK_READ_BYTES);
    KELP_RETURN_ON_ERROR(error);

    // get result
    uint16_t reason;
    uint8_t type;
    com_channel_wait_until_readable(channel_id);
    error = com_channel_peek(channel_id, &type);
    KELP_RETURN_ON_ERROR(error);

    if (type == COM_TYPE_INT32) {
        // there was an error
        int32_t block_service_error; // again, this is a kelp error code, but in its RAW INTEGER FORM >:)
        error = com_get_int32_blocking(channel_id, &block_service_error, &reason);
        KELP_RETURN_ON_ERROR(error);
        if (reason != REASON_BLOCK_ERROR) {
            return KELP_WRONG_REASON;
        }
        return block_service_error;
    }

    // get array containing the memory data (I hope)
    error = com_get_char_array_blocking(channel_id, buffer, buffer_size, bytes_read, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason != REASON_BLOCK_READ_BYTES) {
        return KELP_WRONG_REASON;
    }

    return KELP_OK;
}

kelp_error_t kelp_block_write_bytes(uint8_t device_id, uint8_t* buffer, uint32_t buffer_size, uint32_t *bytes_written, uint32_t start, uint32_t count) {
    uint32_t driver_pid = kelp_block_get_driver_pid(device_id);
    if (driver_pid == 0) {
        driver_pid = BLOCK_SERVICE_PID;
    }

    // send request
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(driver_pid, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    char packet[9];

    packet[0] = device_id;
    packet[1] = start;
    packet[2] = start >> 8;
    packet[3] = start >> 16;
    packet[4] = start >> 24;
    packet[5] = count;
    packet[6] = count >> 8;
    packet[7] = count >> 16;
    packet[8] = count >> 24;

    error = com_send_char_array_fast_blocking(channel_id, packet, 9, REASON_BLOCK_WRITE_BYTES);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_char_array_blocking(channel_id, buffer, buffer_size, REASON_BLOCK_WRITE_BYTES);
    KELP_RETURN_ON_ERROR(error);

    // get result
    uint16_t reason;
    uint8_t type;
    com_channel_wait_until_readable(channel_id);
    error = com_channel_peek(channel_id, &type);
    KELP_RETURN_ON_ERROR(error);

    if (type == COM_TYPE_INT32) {
        // there was an error
        int32_t block_service_error; // again, this is a kelp error code, but in its RAW INTEGER FORM >:)
        error = com_get_int32_blocking(channel_id, &block_service_error, &reason);
        KELP_RETURN_ON_ERROR(error);
        if (reason != REASON_BLOCK_ERROR) {
            return KELP_WRONG_REASON;
        }
        return block_service_error;
    }

    int64_t response;
    error = com_get_int64_blocking(channel_id, &response, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason != REASON_BLOCK_WRITE_BYTES) {
        return KELP_WRONG_REASON;
    }

    if (response >= 0) {
        if (bytes_written != NULL) {
            *bytes_written = (uint32_t)response;
        }
    } else {
        return (int32_t)response;
    }

    return KELP_OK;
}

kelp_error_t kelp_block_reset_device(struct block_device_t* driver) {
    driver->driver_pid = 0;
    driver->block_size = 0;
    driver->block_count = 0;
    driver->size = 0;
    return KELP_OK;
}

kelp_error_t kelp_block_add_device(uint8_t* device_id, uint32_t pid, uint32_t block_size, uint32_t block_count) {
    for (uint8_t bd = 0; bd < BLOCK_SERVICE_MAX_DEVICES; bd++) {
        struct block_device_t* device = &block_devices[bd];
        if (device->driver_pid == 0) {
            device->driver_pid = pid;
            device->block_size = block_size;
            device->block_count = block_count;
            device->size = block_count * block_size;
            *device_id = bd;
            return KELP_OK;
        }
    }
    return KELP_NONE_FREE;
}

kelp_error_t kelp_block_remove_device(uint32_t pid, uint8_t device_id) {
    struct block_device_t* driver = &block_devices[device_id];
    if (driver->driver_pid == pid) {
        kelp_block_reset_device(driver);
        return KELP_OK;
    }
    return KELP_NO_TASK;
}

kelp_error_t kelp_block_handle_mount_request(uint16_t channel_id, char data[CHANNEL_SIZE], uint16_t size) {
    if (size != 8) {
        return KELP_PROTOCOL;
    } else {
        uint32_t block_size = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
        uint32_t block_count = data[4] << 24 | data[5] << 16 | data[6] << 8 | data[7];

        uint8_t device_id = 0;
        kelp_error_t error = kelp_block_add_device(&device_id, get_channel_partner_pid(channel_id), block_size, block_count);
        KELP_RETURN_ON_ERROR(error);

        com_send_int32_blocking(channel_id, device_id, REASON_BLOCK_MOUNT);
        return KELP_OK;
    }
}

kelp_error_t kelp_block_handle_read_request(uint16_t channel_id, char data[CHANNEL_SIZE], uint16_t size) {
    if (size != 9) {
        return KELP_PROTOCOL;
    } else {
        uint8_t device_id = data[0];
        struct block_device_t block_driver = block_devices[device_id];

        uint32_t count = data[5] | data[6] << 8 | data[7] << 16 | data[8] << 24;    // number of requested blocks
        uint32_t return_buffer_size = count * block_driver.block_size ;             // the size of the return buffer in bytes

        uint32_t driver_pid = block_devices[device_id].driver_pid;                  // pid of the driver task

        // send data down to driver
        // get channel
        uint16_t driver_channel_id = 0;
        kelp_error_t error = com_channel_request_blocking(driver_pid, true, &driver_channel_id);
        KELP_RETURN_ON_ERROR(error);

        // send data
        error = com_send_char_array_fast_blocking(driver_channel_id, data, 9, REASON_BLOCK_READ_BYTES);
        KELP_RETURN_ON_ERROR(error);

        // get response
        uint8_t buffer[return_buffer_size];
        uint32_t num_bytes_read = 0;
        uint16_t reason;
        error = com_get_char_array_blocking(driver_channel_id, buffer, return_buffer_size, &num_bytes_read, &reason);
        if (reason != REASON_BLOCK_READ_BYTES) {
            return KELP_WRONG_REASON;
        }
        KELP_RETURN_ON_ERROR(error);

        // send response to task
        error = com_send_char_array_blocking(channel_id, buffer, num_bytes_read, REASON_BLOCK_READ_BYTES);
        KELP_RETURN_ON_ERROR(error);
    }
    return KELP_OK;
}

kelp_error_t kelp_block_handle_write_request(uint16_t channel_id, char* data, uint16_t size) {
    if (size != 9) {
        return KELP_PROTOCOL;
    } else {
        uint8_t device_id = data[0];
        struct block_device_t block_driver = block_devices[device_id];

        uint32_t count = data[5] | data[6] << 8 | data[7] << 16 | data[8] << 24;    // number of provided blocks
        uint32_t buffer_size = count * block_driver.block_size;                     // the size of the return buffer in bytes

        uint32_t driver_pid = block_devices[device_id].driver_pid;                  // pid of the driver task

        // get channel
        uint16_t driver_channel_id = 0;
        kelp_error_t error = com_channel_request_blocking(driver_pid, true, &driver_channel_id);
        KELP_RETURN_ON_ERROR(error);

        // get buffer to write
        uint8_t buffer[buffer_size];
        uint32_t received_buffer_size = 0;
        uint16_t reason;
        error = com_get_char_array_blocking(channel_id, buffer, buffer_size, &received_buffer_size, &reason);
        KELP_RETURN_ON_ERROR(error);
        if (reason != REASON_BLOCK_WRITE_BYTES) {
            return KELP_WRONG_REASON;
        }
        if (buffer_size != received_buffer_size) {
            return KELP_PROTOCOL; // the buffer must be big enough to fill the requested blocks
        }

        // send data characteristics
        error = com_send_char_array_blocking(driver_channel_id, data, size, REASON_BLOCK_WRITE_BYTES);
        KELP_RETURN_ON_ERROR(error);

        // send data for writing
        error = com_send_char_array_blocking(driver_channel_id, buffer, buffer_size, REASON_BLOCK_WRITE_BYTES);
        KELP_RETURN_ON_ERROR(error);

        // get result
        uint8_t type;
        com_channel_wait_until_readable(channel_id);
        error = com_channel_peek(channel_id, &type);
        KELP_RETURN_ON_ERROR(error);

        if (type == COM_TYPE_INT32) {
            // there was an error
            int32_t block_service_error; // again, this is a kelp error code, but in its RAW INTEGER FORM >:)
            error = com_get_int32_blocking(channel_id, &block_service_error, &reason);
            KELP_RETURN_ON_ERROR(error);
            if (reason != REASON_BLOCK_ERROR) {
                return KELP_WRONG_REASON;
            }
            return block_service_error;
        }

        // get response
        int64_t response;
        error = com_get_int64_blocking(driver_channel_id, &response, &reason);
        if (reason != REASON_BLOCK_WRITE_BYTES) {
            return KELP_WRONG_REASON;
        }
        KELP_RETURN_ON_ERROR(error);

        // send response to task
        error = com_send_int64_blocking(channel_id, response, REASON_BLOCK_WRITE_BYTES);
        KELP_RETURN_ON_ERROR(error);
    }
    return KELP_OK;
}

void kelp_task_block_service(uint32_t pid, uint32_t* signals, char* args) {
    task_request_stack(2048);

    uint32_t l = 0;

    for (uint32_t i = 0; i < BLOCK_SERVICE_MAX_DEVICES; i++) {
        struct block_device_t* device = &block_devices[i];
        kelp_block_reset_device(device);
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
        for (uint32_t c = 0; c < num_connected; c++) {
            uint16_t channel_id = channel_ids[c];

            // can we read from this channel?
            if (is_channel_ready_to_read(channel_id)) {
                uint8_t content_type = 0;
                error = com_channel_peek(channel_id, &content_type);
                if (error != KELP_OK) {
                    continue;
                }

                switch (content_type) {

                case COM_TYPE_ARRAY: {
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
                    case REASON_BLOCK_MOUNT:
                        // attempt to mount it
                        error = kelp_block_handle_mount_request(channel_id, data, size);
                        if (error != KELP_OK) {
                            com_send_int32_blocking(channel_id, error, REASON_BLOCK_ERROR);
                        }
                        break;
                    case REASON_BLOCK_READ_BYTES:
                        // send the requested bytes
                        error = kelp_block_handle_read_request(channel_id, data, size);
                        if (error != KELP_OK) {
                            com_send_int32_blocking(channel_id, error, REASON_BLOCK_ERROR);
                        }
                        break;
                    case REASON_BLOCK_WRITE_BYTES:
                        // send the requested bytes
                        error = kelp_block_handle_write_request(channel_id, data, size);
                        if (error != KELP_OK) {
                            com_send_int32_blocking(channel_id, error, REASON_BLOCK_ERROR);
                        }
                        break;
                    default: break;
                    }
                } break;

                case COM_TYPE_UINT32: {
                    // get the reason
                    uint16_t reason = 0;

                    uint32_t data;

                    error = com_get_uint32(channel_id, &data, &reason);
                    if (error != KELP_OK) {
                        continue;
                    }

                    // do they want to get the driver pid?
                    switch (reason) {
                    case REASON_BLOCK_GET_DRIVER_PID:
                        // get device id from channel
                        struct block_device_t* device = &block_devices[data];

                        uint32_t driver_pid = 0;
                        if (device->block_count > 0) {
                            driver_pid = device->driver_pid;
                        }

                        com_send_uint32_blocking(channel_id, driver_pid, REASON_BLOCK_GET_DRIVER_PID);
                        break;
                    default: break;
                    }
                } break;
                }
            }
        }

        l++;

        task_sleep_us(250);
    }
}