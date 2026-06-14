# VFS Design

## Purpose

Add a project-local virtual file system layer for Y-Trace firmware. The VFS provides one file API for application code while allowing storage backends such as SD card FatFs and internal flash littlefs to be registered behind mount points.

This design does not depend on RT-Thread DFS because the project uses RT-Thread Nano. It also does not include track, GPX, binary snapshot parsing, or other business-format conversion.

## Scope

In scope:

- A lightweight VFS API for open, read, write, close, delete, rename, stat, and sync.
- Backend registration and mount-point dispatch.
- `/sd` backed by FatFs.
- `/flash` backed by littlefs.
- Optional cache mode selected by `VFS_O_CACHE`.
- Cache registration through a normal configuration structure.
- Background cache submission of raw bytes to the final target path.

Out of scope:

- RT-Thread DFS/POSIX integration.
- GPX export or binary snapshot parsing.
- Business-specific file formats.
- Random-write cache consistency.
- Transparent conversion during cache flush.

## Public API Shape

The VFS exposes a single API family:

```c
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
```

`vfs_write()` remains the only write function. Different behavior is selected with flags passed to `vfs_open()`.

## Path Limits

```c
#define VFS_PATH_MAX 64
```

`VFS_PATH_MAX` is the total path buffer size including the trailing null byte. The longest accepted path string is therefore 63 bytes. Path construction must fail with `VFS_ERR_PATH_TOO_LONG` instead of truncating.

Examples of expected short paths:

- `/sd/t/0001.bin`
- `/sd/log/0001.txt`
- `/flash/cfg.bin`
- `/flash/.vc/0001.o`

## Open Flags

File behavior flags:

```c
#define VFS_O_READ   (1U << 0)
#define VFS_O_WRITE  (1U << 1)
#define VFS_O_CREATE (1U << 2)
#define VFS_O_TRUNC  (1U << 3)
#define VFS_O_APPEND (1U << 4)
```

Policy flags:

```c
#define VFS_O_CACHE  (1U << 16)
```

`VFS_O_CREATE` means the final target file may be created if it does not exist. It does not conflict with `VFS_O_CACHE`.

`VFS_O_CACHE` means the opened file uses the registered cache policy. It does not mean "write to flash only when SD fails." The final path passed to `vfs_open()` remains the target path, and cache internals decide where temporary data is stored and when it is submitted.

If `VFS_O_CACHE` is passed before a cache is registered, `vfs_open()` returns `VFS_ERR_NOT_SUPPORTED`.

## Backend Registration

Backends are registered by name and mounted under a path:

```c
vfs_register_backend(&vfs_fatfs_backend);
vfs_register_backend(&vfs_littlefs_backend);

vfs_mount("/sd", "fatfs", &sd_ctx);
vfs_mount("/flash", "littlefs", &flash_ctx);
```

The VFS core uses longest-prefix matching to select a mount. It strips the mount prefix before calling the backend. For example, `/sd/log/a.txt` becomes `/log/a.txt` for the FatFs backend.

The VFS core must not hard-code SD or flash behavior.

## Cache Registration

Cache support is registered through a configuration structure:

```c
typedef struct {
    const char *root_path;
    uint32_t segment_size;
    uint8_t high_watermark_percent;
    uint8_t critical_watermark_percent;
    uint8_t worker_priority;
    uint16_t worker_stack_size;
} vfs_cache_config_t;
```

Example:

```c
static const vfs_cache_config_t cache_cfg = {
    .root_path = "/flash/.vc",
    .segment_size = 4096U,
    .high_watermark_percent = 80U,
    .critical_watermark_percent = 95U,
    .worker_priority = RT_THREAD_PRIORITY_MAX - 3,
    .worker_stack_size = 1024U,
};

vfs_cache_register(&cache_cfg);
```

`root_path` is the cache root directory, not a final target file path.

## Cached File Semantics

For:

