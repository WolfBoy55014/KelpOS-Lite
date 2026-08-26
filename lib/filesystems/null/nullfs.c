//
// Created by wolfboy on 7/21/2026.
//

#include "nullfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block_service.h"

bool kelp_nullfs_probe(uint8_t device_id) {
    // get the first 5 bytes to tell if this disk uses nullfs
    uint32_t block_size;
    kelp_error_t error = kelp_block_get_block_size(device_id, &block_size);
    if (error != KELP_OK) {
        printf("NullFS Error %ld at line %d\n", error, __LINE__);
        return false;
    }

    if (block_size < 5) {
        printf("NullFS Error too small at line %d\n", __LINE__);
        return false;
    }

    uint8_t* buffer = malloc(block_size);
    if (buffer == NULL) {
        return false;
    }
    uint32_t bytes_read;
    error = kelp_block_read_bytes(device_id, buffer, block_size, &bytes_read, 0, 1);
    if (error != KELP_OK) {
        free(buffer);
        printf("NullFS Error %ld at line %d\n", error, __LINE__);
        return false;
    }

    if (bytes_read != block_size) {
        free(buffer);
        return false;
    }

    if (strcmp(buffer, "null") == 0) {
        free(buffer);
        return true;
    }

    free(buffer);
    return false;
}

void* kelp_nullfs_mount(uint8_t device_id) {
    kelp_nullfs_ctx_t* context = malloc(sizeof(kelp_nullfs_ctx_t));
    if (context == NULL) {
        return NULL;
    }

    uint32_t block_size;
    kelp_error_t error = kelp_block_get_block_size(device_id, &block_size);
    if (error != KELP_OK) {
        free(context);
        return NULL;
    }

    uint32_t block_count;
    error = kelp_block_get_block_count(device_id, &block_count);
    if (error != KELP_OK) {
        free(context);
        return NULL;
    }

    uint32_t boot_count = 0;
    uint8_t* buffer = malloc(block_size);
    if (buffer == NULL) {
        free(context);
        return NULL;
    }

    uint32_t bytes_read;
    error = kelp_block_read_bytes(device_id, buffer, block_size, &bytes_read, 1, 1);
    if (error != KELP_OK || bytes_read != block_size) {
        free(buffer);
        free(context);
        return NULL;
    }
    memcpy(&boot_count, buffer, sizeof(boot_count));

    boot_count++;
    printf("Boot Count: %lu\n", boot_count);

    memcpy(buffer, &boot_count, sizeof(boot_count));
    uint32_t bytes_written;
    error = kelp_block_write_bytes(device_id, buffer, block_size, &bytes_written, 1, 1);
    if (error != KELP_OK || bytes_written != block_size) {
        free(buffer);
        free(context);
        return NULL;
    }

    free(buffer);
    return context;
}

kelp_error_t kelp_nullfs_unmount(kelp_fs_mount_t* mount) {
    free(mount->context);
    return KELP_OK;
}

const struct kelp_fs_backend_plugin kelp_nullfs_plugin = {
    .name           = "nullfs",
    .probe          = kelp_nullfs_probe,
    .mount          = kelp_nullfs_mount,
    .unmount        = kelp_nullfs_unmount,
    .stat           = NULL,

    /* File ops */
    .file_open      = NULL,
    .file_close     = NULL,
    .file_read      = NULL,
    .file_write     = NULL,
    .file_seek      = NULL,
    .file_tell      = NULL,
    .file_size      = NULL,
    .file_flush     = NULL,
    .file_truncate  = NULL,
    .file_rename    = NULL,

    /* Directory ops */
    .dir_mkdir      = NULL,
    .dir_opendir    = NULL,
    .dir_readdir    = NULL,
    .dir_closedir   = NULL,
    .dir_rewinddir  = NULL,

    .max_name_len   = 255,
    .flags          = 0,
};
