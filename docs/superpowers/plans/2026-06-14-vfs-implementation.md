# VFS Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a project-local VFS with registered backends, `/sd` and `/flash` mount dispatch, and an optional registered raw-byte cache policy.

**Architecture:** Implement a small host-testable VFS core under `Application/storage/`, with backend registration and longest-prefix mount dispatch in `vfs.c`. Add cache policy support as a separate module that creates internal cache segments under a registered root path; cache submission remains raw bytes only and contains no track/GPX logic. Add firmware backend adapters for FatFs and littlefs after the core semantics are locked by host Unity tests.

**Tech Stack:** C11, CMake, Unity host tests, RT-Thread Nano init hooks, FatFs, littlefs.

---

## File Structure

- Create `Application/storage/vfs.h`: public constants, flags, errors, file/stat/backend/cache types, and public API declarations.
- Create `Application/storage/vfs.c`: VFS initialization, backend registry, mount table, path validation, direct dispatch, and cache-mode dispatch entry points.
- Create `Application/storage/vfs_cache.h`: private cache API used by `vfs.c`.
- Create `Application/storage/vfs_cache.c`: cache registration, cached open/write/sync/close, segment naming, and worker wake/pump hooks.
- Create `Application/storage/vfs_fatfs.h`: FatFs backend declaration.
- Create `Application/storage/vfs_fatfs.c`: FatFs backend implementation for `/sd`.
- Create `Application/storage/vfs_littlefs.h`: littlefs backend declaration.
- Create `Application/storage/vfs_littlefs.c`: littlefs backend implementation for `/flash`.
- Create `Application/storage/storage_init.c`: firmware init hook that registers backends, mounts paths, and registers the cache config.
- Create `Test/test_vfs_core.c`: host tests for error values, path validation, backend registration, mount dispatch, and direct I/O forwarding.
- Create `Test/test_vfs_cache.c`: host tests for cache registration, `VFS_O_CACHE` behavior, segment writes, close sealing, and unsupported-cache errors.
- Modify `Test/CMakeLists.txt`: add host test executables for VFS core/cache with mock backends.
- Modify `cmake/sources.cmake`: include VFS sources in firmware build.

## Task 1: Public API Contract And Host Test Lane

**Files:**
- Create: `Application/storage/vfs.h`
- Create: `Test/test_vfs_core.c`
- Modify: `Test/CMakeLists.txt`

- [ ] **Step 1: Write the public header**

Create `Application/storage/vfs.h`:

```c
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
```

- [ ] **Step 2: Write the initial contract test**

Create `Test/test_vfs_core.c`:

```c
#include "storage/vfs.h"
#include "unity.h"

static void test_vfs_errors_do_not_overlap_rtthread_common_negative_errors(void)
{
    TEST_ASSERT_LESS_OR_EQUAL_INT(-1000, VFS_ERR_BASE);
    TEST_ASSERT_LESS_THAN_INT(-10, VFS_ERR_INVALID);
    TEST_ASSERT_LESS_THAN_INT(-10, VFS_ERR_NOT_FOUND);
    TEST_ASSERT_LESS_THAN_INT(-10, VFS_ERR_IO);
    TEST_ASSERT_LESS_THAN_INT(-10, VFS_ERR_NO_SPACE);
    TEST_ASSERT_LESS_THAN_INT(-10, VFS_ERR_BUSY);
    TEST_ASSERT_LESS_THAN_INT(-10, VFS_ERR_NOT_SUPPORTED);
    TEST_ASSERT_LESS_THAN_INT(-10, VFS_ERR_PATH_TOO_LONG);
}

static void test_vfs_path_limit_includes_null_terminator(void)
{
    TEST_ASSERT_EQUAL_UINT32(64U, VFS_PATH_MAX);
}

static void test_vfs_cache_flag_uses_policy_bit_range(void)
{
    TEST_ASSERT_EQUAL_UINT32(1UL << 16, VFS_O_CACHE);
    TEST_ASSERT_EQUAL_UINT32(0U, VFS_O_CACHE & (VFS_O_READ | VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC | VFS_O_APPEND));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_vfs_errors_do_not_overlap_rtthread_common_negative_errors);
    RUN_TEST(test_vfs_path_limit_includes_null_terminator);
    RUN_TEST(test_vfs_cache_flag_uses_policy_bit_range);
    return UNITY_END();
}
```

- [ ] **Step 3: Add the host test executable**

