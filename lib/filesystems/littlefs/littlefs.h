//
// Created by wolfboy on 7/29/2026.
//

#ifndef KELPOS_LITE_LITTLEFS_H
#define KELPOS_LITE_LITTLEFS_H

#include "file_service.h"
#include "lfs.h"

typedef struct {
    uint8_t device_id;
    uint32_t block_size;
    uint32_t block_count;
} kelp_lfsv2_driver_context_t;

extern const struct kelp_fs_backend_plugin kelp_lfsv2_plugin;

#endif //KELPOS_LITE_LITTLEFS_H
