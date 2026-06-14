#include "storage/vfs_cache.h"

#include <stdio.h>
#include <string.h>

#define VFS_CACHE_MAX_OPEN_FILES 2U

typedef struct {
    vfs_cache_config_t config;
    uint32_t next_id;
} vfs_cache_state_t;

typedef struct {
    uint8_t in_use;
    vfs_file_t segment;
    char open_path[VFS_PATH_MAX];
    char sealed_path[VFS_PATH_MAX];
    char final_path[VFS_PATH_MAX];
    uint32_t flags;
} vfs_cache_file_t;

static vfs_cache_state_t cache_state;
static vfs_cache_file_t cache_files[VFS_CACHE_MAX_OPEN_FILES];

static int copy_checked(char dest[VFS_PATH_MAX], const char *src)
{
    if (src == NULL || src[0] != '/' || strnlen(src, VFS_PATH_MAX) >= VFS_PATH_MAX) {
        return VFS_ERR_INVALID;
    }
    strcpy(dest, src);
    return VFS_OK;
}

static int make_segment_path(char dest[VFS_PATH_MAX], const char *root, uint32_t id, char suffix)
{
    int written = snprintf(dest, VFS_PATH_MAX, "%s/%04lu.%c", root, (unsigned long)id, suffix);
    if (written < 0 || written >= (int)VFS_PATH_MAX) {
        return VFS_ERR_PATH_TOO_LONG;
    }
    return VFS_OK;
}

static vfs_cache_file_t *alloc_cache_file(void)
{
    for (uint32_t i = 0U; i < VFS_CACHE_MAX_OPEN_FILES; i++) {
        if (cache_files[i].in_use == 0U) {
            cache_files[i].in_use = 1U;
            return &cache_files[i];
        }
    }
    return NULL;
}

int vfs_cache_init(void)
{
    memset(&cache_state, 0, sizeof(cache_state));
    memset(cache_files, 0, sizeof(cache_files));
    cache_state.next_id = 1U;
    return VFS_OK;
}

int vfs_cache_register_config(const vfs_cache_config_t *config)
{
    if (config == NULL || config->root_path == NULL || config->root_path[0] != '/' ||
        config->segment_size == 0U || config->high_watermark_percent >= config->critical_watermark_percent ||
        config->critical_watermark_percent > 100U) {
        return VFS_ERR_INVALID;
    }
    if (strnlen(config->root_path, VFS_PATH_MAX) >= VFS_PATH_MAX) {
        return VFS_ERR_PATH_TOO_LONG;
    }
    cache_state.config = *config;
    return VFS_OK;
}

int vfs_cache_open(vfs_file_t *file, const char *final_path, uint32_t flags)
{
    vfs_cache_file_t *cached = NULL;
    uint32_t id = 0U;
    int ret = VFS_OK;

    if (file == NULL) {
        return VFS_ERR_INVALID;
    }
    if (cache_state.config.root_path == NULL) {
        return VFS_ERR_NOT_SUPPORTED;
    }
    id = cache_state.next_id++;

    cached = alloc_cache_file();
    if (cached == NULL) {
        return VFS_ERR_BUSY;
    }
    memset(cached, 0, sizeof(*cached));
    cached->in_use = 1U;

    ret = copy_checked(cached->final_path, final_path);
    if (ret != VFS_OK) {
        cached->in_use = 0U;
        return ret;
    }
    ret = make_segment_path(cached->open_path, cache_state.config.root_path, id, 'o');
    if (ret != VFS_OK) {
        cached->in_use = 0U;
        return ret;
    }
    ret = make_segment_path(cached->sealed_path, cache_state.config.root_path, id, 's');
    if (ret != VFS_OK) {
        cached->in_use = 0U;
        return ret;
    }

    uint32_t segment_flags = flags & (VFS_O_READ | VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC | VFS_O_APPEND);
    segment_flags |= VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC;
    ret = vfs_open(&cached->segment, cached->open_path, segment_flags);
    if (ret != VFS_OK) {
        cached->in_use = 0U;
        return ret;
    }

    cached->flags = flags;
    memset(file, 0, sizeof(*file));
    file->cached = 1U;
    file->backend_file = cached;
    file->flags = flags;
    strcpy(file->path, final_path);
    return VFS_OK;
}

int vfs_cache_write(vfs_file_t *file, const void *buffer, uint32_t size, uint32_t *bytes_written)
{
    vfs_cache_file_t *cached = file == NULL ? NULL : (vfs_cache_file_t *)file->backend_file;
    if (cached == NULL) {
        return VFS_ERR_INVALID;
    }
    return vfs_write(&cached->segment, buffer, size, bytes_written);
}

int vfs_cache_sync(vfs_file_t *file)
{
    vfs_cache_file_t *cached = file == NULL ? NULL : (vfs_cache_file_t *)file->backend_file;
    if (cached == NULL) {
        return VFS_ERR_INVALID;
    }
    return vfs_sync(&cached->segment);
}

int vfs_cache_close(vfs_file_t *file)
{
    vfs_cache_file_t *cached = file == NULL ? NULL : (vfs_cache_file_t *)file->backend_file;
    int ret = VFS_OK;
    if (cached == NULL) {
        return VFS_ERR_INVALID;
    }
    ret = vfs_close(&cached->segment);
    if (ret == VFS_OK) {
        ret = vfs_rename(cached->open_path, cached->sealed_path);
    }
    cached->in_use = 0U;
    file->backend_file = NULL;
    file->cached = 0U;
    return ret;
}
