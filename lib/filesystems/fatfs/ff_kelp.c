//
// Created by wolfboy on 10/7/2026.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "file_service.h"
#include "ff.h"
#include "ffconf.h"

static bool kelp_fatfs_probe(uint8_t device_id) {
    FATFS* fatfs = malloc(sizeof(FATFS));

    if (fatfs == NULL) {
        return false;
    }

    FRESULT error = f_mount(fatfs, device_id, 1);
    free(fatfs);

    if (error == FR_NO_FILESYSTEM) {
        return false;
    }

    return true;
}

void* kelp_fatfs_mount(uint8_t device_id) {
    FATFS* fatfs = malloc(sizeof(FATFS));

    if (fatfs == NULL) {
        return NULL;
    }

    FRESULT error = f_mount(fatfs, device_id, 1);

    if (error != FR_OK) {
        free(fatfs);
        return NULL;
    }

    return fatfs;
}

kelp_error_t kelp_fatfs_unmount(kelp_fs_mount_t* mount) {
    free(mount->context);
    return KELP_OK;
}

/*
kelp_error_t kelp_fatfs_stat(kelp_fs_mount_t* mount, const char* path,
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

kelp_error_t kelp_fatfs_rename(kelp_fs_mount_t* mount, const char* old_path, const char* new_path) {
    struct lfs* lfs = mount->context;
    int error = lfs_rename(lfs, old_path, new_path);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_fatfs_remove(kelp_fs_mount_t* mount, const char* path) {
    struct lfs* lfs = mount->context;
    int error = lfs_remove(lfs, path);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_fatfs_file_open(kelp_fs_mount_t* mount, const char* path,
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

kelp_error_t kelp_fatfs_file_close(kelp_fs_mount_t* mount, void* handle) {
    int error = lfs_file_close(mount->context, handle);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }
    free(handle);
    return KELP_OK;
}

kelp_error_t kelp_fatfs_file_read(kelp_fs_mount_t* mount, void* handle, uint8_t* buf, uint32_t len,
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

kelp_error_t kelp_fatfs_file_write(kelp_fs_mount_t* mount, void* handle, const uint8_t* buf, uint32_t len,
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

kelp_error_t kelp_fatfs_file_seek(kelp_fs_mount_t* mount, void* handle, int32_t offset, kelp_fs_seek_t whence) {
    struct lfs* lfs = mount->context;
    int error = lfs_file_seek(lfs, handle, offset, whence);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }

    return KELP_OK;
}

kelp_error_t kelp_fatfs_file_tell(kelp_fs_mount_t* mount, void* handle, uint32_t* pos) {
    struct lfs* lfs = mount->context;
    int error = lfs_file_tell(lfs, handle);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }

    *pos = (uint32_t)error;
    return KELP_OK;
}

kelp_error_t kelp_fatfs_file_size(kelp_fs_mount_t* mount, void* handle, uint32_t* size) {
    struct lfs* lfs = mount->context;
    int error = lfs_file_size(lfs, handle);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }

    *size = (uint32_t)error;
    return KELP_OK;
}

kelp_error_t kelp_fatfs_file_flush(kelp_fs_mount_t* mount, void* handle) {
    struct lfs* lfs = mount->context;
    int error = lfs_file_sync(lfs, handle);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_fatfs_file_truncate(kelp_fs_mount_t* mount, void* handle, uint32_t size) {
    struct lfs* lfs = mount->context;
    int error = lfs_file_truncate(lfs, handle, size);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_fatfs_dir_mkdir(kelp_fs_mount_t* mount, const char* path) {
    struct lfs* lfs = mount->context;
    int error = lfs_mkdir(lfs, path);
    if (error < 0) {
        printf("LittleFS V2 Error %d at line %d\n", error, __LINE__);
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_fatfs_dir_open(kelp_fs_mount_t* mount, const char* path, void** dir_handle) {
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

kelp_error_t kelp_fatfs_dir_close(kelp_fs_mount_t* mount, void* dir_handle) {
    struct lfs* lfs = mount->context;
    int error = lfs_dir_close(lfs, dir_handle);
    if (error < 0) {
        return error;
    }
    free(dir_handle);
    return KELP_OK;
}

kelp_error_t kelp_fatfs_dir_read(kelp_fs_mount_t* mount, void* dir_handle, kelp_fs_dirent_t* entry) {
    struct lfs* lfs = mount->context;

    struct lfs_info info;
    int error = lfs_dir_read(lfs, dir_handle, &info);
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

kelp_error_t kelp_fatfs_dir_rewind(kelp_fs_mount_t* mount, void* dir_handle) {
    struct lfs* lfs = mount->context;
    int error = lfs_dir_rewind(lfs, dir_handle);
    if (error < 0) {
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_fatfs_dir_seek(kelp_fs_mount_t* mount, void* dir_handle, int32_t offset) {
    struct lfs* lfs = mount->context;
    int error = lfs_dir_seek(lfs, dir_handle, offset);
    if (error < 0) {
        return error;
    }
    return KELP_OK;
}

kelp_error_t kelp_fatfs_dir_tell(kelp_fs_mount_t* mount, void* dir_handle, int32_t *pos) {
    struct lfs* lfs = mount->context;
    int error = lfs_dir_tell(lfs, dir_handle);
    if (error < 0) {
        return error;
    }
    *pos = error;
    return KELP_OK;
}

*/

const struct kelp_fs_backend_plugin kelp_fatfs_plugin = {
    .name = "fatfs",
    .probe = kelp_fatfs_probe,
    .mount = kelp_fatfs_mount,
    .unmount = kelp_fatfs_unmount,
    // .stat = kelp_fatfs_stat,
    // .rename = kelp_fatfs_rename,
    // .remove = kelp_fatfs_remove,
    //
    // /* File ops */
    // .file_open = kelp_fatfs_file_open,
    // .file_close = kelp_fatfs_file_close,
    // .file_read = kelp_fatfs_file_read,
    // .file_write = kelp_fatfs_file_write,
    // .file_seek = kelp_fatfs_file_seek,
    // .file_tell = kelp_fatfs_file_tell,
    // .file_size = kelp_fatfs_file_size,
    // .file_flush = kelp_fatfs_file_flush,
    // .file_truncate = kelp_fatfs_file_truncate,
    //
    // /* Directory ops */
    // .dir_mkdir = kelp_fatfs_dir_mkdir,
    // .dir_open = kelp_fatfs_dir_open,
    // .dir_close = kelp_fatfs_dir_close,
    // .dir_read = kelp_fatfs_dir_read,
    // .dir_rewind = kelp_fatfs_dir_rewind,
    // .dir_seek = kelp_fatfs_dir_seek,
    // .dir_tell = kelp_fatfs_dir_tell,

    .max_name_len = FF_MAX_LFN,
    .flags = 0,
};