Modify `Test/CMakeLists.txt` after the existing `test_ytrace_map_tiles` block:

```cmake
add_executable(test_vfs_core
    test_vfs_core.c
    ../Application/storage/vfs.c
)
target_include_directories(test_vfs_core PRIVATE ${Y_TRACE_APP_DIR})
target_compile_options(test_vfs_core PRIVATE -Wall -Wextra)
target_link_libraries(test_vfs_core PRIVATE unity)
add_test(NAME test_vfs_core COMMAND test_vfs_core)
```

- [ ] **Step 4: Run the test and verify it fails before implementation**

Run:

```sh
cmake -S Test -B build/host-test
cmake --build build/host-test --target test_vfs_core
```

Expected: build fails because `Application/storage/vfs.c` does not exist.

- [ ] **Step 5: Add a minimal source file**

Create `Application/storage/vfs.c`:

```c
#include "storage/vfs.h"
```

- [ ] **Step 6: Run the test and verify it passes**

Run:

```sh
cmake --build build/host-test --target test_vfs_core
ctest --test-dir build/host-test --output-on-failure -R test_vfs_core
```

Expected: `test_vfs_core` passes.

- [ ] **Step 7: Commit**

```bash
git add Application/storage/vfs.h Application/storage/vfs.c Test/test_vfs_core.c Test/CMakeLists.txt
git commit -m "Define VFS public API contract"
```

## Task 2: Direct Backend Registry And Mount Dispatch

**Files:**
- Modify: `Application/storage/vfs.c`
- Modify: `Test/test_vfs_core.c`

- [ ] **Step 1: Extend the host test with a mock backend**

Append these helpers near the top of `Test/test_vfs_core.c` after the includes:

```c
typedef struct {
    int mount_count;
    int open_count;
    int read_count;
    int write_count;
    int sync_count;
    int close_count;
    int delete_count;
    int rename_count;
    int stat_count;
    const char *last_path;
    const char *last_old_path;
    const char *last_new_path;
    uint32_t last_flags;
    char storage[32];
    uint32_t storage_len;
} mock_backend_ctx_t;

static int mock_mount(void *ctx)
{
    ((mock_backend_ctx_t *)ctx)->mount_count++;
    return VFS_OK;
}

static int mock_open(void *ctx, vfs_file_t *file, const char *path, uint32_t flags)
{
    mock_backend_ctx_t *mock = (mock_backend_ctx_t *)ctx;
    mock->open_count++;
    mock->last_path = path;
    mock->last_flags = flags;
    file->backend_ctx = ctx;
    return VFS_OK;
}

static int mock_read(vfs_file_t *file, void *buffer, uint32_t size, uint32_t *bytes_read)
{
    mock_backend_ctx_t *mock = (mock_backend_ctx_t *)file->backend_ctx;
    uint32_t count = mock->storage_len < size ? mock->storage_len : size;
    for (uint32_t i = 0; i < count; i++) {
        ((char *)buffer)[i] = mock->storage[i];
    }
    *bytes_read = count;
    mock->read_count++;
    return VFS_OK;
}

static int mock_write(vfs_file_t *file, const void *buffer, uint32_t size, uint32_t *bytes_written)
{
    mock_backend_ctx_t *mock = (mock_backend_ctx_t *)file->backend_ctx;
    uint32_t count = size < sizeof(mock->storage) ? size : sizeof(mock->storage);
    for (uint32_t i = 0; i < count; i++) {
        mock->storage[i] = ((const char *)buffer)[i];
    }
    mock->storage_len = count;
    *bytes_written = count;
    mock->write_count++;
    return count == size ? VFS_OK : VFS_ERR_NO_SPACE;
}

static int mock_sync(vfs_file_t *file)
{
    ((mock_backend_ctx_t *)file->backend_ctx)->sync_count++;
    return VFS_OK;
}

static int mock_close(vfs_file_t *file)
{
    ((mock_backend_ctx_t *)file->backend_ctx)->close_count++;
    return VFS_OK;
}

static int mock_delete(void *ctx, const char *path)
{
    mock_backend_ctx_t *mock = (mock_backend_ctx_t *)ctx;
    mock->delete_count++;
    mock->last_path = path;
    return VFS_OK;
}

static int mock_rename(void *ctx, const char *old_path, const char *new_path)
{
    mock_backend_ctx_t *mock = (mock_backend_ctx_t *)ctx;
    mock->rename_count++;
    mock->last_old_path = old_path;
    mock->last_new_path = new_path;
    return VFS_OK;
}

static int mock_stat(void *ctx, const char *path, vfs_stat_t *stat)
{
    mock_backend_ctx_t *mock = (mock_backend_ctx_t *)ctx;
    mock->stat_count++;
    mock->last_path = path;
    stat->size = mock->storage_len;
    stat->is_dir = 0U;
    return VFS_OK;
}

static const vfs_backend_t mock_backend = {
    .name = "mock",
    .mount = mock_mount,
    .open = mock_open,
    .read = mock_read,
    .write = mock_write,
    .sync = mock_sync,
    .close = mock_close,
    .delete = mock_delete,
    .rename = mock_rename,
    .stat = mock_stat,
};
```

