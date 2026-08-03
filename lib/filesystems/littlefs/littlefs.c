//
// Created by wolfboy on 7/29/2026.
//

#include "littlefs.h"

#include "block_service.h"

__attribute__((noinline))
static int kelp_lfsv2_block_device_read(const struct lfs_config* c, lfs_block_t block,
                                        lfs_off_t off, void* buffer, lfs_size_t size) {
    if (off != 0) {
        return LFS_ERR_INVAL;
    }

    if (size % c->block_size != 0) {
        return LFS_ERR_INVAL;
    }

    uint32_t bytes_read;
    kelp_error_t error = kelp_block_read_bytes(0, buffer, size, &bytes_read, block, size / 512);
    if (error != KELP_OK) {
        return LFS_ERR_IO;
    }

    if (bytes_read != size) {
        return LFS_ERR_IO;
    }

    return LFS_ERR_OK;
}

__attribute__((noinline))
static int kelp_lfsv2_block_device_prog(const struct lfs_config* c, lfs_block_t block,
                                        lfs_off_t off, const void* buffer, lfs_size_t size) {
    if (off != 0) {
        return LFS_ERR_INVAL;
    }

    if (size % c->block_size != 0) {
        return LFS_ERR_INVAL;
    }

    uint32_t bytes_written;
    kelp_error_t error = kelp_block_write_bytes(0, buffer, size, &bytes_written, block, size / 512);
    if (error != KELP_OK) {
        return LFS_ERR_IO;
    }

    if (bytes_written != size) {
        return LFS_ERR_IO;
    }

    return LFS_ERR_OK;
}

__attribute__((noinline))
static int kelp_lfsv2_block_device_erase(const struct lfs_config* c, lfs_block_t block) {
    return LFS_ERR_OK;
}

__attribute__((noinline))
static int kelp_lfsv2_block_device_sync(const struct lfs_config* c) {
    return LFS_ERR_OK;
}

const struct lfs_config kelp_lfsv2_default_cfg = {
    // block device operations
    .read = kelp_lfsv2_block_device_read,
    .prog = kelp_lfsv2_block_device_prog,
    .erase = kelp_lfsv2_block_device_erase,
    .sync = kelp_lfsv2_block_device_sync,

    // block device configuration
    .read_size = 512,
    .prog_size = 512,
    .block_size = 512,
    .block_count = 0,
    .cache_size = 512,
    .lookahead_size = 32,
    .block_cycles = 500,
};

static int translate_flags(uint32_t flags) {
    int lfs_flags = 0;
    if (flags & FS_O_RDONLY)  lfs_flags |= LFS_O_RDONLY;
    if (flags & FS_O_WRONLY)  lfs_flags |= LFS_O_WRONLY;
    if (flags & FS_O_RDWR)    lfs_flags |= LFS_O_RDWR;
    if (flags & FS_O_CREAT)   lfs_flags |= LFS_O_CREAT;
    if (flags & FS_O_EXCL)    lfs_flags |= LFS_O_EXCL;
    if (flags & FS_O_TRUNC)   lfs_flags |= LFS_O_TRUNC;
    if (flags & FS_O_APPEND)  lfs_flags |= LFS_O_APPEND;
    return lfs_flags;
}

static struct lfs_config* build_config(uint8_t device_id) {
    uint32_t block_size;
    kelp_error_t error = kelp_block_get_block_size(device_id, &block_size);
    if (error != KELP_OK) {
        printf("LittleFS V2 Error %ld at line %d\n", error, __LINE__);
        return NULL;
    }

    uint32_t block_count;
    error = kelp_block_get_block_count(device_id, &block_count);
    if (error != KELP_OK) {
        printf("LittleFS V2 Error %ld at line %d\n", error, __LINE__);
        return NULL;
    }

    kelp_lfsv2_driver_context_t* driver_ctx = malloc(sizeof(kelp_lfsv2_driver_context_t));
    if (driver_ctx == NULL) {
        printf("LittleFS V2 malloc error at line %d\n", __LINE__);
        return NULL;
    }

    driver_ctx->device_id = device_id;
    driver_ctx->block_size = block_size;
    driver_ctx->block_count = block_count;

    // set up lfs config
    struct lfs_config* cfg = malloc(sizeof(struct lfs_config));
    if (cfg == NULL) {
        free(driver_ctx);
        printf("LittleFS V2 malloc error at line %d\n", __LINE__);
        return NULL;
    }

    // copy defaults
    memcpy(cfg, &kelp_lfsv2_default_cfg, sizeof(struct lfs_config));

    // modify defaults
    cfg->block_size = block_size;
    cfg->read_size = block_size;
    cfg->prog_size = block_size;
    cfg->cache_size = block_size;
    cfg->block_count = block_count;

    // save driver context in config
    cfg->context = driver_ctx;

    return cfg;
}

