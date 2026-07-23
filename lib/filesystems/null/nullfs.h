//
// Created by wolfboy on 7/21/2026.
//

#ifndef KELPOS_LITE_NULLFS_H
#define KELPOS_LITE_NULLFS_H
#include <stddef.h>

#include "file_service.h"

// This a dummy FS for testing. It might become more later

typedef struct {
    uint32_t boot_count;
    uint32_t block_count;
    uint32_t block_size;
} kelp_nullfs_ctx_t;

extern const struct kelp_fs_backend_plugin kelp_nullfs_plugin;

#endif //KELPOS_LITE_NULLFS_H
