//
// Created by wolfboy on 5/1/2026.
//

#ifndef KELPOS_LITE_BLOCK_SERVICE_H
#define KELPOS_LITE_BLOCK_SERVICE_H

#define BLOCK_SERVICE_PID 201

#define BLOCK_SERVICE_MAX_DEVICES 4

#define REASON_BLOCK_MOUNT 8975
#define REASON_BLOCK_UNMOUNT 54365
#define REASON_BLOCK_READ_BYTES 9346
#define REASON_BLOCK_WRITE_BYTES 25675
#define REASON_BLOCK_GET_DRIVER_PID 39487
#define REASON_BLOCK_ERROR 4908

#include <stdint.h>

#include "error_codes.h"

struct block_device_t {
    uint32_t driver_pid;
    uint32_t block_size;
    uint32_t block_count;
    uint32_t size;
};

kelp_error_t kelp_block_mount_device(uint8_t* device_id, uint32_t block_size, uint32_t block_count);

kelp_error_t kelp_block_unmount_device(uint8_t device_id);

kelp_error_t kelp_block_read_bytes(uint8_t device_id, uint8_t* buffer, uint32_t buffer_size, uint32_t *bytes_read, uint32_t start, uint32_t count);

kelp_error_t kelp_block_write_bytes(uint8_t device_id, uint8_t* buffer, uint32_t buffer_size, uint32_t *bytes_written, uint32_t start, uint32_t count);

void kelp_task_block_service(uint32_t pid, uint32_t* signals, char* args);

#endif //KELPOS_LITE_BLOCK_SERVICE_H