bool kelp_lfsv2_probe(uint8_t device_id) {
    // read the superblock of littlefs v2

    uint32_t block_size;
    kelp_error_t error = kelp_block_get_block_size(device_id, &block_size);
    if (error != KELP_OK) {
        printf("LittleFS V2 Error %ld at line %d\n", error, __LINE__);
        return false;
    }

    uint32_t block_count;
    error = kelp_block_get_block_count(device_id, &block_count);
    if (error != KELP_OK) {
        printf("LittleFS V2 Error %ld at line %d\n", error, __LINE__);
        return false;
    }

    const uint32_t superblock_size_bytes = 16;
    const uint32_t superblock_size_blocks = (superblock_size_bytes + block_size - 1) / block_size;

    uint8_t* buffer = malloc(superblock_size_blocks * block_size);
    if (buffer == NULL) {
        return false;
    }

    uint32_t bytes_read;
    error = kelp_block_read_bytes(device_id, buffer, block_size, &bytes_read, 0, superblock_size_blocks);
    if (error != KELP_OK) {
        free(buffer);
        printf("LittleFS V2 Error %ld at line %d\n", error, __LINE__);
        return false;
    }

    if (bytes_read != (superblock_size_blocks * block_size)) {
        free(buffer);
        return false;
    }

    // for (uint32_t i = 0; i < superblock_size_bytes; i++) {
    //     printf("Byte %lu: %X\n", i, buffer[i]);
    // }

    // NOT bytes 4-7 to decode tag
    for (uint32_t i = 4; i <= 7; i++) {
        buffer[i] = ~buffer[i];
    }

    bool valid = (buffer[4] & 0b10000000) == 0; // decoded validity bit must be 0
    if (!valid) {
        free(buffer);
        printf("LittleFS V2 validity error at line %d\n", __LINE__);
        return false;
    }

    uint16_t type = (((buffer[4] << 8) | buffer[5]) & 0b0111111111110000) >> 4;
    if (type != 0x0ff) {
        free(buffer);
        printf("LittleFS V2 type error at line %d\n", __LINE__);
        return false;
    }

    uint8_t magic[] = {
        'l', 'i', 't', 't', 'l', 'e', 'f', 's'
    };

    for (uint8_t c = 0; c < 8; c++) {
        if (buffer[c + 8] != magic[c]) {
            free(buffer);
            printf("LittleFS V2 magic error at line %d\n", __LINE__);
            return false;
        }
    }

    free(buffer);
    return true;
}

void* kelp_lfsv2_mount(uint8_t device_id) {
    struct lfs_config* cfg = build_config(device_id);
    if (cfg == NULL) {
        return NULL;
    }

    // set up lfs object
    struct lfs* lfs = malloc(sizeof(struct lfs));
    if (lfs == NULL) {
        free(cfg);
        return NULL;
    }

    int error = lfs_mount(lfs, cfg);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        free(lfs);
        free(cfg);
        return NULL;
    }

    return lfs;
}

kelp_error_t kelp_lfsv2_unmount(kelp_fs_mount_t* mount) {
    struct lfs* lfs = (struct lfs*)mount->context;

    if (lfs == NULL) {
        printf("LittleFS V2 missing context at line %d\n", __LINE__);
        return KELP_NO_EXIST;
    }

    if (lfs->cfg == NULL) {
        printf("LittleFS V2 missing config at line %d\n", __LINE__);
        return KELP_NO_EXIST;
    }

    if (lfs->cfg->context == NULL) {
        printf("LittleFS V2 missing driver context at line %d\n", __LINE__);
        return KELP_NO_EXIST;
    }

    lfs_unmount(lfs);

    free(lfs->cfg->context);
    free((struct lfs_config*)lfs->cfg);
    free(lfs);

    return KELP_OK;
}

kelp_error_t kelp_lfsv2_open(kelp_fs_mount_t* mount, const char* path,
                             uint32_t flags, void** handle) {
    lfs_file_t* file = malloc(sizeof(lfs_file_t));
    if (file == NULL) {
        printf("LittleFS V2 failed to malloc memory at %d\n", __LINE__);
        return KELP_MEMORY;
    }

    struct lfs* lfs = mount->context;

    int lfs_flags = translate_flags(flags);

    int error = lfs_file_open(lfs, file, path, lfs_flags);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        free(file);
        return error;
    }

    *handle = file;
    return KELP_OK;
}

kelp_error_t kelp_lfsv2_close(kelp_fs_mount_t* mount, void* handle) {
    int error = lfs_file_close(mount->context, handle);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }
    free(handle);
    return KELP_OK;
}

kelp_error_t kelp_lfsv2_read(kelp_fs_mount_t* mount, void* handle, void* buf, uint32_t len,
                             uint32_t* bytes_read) {
    struct lfs* lfs = mount->context;
    int error = lfs_file_read(lfs, handle, buf, len);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }

    *bytes_read = (uint32_t)buf;
    return KELP_OK;
}

kelp_error_t kelp_lfsv2_write(kelp_fs_mount_t* mount, void* handle, const void* buf, uint32_t len,
                              uint32_t* bytes_written) {
    struct lfs* lfs = mount->context;
    int error = lfs_file_write(lfs, handle, buf, len);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }

    *bytes_written = (uint32_t)buf;
    return KELP_OK;
}

kelp_error_t kelp_lfsv2_stat(kelp_fs_mount_t* mount, const char* path,
                             kelp_fs_dirent_t* out) {
    struct lfs* lfs = mount->context;
    struct lfs_info info;

    int error = lfs_stat(lfs, path, &info);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }

    switch (info.type) {
    case LFS_TYPE_REG:
        out->type = FS_DT_FILE;
        break;
    case LFS_TYPE_DIR:
        out->type = FS_DT_DIR;
        break;
    default:
        out->type = FS_DT_UNKNOWN;
    }

    out->size = info.size;
    out->modified = 0;

    if (strlen(info.name) > FILE_SERVICE_MAX_NAME) {
        return KELP_TOO_BIG;
    }

    strncpy(out->name, info.name, FILE_SERVICE_MAX_NAME + 1);

    return KELP_OK;
}

const struct kelp_fs_backend_plugin kelp_lfsv2_plugin = {
    .name = "lfsv2",
    .probe = kelp_lfsv2_probe,
    .mount = kelp_lfsv2_mount,
    .unmount = kelp_lfsv2_unmount,
    .open = kelp_lfsv2_open,
    .close = kelp_lfsv2_close,
    .read = kelp_lfsv2_read,
    .write = kelp_lfsv2_write,
    .stat = kelp_lfsv2_stat,
    .max_name_len = LFS_NAME_MAX,
    .flags = 0,
};
