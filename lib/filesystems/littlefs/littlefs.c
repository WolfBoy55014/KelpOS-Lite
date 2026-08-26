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

    kelp_lfsv2_driver_context_t* driver_ctx = c->context;
    uint32_t block_size = driver_ctx->block_size;
    uint8_t device_id = driver_ctx->device_id;

    uint32_t bytes_read;
    kelp_error_t error = kelp_block_read_bytes(device_id, buffer, size, &bytes_read, block, size / block_size);
    if (error != KELP_OK) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return LFS_ERR_IO;
    }

    if (bytes_read != size) {
        printf("LittleFS V2 did not read specified bytes at line %d\n", __LINE__);
        printf("Requested %lu, but got %lu\n", size, bytes_read);
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

    kelp_lfsv2_driver_context_t* driver_ctx = c->context;
    uint32_t block_size = driver_ctx->block_size;
    uint8_t device_id = driver_ctx->device_id;

    uint32_t bytes_written;
    kelp_error_t error = kelp_block_write_bytes(device_id, buffer, size, &bytes_written, block, size / block_size);
    if (error != KELP_OK) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return LFS_ERR_IO;
    }

    if (bytes_written != size) {
        printf("LittleFS V2 did not write specified bytes at line %d\n", __LINE__);
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

kelp_error_t kelp_lfsv2_file_open(kelp_fs_mount_t* mount, const char* path,
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

kelp_error_t kelp_lfsv2_file_close(kelp_fs_mount_t* mount, void* handle) {
    int error = lfs_file_close(mount->context, handle);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }
    free(handle);
    return KELP_OK;
}

kelp_error_t kelp_lfsv2_file_read(kelp_fs_mount_t* mount, void* handle, uint8_t* buf, uint32_t len,
                                  uint32_t* bytes_read) {
    struct lfs* lfs = mount->context;
    int error = lfs_file_read(lfs, handle, buf, len);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }

    *bytes_read = (uint32_t)error;
    return KELP_OK;
}

kelp_error_t kelp_lfsv2_file_write(kelp_fs_mount_t* mount, void* handle, const uint8_t* buf, uint32_t len,
                                   uint32_t* bytes_written) {
    struct lfs* lfs = mount->context;
    int error = lfs_file_write(lfs, handle, buf, len);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }

    *bytes_written = (uint32_t)error;
    return KELP_OK;
}

kelp_error_t kelp_lfsv2_file_seek(kelp_fs_mount_t* mount, void* handle, int32_t offset, kelp_fs_seek_t whence) {
    struct lfs* lfs = mount->context;
    int error = lfs_file_seek(lfs, handle, offset, whence);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }

    return KELP_OK;
}

kelp_error_t kelp_lfsv2_file_tell(kelp_fs_mount_t* mount, void* handle, uint32_t* pos) {
    struct lfs* lfs = mount->context;
    int error = lfs_file_tell(lfs, handle);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }

    *pos = (uint32_t)error;
    return KELP_OK;
}

kelp_error_t kelp_lfsv2_file_size(kelp_fs_mount_t* mount, void* handle, uint32_t* size) {
    struct lfs* lfs = mount->context;
    int error = lfs_file_size(lfs, handle);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }

    *size = (uint32_t)error;
    return KELP_OK;
}

kelp_error_t kelp_lfsv2_file_flush(kelp_fs_mount_t* mount, void* handle) {
    struct lfs* lfs = mount->context;
    int error = lfs_file_sync(lfs, handle);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_lfsv2_file_truncate(kelp_fs_mount_t* mount, void* handle, uint32_t size) {
    struct lfs* lfs = mount->context;
    int error = lfs_file_trunc(lfs, handle, size);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_lfsv2_file_rename(kelp_fs_mount_t* mount, const char* old_path, const char* new_path) {
    struct lfs* lfs = mount->context;
    int error = lfs_rename(lfs, old_path, new_path);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_lfsv2_dir_mkdir(kelp_fs_mount_t* mount, const char* path) {
    struct lfs* lfs = mount->context;
    int error = lfs_mkdir(lfs, path);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_lfsv2_dir_opendir(kelp_fs_mount_t* mount, const char* path, void** dir_handle) {
    lfs_dir_t* dir = malloc(sizeof(lfs_dir_t));
    if (dir == NULL) {
        return KELP_MEMORY;
    }

    struct lfs* lfs = mount->context;
    int error = lfs_dir_open(lfs, dir, path);
    if (error < 0) {
        free(dir);
        return error;
    }

    *dir_handle = dir;
    return KELP_OK;
}

kelp_error_t kelp_lfsv2_dir_readdir(kelp_fs_mount_t* mount, void* dir_handle, kelp_fs_dirent_t* entry) {
    struct lfs* lfs = mount->context;
    struct lfs_dir dir;
    memcpy(&dir, dir_handle, sizeof(dir));

    struct lfs_info info;
    int error = lfs_dir_read(lfs, &dir, &info);
    if (error <= 0) {
        // error=0 means EOF, error<0 is actual error
        return (error < 0) ? error : 1; // return 1 for EOF
    }

    entry->type = (info.type == LFS_TYPE_REG) ? FS_DT_FILE : FS_DT_DIR;
    entry->size = info.size;
    entry->modified = 0;
    strncpy(entry->name, info.name, FILE_SERVICE_MAX_NAME);
    entry->name[FILE_SERVICE_MAX_NAME] = '\0';

    return KELP_OK;
}

kelp_error_t kelp_lfsv2_dir_closedir(kelp_fs_mount_t* mount, void* dir_handle) {
    struct lfs* lfs = mount->context;
    int error = lfs_dir_close(lfs, dir_handle);
    if (error < 0) {
        return error;
    }
    free(dir_handle);
    return KELP_OK;
}

kelp_error_t kelp_lfsv2_dir_rewinddir(kelp_fs_mount_t* mount, void* dir_handle) {
    // LittleFS doesn't support rewinddir; just close and reopen at root
    (void)mount;
    (void)dir_handle;
    return LFS_ERR_INVAL; // not supported
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
    .stat = kelp_lfsv2_stat,

    /* File ops */
    .file_open = kelp_lfsv2_file_open,
    .file_close = kelp_lfsv2_file_close,
    .file_read = kelp_lfsv2_file_read,
    .file_write = kelp_lfsv2_file_write,
    .file_seek = kelp_lfsv2_file_seek,
    .file_tell = kelp_lfsv2_file_tell,
    .file_size = kelp_lfsv2_file_size,
    .file_flush = kelp_lfsv2_file_flush,
    .file_truncate = kelp_lfsv2_file_truncate,
    .file_rename = kelp_lfsv2_file_rename,

    /* Directory ops */
    .dir_mkdir = kelp_lfsv2_dir_mkdir,
    .dir_opendir = kelp_lfsv2_dir_opendir,
    .dir_readdir = kelp_lfsv2_dir_readdir,
    .dir_closedir = kelp_lfsv2_dir_closedir,
    .dir_rewinddir = kelp_lfsv2_dir_rewinddir,

    .max_name_len = LFS_NAME_MAX,
    .flags = 0,
};
