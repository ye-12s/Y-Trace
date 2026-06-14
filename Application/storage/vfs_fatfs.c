#include "storage/vfs_fatfs.h"

#include "ff.h"

#include <string.h>

#define VFS_FATFS_MAX_OPEN_FILES 2U

typedef struct {
    uint8_t in_use;
    FIL fil;
} vfs_fatfs_file_t;

static vfs_fatfs_file_t fatfs_files[VFS_FATFS_MAX_OPEN_FILES];
static FATFS fatfs_fs;

static int fatfs_result_to_vfs(FRESULT result)
{
    switch (result) {
        case FR_OK:
            return VFS_OK;
        case FR_NO_FILE:
        case FR_NO_PATH:
            return VFS_ERR_NOT_FOUND;
        case FR_DENIED:
        case FR_WRITE_PROTECTED:
        case FR_LOCKED:
            return VFS_ERR_BUSY;
        case FR_NOT_ENOUGH_CORE:
        case FR_TOO_MANY_OPEN_FILES:
            return VFS_ERR_NO_SPACE;
        case FR_INVALID_NAME:
        case FR_INVALID_PARAMETER:
        case FR_INVALID_OBJECT:
            return VFS_ERR_INVALID;
        default:
            return VFS_ERR_IO;
    }
}

static BYTE fatfs_mode_from_flags(uint32_t flags)
{
    BYTE mode = 0U;
    if ((flags & VFS_O_READ) != 0U) {
        mode |= FA_READ;
    }
    if ((flags & VFS_O_WRITE) != 0U) {
        mode |= FA_WRITE;
    }
    if ((flags & VFS_O_TRUNC) != 0U) {
        mode |= FA_CREATE_ALWAYS;
    } else if ((flags & VFS_O_APPEND) != 0U) {
        mode |= FA_OPEN_APPEND;
    } else if ((flags & VFS_O_CREATE) != 0U) {
        mode |= FA_OPEN_ALWAYS;
    }
    return mode;
}

static vfs_fatfs_file_t *fatfs_alloc_file(void)
{
    for (uint32_t i = 0U; i < VFS_FATFS_MAX_OPEN_FILES; i++) {
        if (fatfs_files[i].in_use == 0U) {
            fatfs_files[i].in_use = 1U;
            return &fatfs_files[i];
        }
    }
    return NULL;
}

static int fatfs_mount(void *ctx)
{
    (void)ctx;
    return fatfs_result_to_vfs(f_mount(&fatfs_fs, "0:", 0));
}

static int fatfs_open(void *ctx, vfs_file_t *file, const char *path, uint32_t flags)
{
    vfs_fatfs_file_t *slot = fatfs_alloc_file();
    FRESULT result;
    (void)ctx;
    if (slot == NULL) {
        return VFS_ERR_BUSY;
    }
    memset(&slot->fil, 0, sizeof(slot->fil));
    result = f_open(&slot->fil, path, fatfs_mode_from_flags(flags));
    if (result != FR_OK) {
        slot->in_use = 0U;
        return fatfs_result_to_vfs(result);
    }
    file->backend_file = slot;
    return VFS_OK;
}

static int fatfs_read(vfs_file_t *file, void *buffer, uint32_t size, uint32_t *bytes_read)
{
    vfs_fatfs_file_t *slot = (vfs_fatfs_file_t *)file->backend_file;
    UINT count = 0U;
    int ret = fatfs_result_to_vfs(f_read(&slot->fil, buffer, size, &count));
    *bytes_read = count;
    return ret;
}

static int fatfs_write(vfs_file_t *file, const void *buffer, uint32_t size, uint32_t *bytes_written)
{
    vfs_fatfs_file_t *slot = (vfs_fatfs_file_t *)file->backend_file;
    UINT count = 0U;
    int ret = fatfs_result_to_vfs(f_write(&slot->fil, buffer, size, &count));
    *bytes_written = count;
    return ret;
}

static int fatfs_sync(vfs_file_t *file)
{
    vfs_fatfs_file_t *slot = (vfs_fatfs_file_t *)file->backend_file;
    return fatfs_result_to_vfs(f_sync(&slot->fil));
}

static int fatfs_close(vfs_file_t *file)
{
    vfs_fatfs_file_t *slot = (vfs_fatfs_file_t *)file->backend_file;
    int ret = fatfs_result_to_vfs(f_close(&slot->fil));
    slot->in_use = 0U;
    file->backend_file = NULL;
    return ret;
}

static int fatfs_delete(void *ctx, const char *path)
{
    (void)ctx;
    return fatfs_result_to_vfs(f_unlink(path));
}

static int fatfs_rename(void *ctx, const char *old_path, const char *new_path)
{
    (void)ctx;
    return fatfs_result_to_vfs(f_rename(old_path, new_path));
}

static int fatfs_stat(void *ctx, const char *path, vfs_stat_t *stat)
{
    FILINFO info;
    FRESULT result;
    (void)ctx;
    result = f_stat(path, &info);
    if (result != FR_OK) {
        return fatfs_result_to_vfs(result);
    }
    stat->size = (uint32_t)info.fsize;
    stat->is_dir = (info.fattrib & AM_DIR) != 0U ? 1U : 0U;
    return VFS_OK;
}

const vfs_backend_t vfs_fatfs_backend = {
    .name = "fatfs",
    .mount = fatfs_mount,
    .open = fatfs_open,
    .read = fatfs_read,
    .write = fatfs_write,
    .sync = fatfs_sync,
    .close = fatfs_close,
    .delete = fatfs_delete,
    .rename = fatfs_rename,
    .stat = fatfs_stat,
};
