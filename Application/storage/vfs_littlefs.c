#include "storage/vfs_littlefs.h"

static int littlefs_not_ready_open(void *ctx, vfs_file_t *file, const char *path, uint32_t flags)
{
    (void)ctx;
    (void)file;
    (void)path;
    (void)flags;
    return VFS_ERR_NOT_SUPPORTED;
}

static int littlefs_not_ready_delete(void *ctx, const char *path)
{
    (void)ctx;
    (void)path;
    return VFS_ERR_NOT_SUPPORTED;
}

static int littlefs_not_ready_rename(void *ctx, const char *old_path, const char *new_path)
{
    (void)ctx;
    (void)old_path;
    (void)new_path;
    return VFS_ERR_NOT_SUPPORTED;
}

static int littlefs_not_ready_stat(void *ctx, const char *path, vfs_stat_t *stat)
{
    (void)ctx;
    (void)path;
    (void)stat;
    return VFS_ERR_NOT_SUPPORTED;
}

const vfs_backend_t vfs_littlefs_backend = {
    .name = "littlefs",
    .open = littlefs_not_ready_open,
    .delete = littlefs_not_ready_delete,
    .rename = littlefs_not_ready_rename,
    .stat = littlefs_not_ready_stat,
};
