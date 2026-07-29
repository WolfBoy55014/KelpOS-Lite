//
// Created by wolfboy on 5/1/2026.
//

#include "block_service.h"

#include <string.h>
#include <stdlib.h>

#include "channel.h"
#include "com_channel_protocol.h"
#include "scheduler.h"
#include "error_codes.h"
#include "file_service.h"
#include "kernel_config.h"
#include "hardware/gpio.h"

static struct block_device_t block_devices[BLOCK_SERVICE_MAX_DEVICES];

// Extract a 32-bit big-endian value from a buffer
static inline uint32_t get_uint32_be(const char* data) {
    return ((uint8_t)data[0] << 24) | ((uint8_t)data[1] << 16) |
           ((uint8_t)data[2] << 8)  |  (uint8_t)data[3];
}

// Pack a 32-bit value into a buffer in big-endian
static inline void put_uint32_be(char* data, uint32_t val) {
    data[0] = val >> 24;
    data[1] = val >> 16;
    data[2] = val >> 8;
    data[3] = val;
}

// Build the 9-byte block request packet (device_id + start + count)
static inline void build_block_request_packet(char packet[9], uint8_t device_id, uint32_t start, uint32_t count) {
    packet[0] = device_id;
    put_uint32_be(&packet[1], start);
    put_uint32_be(&packet[5], count);
}

// Check the device_id is in bounds
static inline bool is_valid_device_id(uint8_t device_id) {
    if (device_id >= BLOCK_SERVICE_MAX_DEVICES) {
        return false;
    }
    if (block_devices[device_id].driver_pid == 0) {
        return false;
    }
    return true;
}

static uint32_t kelp_block_get_driver_pid(uint8_t device_id) {
    BLOCK_SERVICE_ACTIVITY_LED_ON;
    // send request
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(BLOCK_SERVICE_PID, true, &channel_id);
    if (error != KELP_OK) {
        BLOCK_SERVICE_ACTIVITY_LED_OFF;
        return 0;
    }

    // send request to service
    error = com_send_uint32_blocking(channel_id, device_id, REASON_BLOCK_GET_DRIVER_PID);
    if (error != KELP_OK) {
        BLOCK_SERVICE_ACTIVITY_LED_OFF;
        return 0;
    }

    // check for service error
    kelp_error_t service_error;
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (service_error != KELP_OK || error != KELP_OK) {
        BLOCK_SERVICE_ACTIVITY_LED_OFF;
        return 0;
    }

    // get response
    uint32_t response;
    uint16_t reason;
    error = com_get_uint32_blocking(channel_id, &response, &reason);
    if (error != KELP_OK) {
        BLOCK_SERVICE_ACTIVITY_LED_OFF;
        return 0;
    }

    if (reason != REASON_BLOCK_GET_DRIVER_PID) {
        BLOCK_SERVICE_ACTIVITY_LED_OFF;
        return 0;
    }

    // check for service error
    error = com_check_for_error_blocking(channel_id, &service_error);
    if (service_error != KELP_OK || error != KELP_OK) {
        BLOCK_SERVICE_ACTIVITY_LED_OFF;
        return 0;
    }

    BLOCK_SERVICE_ACTIVITY_LED_OFF;
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

    kelp_error_t service_error;
    KELP_BLOCK_CHECK_FOR_SERVICE_ERROR;

    // get confirmation

    uint32_t return_code;
    uint16_t reason;

    error = com_get_uint32_blocking(channel_id, &return_code, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason != REASON_BLOCK_MOUNT) {
        return KELP_WRONG_REASON;
    }

    *device_id = (uint8_t)return_code;

    KELP_BLOCK_CHECK_FOR_SERVICE_ERROR;

    return KELP_OK;
}

kelp_error_t kelp_block_unmount_device(uint8_t device_id) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(BLOCK_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, device_id, REASON_BLOCK_UNMOUNT);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    KELP_BLOCK_CHECK_FOR_SERVICE_ERROR;

    return KELP_OK;
}