Add these tests before `main()`:

```c
static void test_vfs_direct_open_dispatches_to_longest_mount_prefix(void)
{
    mock_backend_ctx_t root_ctx = {0};
    mock_backend_ctx_t log_ctx = {0};
    vfs_file_t file = {0};

    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_init());
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_register_backend(&mock_backend));
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_mount("/sd", "mock", &root_ctx));
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_mount("/sd/log", "mock", &log_ctx));

    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_open(&file, "/sd/log/a.txt", VFS_O_WRITE | VFS_O_CREATE));

    TEST_ASSERT_EQUAL_INT(0, root_ctx.open_count);
    TEST_ASSERT_EQUAL_INT(1, log_ctx.open_count);
    TEST_ASSERT_EQUAL_STRING("/a.txt", log_ctx.last_path);
    TEST_ASSERT_EQUAL_UINT32(VFS_O_WRITE | VFS_O_CREATE, log_ctx.last_flags);
}

static void test_vfs_direct_file_operations_forward_to_backend(void)
{
    mock_backend_ctx_t ctx = {0};
    vfs_file_t file = {0};
    char read_buffer[8] = {0};
    uint32_t count = 0U;
    vfs_stat_t stat = {0};

    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_init());
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_register_backend(&mock_backend));
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_mount("/sd", "mock", &ctx));
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_open(&file, "/sd/a.txt", VFS_O_WRITE | VFS_O_CREATE));

    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_write(&file, "abc", 3U, &count));
    TEST_ASSERT_EQUAL_UINT32(3U, count);
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_sync(&file));
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_read(&file, read_buffer, sizeof(read_buffer), &count));
    TEST_ASSERT_EQUAL_UINT32(3U, count);
    TEST_ASSERT_EQUAL_MEMORY("abc", read_buffer, 3U);
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_close(&file));

    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_stat("/sd/a.txt", &stat));
    TEST_ASSERT_EQUAL_UINT32(3U, stat.size);
    TEST_ASSERT_EQUAL_STRING("/a.txt", ctx.last_path);
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_rename("/sd/a.txt", "/sd/b.txt"));
    TEST_ASSERT_EQUAL_STRING("/a.txt", ctx.last_old_path);
    TEST_ASSERT_EQUAL_STRING("/b.txt", ctx.last_new_path);
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_delete("/sd/b.txt"));
    TEST_ASSERT_EQUAL_STRING("/b.txt", ctx.last_path);
}

static void test_vfs_rejects_missing_mount_and_long_paths(void)
{
    vfs_file_t file = {0};
    const char long_path[] = "/sd/abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijk";

    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_init());
    TEST_ASSERT_EQUAL_INT(VFS_ERR_NOT_FOUND, vfs_open(&file, "/sd/missing.txt", VFS_O_READ));
    TEST_ASSERT_EQUAL_INT(VFS_ERR_PATH_TOO_LONG, vfs_open(&file, long_path, VFS_O_READ));
}
```

Add the new tests to `main()`:

