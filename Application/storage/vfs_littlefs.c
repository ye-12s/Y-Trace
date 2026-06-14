#include "storage/vfs_littlefs.h"

#include "drivers/drv_flash.h"
#include "littlefs/lfs.h"

#include <stddef.h>
#include <string.h>

#define VFS_LITTLEFS_MAX_OPEN_FILES 2U
#define VFS_LITTLEFS_READ_SIZE      16U
#define VFS_LITTLEFS_PROG_SIZE      16U
#define VFS_LITTLEFS_CACHE_SIZE     64U
#define VFS_LITTLEFS_LOOKAHEAD_SIZE 32U
#define VFS_LITTLEFS_BLOCK_CYCLES   500

typedef struct {
    uint8_t in_use;
    lfs_file_t file;
} vfs_littlefs_file_t;

static lfs_t littlefs;
static vfs_littlefs_file_t littlefs_files[VFS_LITTLEFS_MAX_OPEN_FILES];
static uint8_t littlefs_read_buffer[VFS_LITTLEFS_CACHE_SIZE];
static uint8_t littlefs_prog_buffer[VFS_LITTLEFS_CACHE_SIZE];
static uint8_t littlefs_lookahead_buffer[VFS_LITTLEFS_LOOKAHEAD_SIZE];

static int littlefs_flash_read(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    uint32_t addr = FLASH_FILESYSTEM_START_ADDR + (uint32_t)block * cfg->block_size + off;
    return drv_flash_read(addr, (uint8_t *)buffer, size) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

static int littlefs_flash_prog(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    uint32_t addr = FLASH_FILESYSTEM_START_ADDR + (uint32_t)block * cfg->block_size + off;
    return drv_flash_write_nocheck(addr, (uint8_t *)buffer, size) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

static int littlefs_flash_erase(const struct lfs_config *cfg, lfs_block_t block)
{
    uint32_t addr = FLASH_FILESYSTEM_START_ADDR + (uint32_t)block * cfg->block_size;
    return drv_flash_erase_sector(addr) == 0 ? LFS_ERR_OK : LFS_ERR_IO;
}

static int littlefs_flash_sync(const struct lfs_config *cfg)
{
    (void)cfg;
    return LFS_ERR_OK;
}

static const struct lfs_config littlefs_config = {
    .read             = littlefs_flash_read,
    .prog             = littlefs_flash_prog,
    .erase            = littlefs_flash_erase,
    .sync             = littlefs_flash_sync,
    .read_size        = VFS_LITTLEFS_READ_SIZE,
    .prog_size        = VFS_LITTLEFS_PROG_SIZE,
    .block_size       = SECTOR_SIZE,
    .block_count      = FLASH_FILESYSTEM_SIZE / SECTOR_SIZE,
    .block_cycles     = VFS_LITTLEFS_BLOCK_CYCLES,
    .cache_size       = VFS_LITTLEFS_CACHE_SIZE,
    .lookahead_size   = VFS_LITTLEFS_LOOKAHEAD_SIZE,
    .read_buffer      = littlefs_read_buffer,
    .prog_buffer      = littlefs_prog_buffer,
    .lookahead_buffer = littlefs_lookahead_buffer,
};

static int littlefs_result_to_vfs(int result)
{
    switch (result) {
        case LFS_ERR_OK:
            return VFS_OK;
        case LFS_ERR_NOENT:
            return VFS_ERR_NOT_FOUND;
        case LFS_ERR_NOSPC:
        case LFS_ERR_NOMEM:
            return VFS_ERR_NO_SPACE;
        case LFS_ERR_EXIST:
        case LFS_ERR_NOTEMPTY:
            return VFS_ERR_BUSY;
        case LFS_ERR_INVAL:
        case LFS_ERR_BADF:
        case LFS_ERR_NAMETOOLONG:
            return VFS_ERR_INVALID;
        default:
            return VFS_ERR_IO;
    }
}

static int littlefs_flags_from_vfs(uint32_t flags)
{
    int mode = 0;

    if ((flags & VFS_O_READ) != 0U && (flags & VFS_O_WRITE) != 0U) {
        mode |= LFS_O_RDWR;
    } else if ((flags & VFS_O_WRITE) != 0U) {
        mode |= LFS_O_WRONLY;
    } else {
        mode |= LFS_O_RDONLY;
    }

    if ((flags & VFS_O_CREATE) != 0U) {
        mode |= LFS_O_CREAT;
    }
    if ((flags & VFS_O_TRUNC) != 0U) {
        mode |= LFS_O_TRUNC | LFS_O_CREAT;
    }
    if ((flags & VFS_O_APPEND) != 0U) {
        mode |= LFS_O_APPEND;
    }

    return mode;
}

static vfs_littlefs_file_t *littlefs_alloc_file(void)
{
    for (uint32_t i = 0U; i < VFS_LITTLEFS_MAX_OPEN_FILES; i++) {
        if (littlefs_files[i].in_use == 0U) {
            littlefs_files[i].in_use = 1U;
            return &littlefs_files[i];
        }
    }
    return NULL;
}

static int littlefs_mount(void *ctx)
{
    int result;
    (void)ctx;

    memset(littlefs_files, 0, sizeof(littlefs_files));
    result = lfs_mount(&littlefs, &littlefs_config);
    if (result == LFS_ERR_CORRUPT) {
        result = lfs_format(&littlefs, &littlefs_config);
        if (result != LFS_ERR_OK) {
            return littlefs_result_to_vfs(result);
        }
        result = lfs_mount(&littlefs, &littlefs_config);
    }

    return littlefs_result_to_vfs(result);
}

static int littlefs_open(void *ctx, vfs_file_t *file, const char *path, uint32_t flags)
{
    vfs_littlefs_file_t *slot = littlefs_alloc_file();
    int result;
    (void)ctx;

    if (slot == NULL) {
        return VFS_ERR_BUSY;
    }

    memset(&slot->file, 0, sizeof(slot->file));
    result = lfs_file_open(&littlefs, &slot->file, path, littlefs_flags_from_vfs(flags));
    if (result != LFS_ERR_OK) {
        slot->in_use = 0U;
        return littlefs_result_to_vfs(result);
    }

    file->backend_file = slot;
    return VFS_OK;
}

static int littlefs_read(vfs_file_t *file, void *buffer, uint32_t size, uint32_t *bytes_read)
{
    vfs_littlefs_file_t *slot = (vfs_littlefs_file_t *)file->backend_file;
    lfs_ssize_t result        = lfs_file_read(&littlefs, &slot->file, buffer, size);
    if (result < 0) {
        *bytes_read = 0U;
        return littlefs_result_to_vfs((int)result);
    }
    *bytes_read = (uint32_t)result;
    return VFS_OK;
}

static int littlefs_write(vfs_file_t *file, const void *buffer, uint32_t size, uint32_t *bytes_written)
{
    vfs_littlefs_file_t *slot = (vfs_littlefs_file_t *)file->backend_file;
    lfs_ssize_t result        = lfs_file_write(&littlefs, &slot->file, buffer, size);
    if (result < 0) {
        *bytes_written = 0U;
        return littlefs_result_to_vfs((int)result);
    }
    *bytes_written = (uint32_t)result;
    return VFS_OK;
}

static int littlefs_sync(vfs_file_t *file)
{
    vfs_littlefs_file_t *slot = (vfs_littlefs_file_t *)file->backend_file;
    return littlefs_result_to_vfs(lfs_file_sync(&littlefs, &slot->file));
}

static int littlefs_close(vfs_file_t *file)
{
    vfs_littlefs_file_t *slot = (vfs_littlefs_file_t *)file->backend_file;
    int ret                   = littlefs_result_to_vfs(lfs_file_close(&littlefs, &slot->file));
    slot->in_use              = 0U;
    file->backend_file        = NULL;
    return ret;
}

static int littlefs_delete(void *ctx, const char *path)
{
    (void)ctx;
    return littlefs_result_to_vfs(lfs_remove(&littlefs, path));
}

static int littlefs_rename(void *ctx, const char *old_path, const char *new_path)
{
    (void)ctx;
    return littlefs_result_to_vfs(lfs_rename(&littlefs, old_path, new_path));
}

static int littlefs_stat(void *ctx, const char *path, vfs_stat_t *stat)
{
    struct lfs_info info;
    int result;
    (void)ctx;

    result = lfs_stat(&littlefs, path, &info);
    if (result != LFS_ERR_OK) {
        return littlefs_result_to_vfs(result);
    }

    stat->size   = (uint32_t)info.size;
    stat->is_dir = info.type == LFS_TYPE_DIR ? 1U : 0U;
    return VFS_OK;
}

const vfs_backend_t vfs_littlefs_backend = {
    .name   = "littlefs",
    .mount  = littlefs_mount,
    .open   = littlefs_open,
    .read   = littlefs_read,
    .write  = littlefs_write,
    .sync   = littlefs_sync,
    .close  = littlefs_close,
    .delete = littlefs_delete,
    .rename = littlefs_rename,
    .stat   = littlefs_stat,
};