```c
vfs_open(&file, "/sd/cache.txt", VFS_O_WRITE | VFS_O_CREATE | VFS_O_CACHE);
```

The semantics are:

- `/sd/cache.txt` is the final target path.
- Cache files are internal implementation details under `root_path`.
- The cache layer stores raw bytes only.
- Cache flush submits those raw bytes to the final target path.
- No format conversion is hidden in VFS.

Internal cache files should use short generated names such as:

- `/flash/.vc/0001.o` for the currently open segment.
- `/flash/.vc/0001.s` for a sealed segment ready to submit.
- `/flash/.vc/0001.m` for metadata.

Metadata records the final path, flags needed to open the final target, sequence number, length, and integrity fields if implemented.

## Cache Flush Triggers

The cache worker may be woken by:

- Cache usage crossing the high watermark.
- `vfs_sync()`.
- `vfs_close()`.
- Periodic worker wakeup.

`vfs_write()` should not perform heavy submission work. It writes to the cache path and wakes the worker when needed.

At the critical watermark, `vfs_write()` may return a space error instead of claiming success. The VFS cannot guarantee non-blocking, never-fail writes when physical flash space is exhausted.

## Error Semantics

VFS errors use a dedicated negative range so they do not overlap RT-Thread errors such as `-RT_ERROR`, `-RT_ETIMEOUT`, or `-RT_EIO`. Backend-specific errors must be mapped to these VFS errors before returning through the public VFS API.

```c
#define VFS_OK                  0
#define VFS_ERR_BASE           -1000
#define VFS_ERR_INVALID        (VFS_ERR_BASE - 1)
#define VFS_ERR_NOT_FOUND      (VFS_ERR_BASE - 2)
#define VFS_ERR_IO             (VFS_ERR_BASE - 3)
#define VFS_ERR_NO_SPACE       (VFS_ERR_BASE - 4)
#define VFS_ERR_BUSY           (VFS_ERR_BASE - 5)
#define VFS_ERR_NOT_SUPPORTED  (VFS_ERR_BASE - 6)
#define VFS_ERR_PATH_TOO_LONG  (VFS_ERR_BASE - 7)
```

For cached writes, success means bytes were accepted by the cache layer or written to the selected backend according to the active policy. It does not mean background submission to the final path has already completed unless `vfs_sync()` reports success after a synchronous flush.

## First-Version Restrictions

- Cached files support sequential write and append-style workloads only.
- Cached random overwrite is not supported.
- `VFS_O_CACHE` is intended for writable final targets.
- VFS does not provide business-specific data conversion.
- VFS does not expose cache temporary paths as user-visible file paths.
- VFS stat on a final path checks the final path. Cache backlog should be exposed through a separate cache query API later if needed.

## Initialization

Storage initialization can be registered through an RT-Thread init hook:

```c
static int storage_init(void)
{
    int ret = vfs_init();
    if (ret != VFS_OK) {
        return ret;
    }

    vfs_register_backend(&vfs_fatfs_backend);
    vfs_register_backend(&vfs_littlefs_backend);

    vfs_mount("/sd", "fatfs", &sd_ctx);
    vfs_mount("/flash", "littlefs", &flash_ctx);
    vfs_cache_register(&cache_cfg);

    return VFS_OK;
}
INIT_COMPONENT_EXPORT(storage_init);
```

Exact backend contexts and mount timing may be adjusted during implementation based on SD card and littlefs initialization requirements.

## Validation Plan

- Host-buildable unit tests for path matching, path length rejection, flag validation, and backend dispatch.
- Mock backend tests for open/read/write/close/delete/rename/stat/sync forwarding.
- Mock cache tests for `VFS_O_CACHE`, missing cache registration, segment sealing, and final-path metadata.
- Firmware build with `cmake --preset debug` and `cmake --build --preset debug`.
- Hardware validation later for FatFs SD and littlefs flash once backends are implemented.