```c
    RUN_TEST(test_vfs_direct_open_dispatches_to_longest_mount_prefix);
    RUN_TEST(test_vfs_direct_file_operations_forward_to_backend);
    RUN_TEST(test_vfs_rejects_missing_mount_and_long_paths);
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```sh
cmake --build build/host-test --target test_vfs_core
ctest --test-dir build/host-test --output-on-failure -R test_vfs_core
```

Expected: link fails or tests fail because the VFS functions are not implemented.

- [ ] **Step 3: Implement direct VFS core**

Replace `Application/storage/vfs.c` with:

```c
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
```

- [ ] **Step 4: Run the core host test**

Run:

```sh
cmake --build build/host-test --target test_vfs_core
ctest --test-dir build/host-test --output-on-failure -R test_vfs_core
```

Expected: `test_vfs_core` passes.

- [ ] **Step 5: Commit**

```bash
git add Application/storage/vfs.c Test/test_vfs_core.c
git commit -m "Implement VFS backend mount dispatch"
```

## Task 3: Registered Raw-Byte Cache Policy

**Files:**
- Create: `Application/storage/vfs_cache.h`
- Create: `Application/storage/vfs_cache.c`
- Modify: `Application/storage/vfs.c`
- Create: `Test/test_vfs_cache.c`
- Modify: `Test/CMakeLists.txt`

- [ ] **Step 1: Write cache tests for registration and unsupported behavior**

Create `Test/test_vfs_cache.c`:

```c
#include "storage/vfs.h"
#include "unity.h"

#include <string.h>

typedef struct {
    char last_open_path[VFS_PATH_MAX];
    char last_rename_old[VFS_PATH_MAX];
    char last_rename_new[VFS_PATH_MAX];
    char written[128];
    uint32_t written_len;
    uint8_t open_count;
    uint8_t write_count;
    uint8_t close_count;
    uint8_t rename_count;
} cache_mock_ctx_t;

static int cache_mock_open(void *ctx, vfs_file_t *file, const char *path, uint32_t flags)
{
    cache_mock_ctx_t *mock = (cache_mock_ctx_t *)ctx;
    (void)flags;
    strncpy(mock->last_open_path, path, sizeof(mock->last_open_path) - 1U);
    file->backend_ctx = ctx;
    mock->open_count++;
    return VFS_OK;
}

static int cache_mock_write(vfs_file_t *file, const void *buffer, uint32_t size, uint32_t *bytes_written)
{
    cache_mock_ctx_t *mock = (cache_mock_ctx_t *)file->backend_ctx;
    if (mock->written_len + size > sizeof(mock->written)) {
        return VFS_ERR_NO_SPACE;
    }
    memcpy(&mock->written[mock->written_len], buffer, size);
    mock->written_len += size;
    *bytes_written = size;
    mock->write_count++;
    return VFS_OK;
}

static int cache_mock_close(vfs_file_t *file)
{
    ((cache_mock_ctx_t *)file->backend_ctx)->close_count++;
    return VFS_OK;
}

static int cache_mock_rename(void *ctx, const char *old_path, const char *new_path)
{
    cache_mock_ctx_t *mock = (cache_mock_ctx_t *)ctx;
    strncpy(mock->last_rename_old, old_path, sizeof(mock->last_rename_old) - 1U);
    strncpy(mock->last_rename_new, new_path, sizeof(mock->last_rename_new) - 1U);
    mock->rename_count++;
    return VFS_OK;
}

static const vfs_backend_t cache_mock_backend = {
    .name = "mock",
    .open = cache_mock_open,
    .write = cache_mock_write,
    .close = cache_mock_close,
    .rename = cache_mock_rename,
};

static void test_cache_open_requires_registered_cache(void)
{
    cache_mock_ctx_t sd = {0};
    vfs_file_t file = {0};

    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_init());
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_register_backend(&cache_mock_backend));
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_mount("/sd", "mock", &sd));

    TEST_ASSERT_EQUAL_INT(VFS_ERR_NOT_SUPPORTED, vfs_open(&file, "/sd/a.bin", VFS_O_WRITE | VFS_O_CACHE));
}

static void test_cache_register_rejects_invalid_config(void)
{
    vfs_cache_config_t bad_root = {
        .root_path = "flash/.vc",
        .segment_size = 4096U,
        .high_watermark_percent = 80U,
        .critical_watermark_percent = 95U,
        .worker_priority = 10U,
        .worker_stack_size = 1024U,
    };

    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_init());
    TEST_ASSERT_EQUAL_INT(VFS_ERR_INVALID, vfs_cache_register(NULL));
    TEST_ASSERT_EQUAL_INT(VFS_ERR_INVALID, vfs_cache_register(&bad_root));
}