kelp_error_t kelp_block_get_block_size(uint8_t device_id, uint32_t* block_size) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(BLOCK_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, device_id, REASON_BLOCK_SIZE);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    KELP_BLOCK_CHECK_FOR_SERVICE_ERROR;

    uint16_t reason;
    error = com_get_uint32(channel_id, block_size, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason != REASON_BLOCK_SIZE) {
        return KELP_WRONG_REASON;
    }

    KELP_BLOCK_CHECK_FOR_SERVICE_ERROR;

    return KELP_OK;
}

kelp_error_t kelp_block_get_block_count(uint8_t device_id, uint32_t* block_count) {
    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(BLOCK_SERVICE_PID, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_uint32_blocking(channel_id, device_id, REASON_BLOCK_COUNT);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    KELP_BLOCK_CHECK_FOR_SERVICE_ERROR;

    uint16_t reason;
    error = com_get_uint32(channel_id, block_count, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason != REASON_BLOCK_COUNT) {
        return KELP_WRONG_REASON;
    }

    KELP_BLOCK_CHECK_FOR_SERVICE_ERROR;

    return KELP_OK;
}

kelp_error_t kelp_block_read_bytes(uint8_t device_id, uint8_t* buffer, uint32_t buffer_size, uint32_t *bytes_read, uint32_t start, uint32_t count) {
    uint32_t driver_pid = kelp_block_get_driver_pid(device_id);
    if (driver_pid == 0) {
        driver_pid = BLOCK_SERVICE_PID;
    }

    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(driver_pid, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    char packet[9];
    build_block_request_packet(packet, device_id, start, count);

    error = com_send_char_array_fast_blocking(channel_id, packet, 9, REASON_BLOCK_READ_BYTES);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    KELP_BLOCK_CHECK_FOR_SERVICE_ERROR;

    uint16_t reason;
    error = com_get_char_array_blocking(channel_id, buffer, buffer_size, bytes_read, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason != REASON_BLOCK_READ_BYTES) {
        return KELP_WRONG_REASON;
    }

    KELP_BLOCK_CHECK_FOR_SERVICE_ERROR;

    return KELP_OK;
}

kelp_error_t kelp_block_write_bytes(uint8_t device_id, uint8_t* buffer, uint32_t buffer_size, uint32_t *bytes_written, uint32_t start, uint32_t count) {
    uint32_t driver_pid = kelp_block_get_driver_pid(device_id);
    if (driver_pid == 0) {
        driver_pid = BLOCK_SERVICE_PID;
    }

    uint16_t channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(driver_pid, true, &channel_id);
    KELP_RETURN_ON_ERROR(error);

    char packet[9];
    build_block_request_packet(packet, device_id, start, count);
    error = com_send_char_array_fast_blocking(channel_id, packet, 9, REASON_BLOCK_WRITE_BYTES);
    KELP_RETURN_ON_ERROR(error);

    // TODO: we need to check, but this will return empty, as handler sends nothing
    // KELP_BLOCK_CHECK_FOR_SERVICE_ERROR;

    error = com_send_char_array_blocking(channel_id, buffer, buffer_size, REASON_BLOCK_WRITE_BYTES);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t service_error;
    KELP_BLOCK_CHECK_FOR_SERVICE_ERROR;

    uint16_t reason;
    uint32_t response;
    error = com_get_uint32_blocking(channel_id, &response, &reason);
    KELP_RETURN_ON_ERROR(error);

    if (reason != REASON_BLOCK_WRITE_BYTES) {
        return KELP_WRONG_REASON;
    }

    if (bytes_written != NULL) {
        *bytes_written = response;
    } else {
        return KELP_ERROR;
    }

    KELP_BLOCK_CHECK_FOR_SERVICE_ERROR;

    return KELP_OK;
}

static kelp_error_t kelp_block_reset_device(struct block_device_t* driver) {
    driver->driver_pid = 0;
    driver->block_size = 0;
    driver->block_count = 0;
    driver->size = 0;
    return KELP_OK;
}

static kelp_error_t kelp_block_add_device(uint8_t* device_id, uint32_t pid, uint32_t block_size, uint32_t block_count) {
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

static kelp_error_t kelp_block_remove_device(uint32_t pid, uint8_t device_id) {
    struct block_device_t* driver = &block_devices[device_id];
    if (driver->driver_pid == pid) {
        kelp_block_reset_device(driver);
        return KELP_OK;
    }
    return KELP_NO_TASK;
}

static kelp_error_t kelp_block_handle_mount_request(uint16_t channel_id, char data[CHANNEL_SIZE], uint16_t size) {
    if (size != 8) {
        return KELP_PROTOCOL;
    }
    uint32_t block_size = get_uint32_be(&data[0]);
    uint32_t block_count = get_uint32_be(&data[4]);

    uint8_t device_id = 0;
    kelp_error_t error = kelp_block_add_device(&device_id, get_channel_partner_pid(channel_id), block_size, block_count);
    KELP_RETURN_ON_ERROR(error);

    BLOCK_SERVICE_ACTIVITY_LED_ON;
    error = com_send_uint32_blocking(channel_id, device_id, REASON_BLOCK_MOUNT);
    kelp_fs_device_connected(device_id);
    BLOCK_SERVICE_ACTIVITY_LED_OFF;
    return error;
}

static kelp_error_t kelp_block_handle_unmount_request(uint16_t channel_id, uint8_t device_id) {
    if (!is_valid_device_id(device_id)) {
        return KELP_INVALID_ID;
    }
    BLOCK_SERVICE_ACTIVITY_LED_ON;
    kelp_error_t error = kelp_block_remove_device(get_channel_partner_pid(channel_id), device_id);
    kelp_fs_device_removed(device_id);
    BLOCK_SERVICE_ACTIVITY_LED_OFF;
    return error;
}

static kelp_error_t kelp_block_handle_driver_pid_request(uint16_t channel_id, uint8_t device_id) {
    struct block_device_t* device = &block_devices[device_id];

    uint32_t driver_pid = 0;
    if (device->block_count > 0) {
        driver_pid = device->driver_pid;
    }

    return com_send_uint32_blocking(channel_id, driver_pid, REASON_BLOCK_GET_DRIVER_PID);
}

static kelp_error_t kelp_block_handle_read_request(uint16_t channel_id, char data[CHANNEL_SIZE], uint16_t size) {
    if (size != 9) {
        return KELP_PROTOCOL;
    }
    uint8_t device_id = data[0];
    if (!is_valid_device_id(device_id)) {
        return KELP_INVALID_ID;
    }

    struct block_device_t* block_driver = &block_devices[device_id];
    uint32_t count = get_uint32_be(&data[5]);
    uint32_t return_buffer_size = count * block_driver->block_size;
    uint32_t driver_pid = block_driver->driver_pid;

    uint16_t driver_channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(driver_pid, true, &driver_channel_id);
    KELP_RETURN_ON_ERROR(error);

    error = com_send_char_array_fast_blocking(driver_channel_id, data, 9, REASON_BLOCK_READ_BYTES);
    KELP_RETURN_ON_ERROR(error);

    // check if driver returned an error
    kelp_error_t driver_error;
    KELP_BLOCK_CHECK_FOR_DRIVER_ERROR;

    uint8_t* buffer = malloc(return_buffer_size);
    if (!buffer) {
        return KELP_MEMORY;
    }

    uint32_t num_bytes_read = 0;
    uint16_t reason;
    error = com_get_char_array_blocking(driver_channel_id, buffer, return_buffer_size, &num_bytes_read, &reason);
    if (error != KELP_OK) {
        free(buffer);
        return error;
    }
    if (reason != REASON_BLOCK_READ_BYTES) {
        free(buffer);
        return KELP_WRONG_REASON;
    }

    KELP_BLOCK_CHECK_FOR_DRIVER_ERROR;

    error = com_send_char_array_blocking(channel_id, buffer, num_bytes_read, REASON_BLOCK_READ_BYTES);
    free(buffer);
    return error;
}

static kelp_error_t kelp_block_handle_write_request(uint16_t channel_id, char* data, uint16_t size) {
    if (size != 9) {
        return KELP_PROTOCOL;
    }
    uint8_t device_id = data[0];
    if (!is_valid_device_id(device_id)) {
        return KELP_INVALID_ID;
    }

    struct block_device_t* block_driver = &block_devices[device_id];
    uint32_t count = get_uint32_be(&data[5]);
    uint32_t buffer_size = count * block_driver->block_size;
    uint32_t driver_pid = block_driver->driver_pid;

    uint16_t driver_channel_id = 0;
    kelp_error_t error = com_channel_request_blocking(driver_pid, true, &driver_channel_id);
    KELP_RETURN_ON_ERROR(error);

    // get buffer to write from caller
    uint8_t* buffer = malloc(buffer_size);
    if (!buffer) {
        return KELP_MEMORY;
    }

    uint32_t received_buffer_size = 0;
    uint16_t reason;
    error = com_get_char_array_blocking(channel_id, buffer, buffer_size, &received_buffer_size, &reason);
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

    // send header + data to driver
    error = com_send_char_array_fast_blocking(driver_channel_id, data, size, REASON_BLOCK_WRITE_BYTES);
    if (error != KELP_OK) {
        free(buffer);
        return error;
    }

    // TODO: we need to check, but this will return empty, as handler sends nothing
    // KELP_BLOCK_CHECK_FOR_SERVICE_ERROR;

    error = com_send_char_array_blocking(driver_channel_id, buffer, buffer_size, REASON_BLOCK_WRITE_BYTES);
    free(buffer);
    KELP_RETURN_ON_ERROR(error);

    kelp_error_t driver_error;
    KELP_BLOCK_CHECK_FOR_DRIVER_ERROR;

    uint32_t response;
    error = com_get_uint32_blocking(driver_channel_id, &response, &reason);
    KELP_RETURN_ON_ERROR(error);
    if (reason != REASON_BLOCK_WRITE_BYTES) {
        return KELP_WRONG_REASON;
    }

    KELP_BLOCK_CHECK_FOR_DRIVER_ERROR;

    return com_send_uint32_blocking(channel_id, response, REASON_BLOCK_WRITE_BYTES);
}

void kelp_task_block_service(uint32_t pid, uint32_t* signals, char* args) {
    task_request_stack(256);

    uint32_t l = 0;

#if BLOCK_SERVICE_ACTIVITY_LED >= 0
    gpio_init(BLOCK_SERVICE_ACTIVITY_LED);
    gpio_set_dir(BLOCK_SERVICE_ACTIVITY_LED, GPIO_OUT);
    gpio_put(BLOCK_SERVICE_ACTIVITY_LED, false);
#endif

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
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_BLOCK_READ_BYTES:
                        // send the requested bytes
                        error = kelp_block_handle_read_request(channel_id, data, size);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_BLOCK_WRITE_BYTES:
                        // send the requested bytes
                        error = kelp_block_handle_write_request(channel_id, data, size);
                        com_send_error_blocking(channel_id, error);
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
                        error = kelp_block_handle_driver_pid_request(channel_id, data);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_BLOCK_UNMOUNT:
                        // get device id from channel
                        error = kelp_block_handle_unmount_request(channel_id, data);
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_BLOCK_SIZE:
                        // get device id from channel
                        if (!is_valid_device_id(data)) {
                            error = KELP_INVALID_ID;
                        } else if (block_devices[data].driver_pid == 0) {
                            error = KELP_NO_EXIST;
                        } else {
                            error = com_send_uint32_blocking(channel_id, block_devices[data].block_size, REASON_BLOCK_SIZE);
                        }
                        com_send_error_blocking(channel_id, error);
                        break;
                    case REASON_BLOCK_COUNT:
                        // get device id from channel
                        if (!is_valid_device_id(data)) {
                            error = KELP_INVALID_ID;
                        } else if (block_devices[data].driver_pid == 0) {
                            error = KELP_NO_EXIST;
                        } else {
                            error = com_send_uint32_blocking(channel_id, block_devices[data].block_count, REASON_BLOCK_COUNT);
                        }
                        com_send_error_blocking(channel_id, error);
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