//
// Created by wolfboy on 10/7/2026.
//

#ifndef KELPOS_LITE_FF_KELP_H
#define KELPOS_LITE_FF_KELP_H

#include "file_service.h"
#include "include/ff.h"

typedef struct {
    uint8_t device_id;
    uint32_t block_size;
    uint32_t block_count;
} kelp_fatfs_driver_context_t;

extern const struct kelp_fs_backend_plugin kelp_fatfs_plugin;

#endif //KELPOS_LITE_FF_KELP_H