static void test_cached_write_uses_cache_root_not_final_path(void)
{
    cache_mock_ctx_t flash = {0};
    cache_mock_ctx_t sd = {0};
    vfs_cache_config_t cfg = {
        .root_path = "/flash/.vc",
        .segment_size = 4096U,
        .high_watermark_percent = 80U,
        .critical_watermark_percent = 95U,
        .worker_priority = 10U,
        .worker_stack_size = 1024U,
    };
    vfs_file_t file = {0};
    uint32_t written = 0U;

    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_init());
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_register_backend(&cache_mock_backend));
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_mount("/flash", "mock", &flash));
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_mount("/sd", "mock", &sd));
    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_cache_register(&cfg));

    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_open(&file, "/sd/a.bin", VFS_O_WRITE | VFS_O_CREATE | VFS_O_CACHE));
    TEST_ASSERT_EQUAL_STRING("/.vc/0001.o", flash.last_open_path);
    TEST_ASSERT_EQUAL_INT(0, sd.open_count);

    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_write(&file, "abc", 3U, &written));
    TEST_ASSERT_EQUAL_UINT32(3U, written);
    TEST_ASSERT_EQUAL_MEMORY("abc", flash.written, 3U);

    TEST_ASSERT_EQUAL_INT(VFS_OK, vfs_close(&file));
    TEST_ASSERT_EQUAL_STRING("/.vc/0001.o", flash.last_rename_old);
    TEST_ASSERT_EQUAL_STRING("/.vc/0001.s", flash.last_rename_new);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_cache_open_requires_registered_cache);
    RUN_TEST(test_cache_register_rejects_invalid_config);
    RUN_TEST(test_cached_write_uses_cache_root_not_final_path);
    return UNITY_END();
}
```

- [ ] **Step 2: Add cache test target**

Modify `Test/CMakeLists.txt` after `test_vfs_core`:

```cmake
target_sources(test_vfs_core PRIVATE
    ../Application/storage/vfs_cache.c
)

add_executable(test_vfs_cache
    test_vfs_cache.c
    ../Application/storage/vfs.c
    ../Application/storage/vfs_cache.c
)
target_include_directories(test_vfs_cache PRIVATE ${Y_TRACE_APP_DIR})
target_compile_options(test_vfs_cache PRIVATE -Wall -Wextra)
target_link_libraries(test_vfs_cache PRIVATE unity)
add_test(NAME test_vfs_cache COMMAND test_vfs_cache)
```

- [ ] **Step 3: Run the cache test and verify it fails**

Run:

```sh
cmake --build build/host-test --target test_vfs_cache
```

Expected: build fails because `vfs_cache.c` does not exist.

- [ ] **Step 4: Add cache private header**

Create `Application/storage/vfs_cache.h`:

```c
#ifndef Y_TRACE_VFS_CACHE_H
#define Y_TRACE_VFS_CACHE_H

#include "storage/vfs.h"

int vfs_cache_init(void);
int vfs_cache_register_config(const vfs_cache_config_t *config);
int vfs_cache_open(vfs_file_t *file, const char *final_path, uint32_t flags);
int vfs_cache_write(vfs_file_t *file, const void *buffer, uint32_t size, uint32_t *bytes_written);
int vfs_cache_sync(vfs_file_t *file);
int vfs_cache_close(vfs_file_t *file);

#endif
```

- [ ] **Step 5: Implement minimal cache segment behavior**

Create `Application/storage/vfs_cache.c`:

```c
#include "storage/vfs_cache.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t registered;
    vfs_cache_config_t config;
    uint32_t next_id;
} vfs_cache_state_t;

typedef struct {
    vfs_file_t segment;
    char open_path[VFS_PATH_MAX];
    char sealed_path[VFS_PATH_MAX];
    char final_path[VFS_PATH_MAX];
    uint32_t flags;
} vfs_cache_file_t;

static vfs_cache_state_t cache_state;
static vfs_cache_file_t cache_files[2];

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
    cache_state.registered = 1U;
    return VFS_OK;
}

