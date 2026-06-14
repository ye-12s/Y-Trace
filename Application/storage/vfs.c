#include "storage/vfs.h"

#include <stddef.h>
#include <string.h>

#define VFS_MAX_BACKENDS 4U
#define VFS_MAX_MOUNTS 8U

typedef struct {
    char path[VFS_PATH_MAX];
    const vfs_backend_t *backend;
    void *ctx;
} vfs_mount_t;

static const vfs_backend_t *vfs_backends[VFS_MAX_BACKENDS];
static vfs_mount_t vfs_mounts[VFS_MAX_MOUNTS];
static uint8_t vfs_backend_count;
static uint8_t vfs_mount_count;

static int vfs_validate_path(const char *path)
{
    if (path == NULL || path[0] != '/') {
        return VFS_ERR_INVALID;
    }
    if (strnlen(path, VFS_PATH_MAX) >= VFS_PATH_MAX) {
        return VFS_ERR_PATH_TOO_LONG;
    }
    return VFS_OK;
}

static int vfs_copy_path(char dest[VFS_PATH_MAX], const char *src)
{
    int ret = vfs_validate_path(src);
    if (ret != VFS_OK) {
        return ret;
    }
    strcpy(dest, src);
    return VFS_OK;
}

static const vfs_backend_t *vfs_find_backend(const char *name)
{
    for (uint8_t i = 0U; i < vfs_backend_count; i++) {
        if (strcmp(vfs_backends[i]->name, name) == 0) {
            return vfs_backends[i];
        }
    }
    return NULL;
}

static uint8_t vfs_mount_matches(const char *mount_path, const char *path)
{
    size_t len = strlen(mount_path);
    if (strncmp(path, mount_path, len) != 0) {
        return 0U;
    }
    return path[len] == '\0' || path[len] == '/';
}

static const vfs_mount_t *vfs_find_mount(const char *path)
{
    const vfs_mount_t *best = NULL;
    size_t best_len = 0U;
    for (uint8_t i = 0U; i < vfs_mount_count; i++) {
        size_t len = strlen(vfs_mounts[i].path);
        if (len > best_len && vfs_mount_matches(vfs_mounts[i].path, path)) {
            best = &vfs_mounts[i];
            best_len = len;
        }
    }
    return best;
}

static const char *vfs_backend_path(const vfs_mount_t *mount, const char *path)
{
    const char *suffix = path + strlen(mount->path);
    return suffix[0] == '\0' ? "/" : suffix;
}

int vfs_init(void)
{
    memset(vfs_backends, 0, sizeof(vfs_backends));
    memset(vfs_mounts, 0, sizeof(vfs_mounts));
    vfs_backend_count = 0U;
    vfs_mount_count = 0U;
    return VFS_OK;
}

int vfs_register_backend(const vfs_backend_t *backend)
{
    if (backend == NULL || backend->name == NULL || backend->open == NULL) {
        return VFS_ERR_INVALID;
    }
    if (vfs_backend_count >= VFS_MAX_BACKENDS) {
        return VFS_ERR_NO_SPACE;
    }
    vfs_backends[vfs_backend_count++] = backend;
    return VFS_OK;
}

int vfs_mount(const char *mount_path, const char *backend_name, void *backend_ctx)
{
    const vfs_backend_t *backend = NULL;
    int ret = vfs_validate_path(mount_path);
    if (ret != VFS_OK) {
        return ret;
    }
    if (backend_name == NULL || vfs_mount_count >= VFS_MAX_MOUNTS) {
        return VFS_ERR_INVALID;
    }
    backend = vfs_find_backend(backend_name);
    if (backend == NULL) {
        return VFS_ERR_NOT_FOUND;
    }
    ret = vfs_copy_path(vfs_mounts[vfs_mount_count].path, mount_path);
    if (ret != VFS_OK) {
        return ret;
    }
    vfs_mounts[vfs_mount_count].backend = backend;
    vfs_mounts[vfs_mount_count].ctx = backend_ctx;
    if (backend->mount != NULL) {
        ret = backend->mount(backend_ctx);
        if (ret != VFS_OK) {
            return ret;
        }
    }
    vfs_mount_count++;
    return VFS_OK;
}

