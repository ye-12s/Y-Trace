#ifndef Y_TRACE_VFS_H
#define Y_TRACE_VFS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VFS_PATH_MAX 64U

#define VFS_OK 0
#define VFS_ERR_BASE (-1000)
#define VFS_ERR_INVALID (VFS_ERR_BASE - 1)
#define VFS_ERR_NOT_FOUND (VFS_ERR_BASE - 2)
#define VFS_ERR_IO (VFS_ERR_BASE - 3)
#define VFS_ERR_NO_SPACE (VFS_ERR_BASE - 4)
#define VFS_ERR_BUSY (VFS_ERR_BASE - 5)
#define VFS_ERR_NOT_SUPPORTED (VFS_ERR_BASE - 6)
#define VFS_ERR_PATH_TOO_LONG (VFS_ERR_BASE - 7)

#define VFS_O_READ (1UL << 0)
#define VFS_O_WRITE (1UL << 1)
#define VFS_O_CREATE (1UL << 2)
#define VFS_O_TRUNC (1UL << 3)
#define VFS_O_APPEND (1UL << 4)
#define VFS_O_CACHE (1UL << 16)

typedef struct vfs_backend vfs_backend_t;

typedef struct {
    uint32_t size;
    uint8_t is_dir;
} vfs_stat_t;

typedef struct {
    const char *root_path;
    uint32_t segment_size;
    uint8_t high_watermark_percent;
    uint8_t critical_watermark_percent;
    uint8_t worker_priority;
    uint16_t worker_stack_size;
} vfs_cache_config_t;

typedef struct vfs_file {
    void *backend_file;
    void *backend_ctx;
    const vfs_backend_t *backend;
    uint32_t flags;
    uint8_t cached;
    char path[VFS_PATH_MAX];
} vfs_file_t;

struct vfs_backend {
    const char *name;
    int (*mount)(void *ctx);
    int (*open)(void *ctx, vfs_file_t *file, const char *path, uint32_t flags);
    int (*read)(vfs_file_t *file, void *buffer, uint32_t size, uint32_t *bytes_read);
    int (*write)(vfs_file_t *file, const void *buffer, uint32_t size, uint32_t *bytes_written);
    int (*sync)(vfs_file_t *file);
    int (*close)(vfs_file_t *file);
    int (*delete)(void *ctx, const char *path);
    int (*rename)(void *ctx, const char *old_path, const char *new_path);
    int (*stat)(void *ctx, const char *path, vfs_stat_t *stat);
};

int vfs_init(void);
int vfs_register_backend(const vfs_backend_t *backend);
int vfs_mount(const char *mount_path, const char *backend_name, void *backend_ctx);
int vfs_cache_register(const vfs_cache_config_t *config);

int vfs_open(vfs_file_t *file, const char *path, uint32_t flags);
int vfs_read(vfs_file_t *file, void *buffer, uint32_t size, uint32_t *bytes_read);
int vfs_write(vfs_file_t *file, const void *buffer, uint32_t size, uint32_t *bytes_written);
int vfs_sync(vfs_file_t *file);
int vfs_close(vfs_file_t *file);

int vfs_delete(const char *path);
int vfs_rename(const char *old_path, const char *new_path);
int vfs_stat(const char *path, vfs_stat_t *stat);

#ifdef __cplusplus
}
#endif

#endif