int vfs_cache_open(vfs_file_t *file, const char *final_path, uint32_t flags)
{
    vfs_cache_file_t *cached = &cache_files[0];
    uint32_t id = cache_state.next_id++;
    int ret = VFS_OK;
    if (!cache_state.registered) {
        return VFS_ERR_NOT_SUPPORTED;
    }
    ret = copy_checked(cached->final_path, final_path);
    if (ret != VFS_OK) {
        return ret;
    }
    ret = make_segment_path(cached->open_path, cache_state.config.root_path, id, 'o');
    if (ret != VFS_OK) {
        return ret;
    }
    ret = make_segment_path(cached->sealed_path, cache_state.config.root_path, id, 's');
    if (ret != VFS_OK) {
        return ret;
    }
    ret = vfs_open(&cached->segment, cached->open_path, (flags & ~VFS_O_CACHE) | VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
    if (ret != VFS_OK) {
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
    vfs_cache_file_t *cached = (vfs_cache_file_t *)file->backend_file;
    if (cached == NULL) {
        return VFS_ERR_INVALID;
    }
    return vfs_write(&cached->segment, buffer, size, bytes_written);
}

int vfs_cache_sync(vfs_file_t *file)
{
    vfs_cache_file_t *cached = (vfs_cache_file_t *)file->backend_file;
    if (cached == NULL) {
        return VFS_ERR_INVALID;
    }
    return vfs_sync(&cached->segment);
}

int vfs_cache_close(vfs_file_t *file)
{
    vfs_cache_file_t *cached = (vfs_cache_file_t *)file->backend_file;
    int ret = VFS_OK;
    if (cached == NULL) {
        return VFS_ERR_INVALID;
    }
    ret = vfs_close(&cached->segment);
    if (ret != VFS_OK) {
        return ret;
    }
    return vfs_rename(cached->open_path, cached->sealed_path);
}
```

- [ ] **Step 6: Wire cache into VFS core**

Modify `Application/storage/vfs.c`:

Add include:

```c
#include "storage/vfs_cache.h"
```

In `vfs_init()`, before `return VFS_OK;`, add:

```c
    return vfs_cache_init();
```

Replace the current `return VFS_OK;` in `vfs_init()` with that line.

Replace `vfs_cache_register()` with:

```c
int vfs_cache_register(const vfs_cache_config_t *config)
{
    return vfs_cache_register_config(config);
}
```

In `vfs_open()`, replace:

```c
    if ((flags & VFS_O_CACHE) != 0U) {
        return VFS_ERR_NOT_SUPPORTED;
    }
```

with:

```c
    if ((flags & VFS_O_CACHE) != 0U) {
        return vfs_cache_open(file, path, flags);
    }
```

At the start of `vfs_read()`, before backend validation:

```c
    if (file != NULL && file->cached != 0U) {
        return VFS_ERR_NOT_SUPPORTED;
    }
```

At the start of `vfs_write()`, before backend validation:

```c
    if (file != NULL && file->cached != 0U) {
        return vfs_cache_write(file, buffer, size, bytes_written);
    }
```

At the start of `vfs_sync()`, before backend validation:

```c
    if (file != NULL && file->cached != 0U) {
        return vfs_cache_sync(file);
    }
```

At the start of `vfs_close()`, before backend validation:

```c
    if (file != NULL && file->cached != 0U) {
        return vfs_cache_close(file);
    }
```

- [ ] **Step 7: Run cache and core tests**

Run:

```sh
cmake --build build/host-test --target test_vfs_core test_vfs_cache
ctest --test-dir build/host-test --output-on-failure -R "test_vfs_(core|cache)"
```

Expected: both VFS tests pass.

- [ ] **Step 8: Commit**

```bash
git add Application/storage/vfs.c Application/storage/vfs_cache.h Application/storage/vfs_cache.c Test/test_vfs_cache.c Test/CMakeLists.txt
git commit -m "Add registered VFS raw-byte cache policy"
```

## Task 4: FatFs Backend Adapter

**Files:**
- Create: `Application/storage/vfs_fatfs.h`
- Create: `Application/storage/vfs_fatfs.c`
- Modify: `Test/test_vfs_core.c`
- Modify: `cmake/sources.cmake`

- [ ] **Step 1: Add a static contract test for the adapter boundary**

Append to `Test/test_vfs_core.c`:

```c
static void test_vfs_fatfs_backend_source_preserves_error_mapping_boundary(void)
{
    FILE *file = fopen(Y_TRACE_VFS_FATFS_SOURCE_PATH, "rb");
    char buffer[4096] = {0};
    size_t read_len = 0U;

    TEST_ASSERT_NOT_NULL(file);
    read_len = fread(buffer, 1U, sizeof(buffer) - 1U, file);
    fclose(file);

    TEST_ASSERT_GREATER_THAN_UINT32(0U, read_len);
    TEST_ASSERT_NOT_NULL(strstr(buffer, "static int fatfs_result_to_vfs"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "FR_OK"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "VFS_ERR_IO"));
}
```

Add to `main()`:

```c
    RUN_TEST(test_vfs_fatfs_backend_source_preserves_error_mapping_boundary);
```

Add includes at the top of `Test/test_vfs_core.c`:

```c
#include <stdio.h>
#include <string.h>
```

Modify the `test_vfs_core` target in `Test/CMakeLists.txt` to pass the source path:

```cmake
target_compile_definitions(test_vfs_core PRIVATE
    Y_TRACE_VFS_FATFS_SOURCE_PATH="${PROJECT_SOURCE_DIR}/../Application/storage/vfs_fatfs.c")
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```sh
cmake --build build/host-test --target test_vfs_core
ctest --test-dir build/host-test --output-on-failure -R test_vfs_core
```

Expected: test fails because `Application/storage/vfs_fatfs.c` does not exist.

- [ ] **Step 3: Add FatFs backend declaration**

Create `Application/storage/vfs_fatfs.h`:

```c
#ifndef Y_TRACE_VFS_FATFS_H
#define Y_TRACE_VFS_FATFS_H

#include "storage/vfs.h"

extern const vfs_backend_t vfs_fatfs_backend;

#endif
```

- [ ] **Step 4: Add FatFs backend implementation**

Create `Application/storage/vfs_fatfs.c`:

```c
#include "storage/vfs_fatfs.h"

#include "ff.h"

#include <string.h>

typedef struct {
    FIL fil;
} vfs_fatfs_file_t;

static vfs_fatfs_file_t fatfs_files[2];

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
            return VFS_ERR_BUSY;
        case FR_NOT_ENOUGH_CORE:
        case FR_TOO_MANY_OPEN_FILES:
            return VFS_ERR_NO_SPACE;
        case FR_INVALID_NAME:
        case FR_INVALID_PARAMETER:
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
    if ((flags & VFS_O_CREATE) != 0U) {
        mode |= FA_OPEN_ALWAYS;
    }
    if ((flags & VFS_O_TRUNC) != 0U) {
        mode |= FA_CREATE_ALWAYS;
    }
    if ((flags & VFS_O_APPEND) != 0U) {
        mode |= FA_OPEN_APPEND;
    }
    return mode;
}

static int fatfs_open(void *ctx, vfs_file_t *file, const char *path, uint32_t flags)
{
    vfs_fatfs_file_t *slot = &fatfs_files[0];
    FRESULT result;
    (void)ctx;
    memset(slot, 0, sizeof(*slot));
    result = f_open(&slot->fil, path, fatfs_mode_from_flags(flags));
    if (result != FR_OK) {
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
    return fatfs_result_to_vfs(f_close(&slot->fil));
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
    .open = fatfs_open,
    .read = fatfs_read,
    .write = fatfs_write,
    .sync = fatfs_sync,
    .close = fatfs_close,
    .delete = fatfs_delete,
    .rename = fatfs_rename,
    .stat = fatfs_stat,
};
```

- [ ] **Step 5: Add sources to firmware build**

Modify `cmake/sources.cmake` and add under `Application/port/diskio.c`:

```cmake
    Application/storage/vfs.c
    Application/storage/vfs_cache.c
    Application/storage/vfs_fatfs.c
```

- [ ] **Step 6: Run host test**

Run:

```sh
cmake --build build/host-test --target test_vfs_core
ctest --test-dir build/host-test --output-on-failure -R test_vfs_core
```

Expected: `test_vfs_core` passes.

- [ ] **Step 7: Commit**

```bash
git add Application/storage/vfs_fatfs.h Application/storage/vfs_fatfs.c Test/test_vfs_core.c cmake/sources.cmake
git commit -m "Add FatFs VFS backend adapter"
```

## Task 5: Littlefs Backend Adapter Skeleton And Storage Init

**Files:**
- Create: `Application/storage/vfs_littlefs.h`
- Create: `Application/storage/vfs_littlefs.c`
- Create: `Application/storage/storage_init.c`
- Modify: `cmake/sources.cmake`

- [ ] **Step 1: Add littlefs backend declaration**

Create `Application/storage/vfs_littlefs.h`:

```c
#ifndef Y_TRACE_VFS_LITTLEFS_H
#define Y_TRACE_VFS_LITTLEFS_H

#include "storage/vfs.h"

extern const vfs_backend_t vfs_littlefs_backend;

#endif
```

- [ ] **Step 2: Add a conservative littlefs backend skeleton**

Create `Application/storage/vfs_littlefs.c`:

```c
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
```

- [ ] **Step 3: Add storage init hook**

Create `Application/storage/storage_init.c`:

```c
#include "storage/vfs.h"
#include "storage/vfs_fatfs.h"
#include "storage/vfs_littlefs.h"

#include "rtthread.h"

#define LOG_TAG "storage"
#define LOG_LVL LOG_LVL_DBG
#include "ulog.h"

static const vfs_cache_config_t storage_cache_config = {
    .root_path = "/flash/.vc",
    .segment_size = 4096U,
    .high_watermark_percent = 80U,
    .critical_watermark_percent = 95U,
    .worker_priority = RT_THREAD_PRIORITY_MAX - 3,
    .worker_stack_size = 1024U,
};

static int storage_init(void)
{
    int ret = vfs_init();
    if (ret != VFS_OK) {
        LOG_E("vfs_init failed: %d", ret);
        return ret;
    }

    ret = vfs_register_backend(&vfs_fatfs_backend);
    if (ret != VFS_OK) {
        LOG_E("register fatfs failed: %d", ret);
        return ret;
    }

    ret = vfs_register_backend(&vfs_littlefs_backend);
    if (ret != VFS_OK) {
        LOG_E("register littlefs failed: %d", ret);
        return ret;
    }

    ret = vfs_mount("/sd", "fatfs", RT_NULL);
    if (ret != VFS_OK) {
        LOG_E("mount /sd failed: %d", ret);
        return ret;
    }

    ret = vfs_mount("/flash", "littlefs", RT_NULL);
    if (ret != VFS_OK) {
        LOG_E("mount /flash failed: %d", ret);
        return ret;
    }

    ret = vfs_cache_register(&storage_cache_config);
    if (ret != VFS_OK) {
        LOG_E("register vfs cache failed: %d", ret);
        return ret;
    }

    LOG_I("VFS initialized.");
    return VFS_OK;
}
INIT_COMPONENT_EXPORT(storage_init);
```

- [ ] **Step 4: Add files to firmware source list**

Modify `cmake/sources.cmake` and add:

```cmake
    Application/storage/vfs_littlefs.c
    Application/storage/storage_init.c
```

- [ ] **Step 5: Run host tests**

Run:

```sh
cmake --build build/host-test
ctest --test-dir build/host-test --output-on-failure
```

Expected: all host tests pass.

- [ ] **Step 6: Commit**

```bash
git add Application/storage/vfs_littlefs.h Application/storage/vfs_littlefs.c Application/storage/storage_init.c cmake/sources.cmake
git commit -m "Register VFS storage backends at startup"
```

## Task 6: Firmware Build Verification And Follow-Up Notes

**Files:**
- Modify if required: `Application/storage/*.c`
- Modify if required: `cmake/sources.cmake`
- Modify if required: `docs/superpowers/specs/2026-06-14-vfs-design.md`

- [ ] **Step 1: Configure firmware build**

Run:

```sh
cmake --preset debug
```

Expected: Debug build configures successfully.

- [ ] **Step 2: Build firmware**

Run:

```sh
cmake --build --preset debug
```

Expected: firmware builds and emits `Y-Trace.elf`, `Y-Trace.bin`, and `Y-Trace.hex`.

- [ ] **Step 3: Fix compile errors without expanding scope**

If the build fails because of missing includes, type conflicts, or warning-as-error issues, fix only the VFS files and CMake entries. Do not implement full littlefs flash block-device integration in this task; keep `vfs_littlefs.c` as a not-supported skeleton unless the build requires a signature correction.

- [ ] **Step 4: Re-run host and firmware verification**

Run:

```sh
cmake --build build/host-test
ctest --test-dir build/host-test --output-on-failure
cmake --build --preset debug
```

Expected: host tests pass and firmware builds.

- [ ] **Step 5: Commit verification fixes**

```bash
git add Application/storage cmake/sources.cmake docs/superpowers/specs/2026-06-14-vfs-design.md
git commit -m "Verify VFS integration in firmware build"
```

## Self-Review

- Spec coverage: the plan covers the VFS API, path limit, error namespace, registered backends, longest-prefix mount dispatch, `VFS_O_CACHE`, cache config, raw-byte cache segments, and firmware registration. Track/GPX conversion remains out of scope.
- Placeholder scan: no task uses placeholder language for required code; full snippets are provided for new files and tests.
- Type consistency: `vfs_file_t`, `vfs_backend_t`, `vfs_cache_config_t`, flags, and error constants match across tasks.
- Known implementation risk: the first cache implementation uses a fixed two-slot cache array and only exercises one cached file in tests. That is acceptable for locking semantics but should be expanded before concurrent cached writes are relied on.