int vfs_cache_register(const vfs_cache_config_t *config)
{
    (void)config;
    return VFS_ERR_NOT_SUPPORTED;
}

int vfs_open(vfs_file_t *file, const char *path, uint32_t flags)
{
    const vfs_mount_t *mount = NULL;
    int ret = VFS_OK;
    if (file == NULL) {
        return VFS_ERR_INVALID;
    }
    ret = vfs_validate_path(path);
    if (ret != VFS_OK) {
        return ret;
    }
    if ((flags & VFS_O_CACHE) != 0U) {
        return VFS_ERR_NOT_SUPPORTED;
    }
    mount = vfs_find_mount(path);
    if (mount == NULL) {
        return VFS_ERR_NOT_FOUND;
    }
    memset(file, 0, sizeof(*file));
    file->backend = mount->backend;
    file->backend_ctx = mount->ctx;
    file->flags = flags;
    ret = vfs_copy_path(file->path, path);
    if (ret != VFS_OK) {
        return ret;
    }
    return mount->backend->open(mount->ctx, file, vfs_backend_path(mount, path), flags);
}

int vfs_read(vfs_file_t *file, void *buffer, uint32_t size, uint32_t *bytes_read)
{
    if (file == NULL || file->backend == NULL || file->backend->read == NULL || bytes_read == NULL) {
        return VFS_ERR_INVALID;
    }
    return file->backend->read(file, buffer, size, bytes_read);
}

int vfs_write(vfs_file_t *file, const void *buffer, uint32_t size, uint32_t *bytes_written)
{
    if (file == NULL || file->backend == NULL || file->backend->write == NULL || bytes_written == NULL) {
        return VFS_ERR_INVALID;
    }
    return file->backend->write(file, buffer, size, bytes_written);
}

int vfs_sync(vfs_file_t *file)
{
    if (file == NULL || file->backend == NULL) {
        return VFS_ERR_INVALID;
    }
    return file->backend->sync == NULL ? VFS_OK : file->backend->sync(file);
}

int vfs_close(vfs_file_t *file)
{
    if (file == NULL || file->backend == NULL) {
        return VFS_ERR_INVALID;
    }
    return file->backend->close == NULL ? VFS_OK : file->backend->close(file);
}

int vfs_delete(const char *path)
{
    const vfs_mount_t *mount = NULL;
    int ret = vfs_validate_path(path);
    if (ret != VFS_OK) {
        return ret;
    }
    mount = vfs_find_mount(path);
    if (mount == NULL || mount->backend->delete == NULL) {
        return VFS_ERR_NOT_FOUND;
    }
    return mount->backend->delete(mount->ctx, vfs_backend_path(mount, path));
}

int vfs_rename(const char *old_path, const char *new_path)
{
    const vfs_mount_t *old_mount = NULL;
    const vfs_mount_t *new_mount = NULL;
    int ret = vfs_validate_path(old_path);
    if (ret != VFS_OK) {
        return ret;
    }
    ret = vfs_validate_path(new_path);
    if (ret != VFS_OK) {
        return ret;
    }
    old_mount = vfs_find_mount(old_path);
    new_mount = vfs_find_mount(new_path);
    if (old_mount == NULL || new_mount == NULL || old_mount != new_mount || old_mount->backend->rename == NULL) {
        return VFS_ERR_NOT_SUPPORTED;
    }
    return old_mount->backend->rename(old_mount->ctx, vfs_backend_path(old_mount, old_path), vfs_backend_path(new_mount, new_path));
}

int vfs_stat(const char *path, vfs_stat_t *stat)
{
    const vfs_mount_t *mount = NULL;
    int ret = vfs_validate_path(path);
    if (ret != VFS_OK) {
        return ret;
    }
    if (stat == NULL) {
        return VFS_ERR_INVALID;
    }
    mount = vfs_find_mount(path);
    if (mount == NULL || mount->backend->stat == NULL) {
        return VFS_ERR_NOT_FOUND;
    }
    return mount->backend->stat(mount->ctx, vfs_backend_path(mount, path), stat);
}
