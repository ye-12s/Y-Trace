#include "storage/vfs.h"

#include <finsh.h>
#include <rtthread.h>

#include <stdint.h>
#include <string.h>

#define VFS_SELFTEST_MAX_FILES 8U
#define VFS_SELFTEST_MAX_OPEN  4U
#define VFS_SELFTEST_DATA_MAX  64U

typedef struct {
    uint8_t exists;
    char path[VFS_PATH_MAX];
    uint8_t data[VFS_SELFTEST_DATA_MAX];
    uint32_t size;
} vfs_selftest_entry_t;

typedef struct {
    uint8_t used;
    vfs_selftest_entry_t *entry;
    uint32_t pos;
} vfs_selftest_open_t;

typedef struct {
    vfs_selftest_entry_t entries[VFS_SELFTEST_MAX_FILES];
    vfs_selftest_open_t open[VFS_SELFTEST_MAX_OPEN];
} vfs_selftest_backend_ctx_t;

static vfs_selftest_backend_ctx_t vfs_selftest_main_ctx;
static vfs_selftest_backend_ctx_t vfs_selftest_cache_ctx;
static uint8_t vfs_selftest_registered;

static vfs_selftest_entry_t *vfs_selftest_find_entry(vfs_selftest_backend_ctx_t *ctx, const char *path)
{
    for (uint32_t i = 0U; i < VFS_SELFTEST_MAX_FILES; i++) {
        if (ctx->entries[i].exists != 0U && strcmp(ctx->entries[i].path, path) == 0) {
            return &ctx->entries[i];
        }
    }
    return RT_NULL;
}

static vfs_selftest_entry_t *vfs_selftest_alloc_entry(vfs_selftest_backend_ctx_t *ctx, const char *path)
{
    for (uint32_t i = 0U; i < VFS_SELFTEST_MAX_FILES; i++) {
        if (ctx->entries[i].exists == 0U) {
            ctx->entries[i].exists = 1U;
            (void)strncpy(ctx->entries[i].path, path, VFS_PATH_MAX - 1U);
            ctx->entries[i].path[VFS_PATH_MAX - 1U] = '\0';
            ctx->entries[i].size = 0U;
            return &ctx->entries[i];
        }
    }
    return RT_NULL;
}

static vfs_selftest_open_t *vfs_selftest_alloc_open(vfs_selftest_backend_ctx_t *ctx)
{
    for (uint32_t i = 0U; i < VFS_SELFTEST_MAX_OPEN; i++) {
        if (ctx->open[i].used == 0U) {
            ctx->open[i].used = 1U;
            return &ctx->open[i];
        }
    }
    return RT_NULL;
}

static int vfs_selftest_mock_open(void *backend_ctx, vfs_file_t *file, const char *path, uint32_t flags)
{
    vfs_selftest_backend_ctx_t *ctx = (vfs_selftest_backend_ctx_t *)backend_ctx;
    vfs_selftest_entry_t *entry = vfs_selftest_find_entry(ctx, path);
    vfs_selftest_open_t *open = RT_NULL;

    if (entry == RT_NULL) {
        if ((flags & VFS_O_CREATE) == 0U) {
            return VFS_ERR_NOT_FOUND;
        }
        entry = vfs_selftest_alloc_entry(ctx, path);
        if (entry == RT_NULL) {
            return VFS_ERR_NO_SPACE;
        }
    }

    if ((flags & VFS_O_TRUNC) != 0U) {
        entry->size = 0U;
    }

    open = vfs_selftest_alloc_open(ctx);
    if (open == RT_NULL) {
        return VFS_ERR_BUSY;
    }

    open->entry = entry;
    open->pos = (flags & VFS_O_APPEND) != 0U ? entry->size : 0U;
    file->backend_file = open;
    return VFS_OK;
}

static int vfs_selftest_mock_read(vfs_file_t *file, void *buffer, uint32_t size, uint32_t *bytes_read)
{
    vfs_selftest_open_t *open = (vfs_selftest_open_t *)file->backend_file;
    uint32_t available;
    uint32_t count;

    if (open == RT_NULL || buffer == RT_NULL || bytes_read == RT_NULL) {
        return VFS_ERR_INVALID;
    }

    available = open->entry->size - open->pos;
    count = available < size ? available : size;
    (void)memcpy(buffer, &open->entry->data[open->pos], count);
    open->pos += count;
    *bytes_read = count;
    return VFS_OK;
}

static int vfs_selftest_mock_write(vfs_file_t *file, const void *buffer, uint32_t size, uint32_t *bytes_written)
{
    vfs_selftest_open_t *open = (vfs_selftest_open_t *)file->backend_file;

    if (open == RT_NULL || buffer == RT_NULL || bytes_written == RT_NULL) {
        return VFS_ERR_INVALID;
    }
    if (open->pos + size > VFS_SELFTEST_DATA_MAX) {
        return VFS_ERR_NO_SPACE;
    }

    (void)memcpy(&open->entry->data[open->pos], buffer, size);
    open->pos += size;
    if (open->pos > open->entry->size) {
        open->entry->size = open->pos;
    }
    *bytes_written = size;
    return VFS_OK;
}

static int vfs_selftest_mock_sync(vfs_file_t *file)
{
    return file == RT_NULL ? VFS_ERR_INVALID : VFS_OK;
}

static int vfs_selftest_mock_close(vfs_file_t *file)
{
    vfs_selftest_open_t *open = file == RT_NULL ? RT_NULL : (vfs_selftest_open_t *)file->backend_file;
    if (open == RT_NULL) {
        return VFS_ERR_INVALID;
    }
    open->used = 0U;
    open->entry = RT_NULL;
    open->pos = 0U;
    file->backend_file = RT_NULL;
    return VFS_OK;
}

static int vfs_selftest_mock_delete(void *backend_ctx, const char *path)
{
    vfs_selftest_entry_t *entry = vfs_selftest_find_entry((vfs_selftest_backend_ctx_t *)backend_ctx, path);
    if (entry == RT_NULL) {
        return VFS_ERR_NOT_FOUND;
    }
    memset(entry, 0, sizeof(*entry));
    return VFS_OK;
}

static int vfs_selftest_mock_rename(void *backend_ctx, const char *old_path, const char *new_path)
{
    vfs_selftest_entry_t *entry = vfs_selftest_find_entry((vfs_selftest_backend_ctx_t *)backend_ctx, old_path);
    if (entry == RT_NULL) {
        return VFS_ERR_NOT_FOUND;
    }
    if (strnlen(new_path, VFS_PATH_MAX) >= VFS_PATH_MAX) {
        return VFS_ERR_PATH_TOO_LONG;
    }
    (void)strncpy(entry->path, new_path, VFS_PATH_MAX - 1U);
    entry->path[VFS_PATH_MAX - 1U] = '\0';
    return VFS_OK;
}

static int vfs_selftest_mock_stat(void *backend_ctx, const char *path, vfs_stat_t *stat)
{
    vfs_selftest_entry_t *entry = vfs_selftest_find_entry((vfs_selftest_backend_ctx_t *)backend_ctx, path);
    if (entry == RT_NULL) {
        return VFS_ERR_NOT_FOUND;
    }
    stat->size = entry->size;
    stat->is_dir = 0U;
    return VFS_OK;
}

static const vfs_backend_t vfs_selftest_backend = {
    .name = "selftest",
    .open = vfs_selftest_mock_open,
    .read = vfs_selftest_mock_read,
    .write = vfs_selftest_mock_write,
    .sync = vfs_selftest_mock_sync,
    .close = vfs_selftest_mock_close,
    .delete = vfs_selftest_mock_delete,
    .rename = vfs_selftest_mock_rename,
    .stat = vfs_selftest_mock_stat,
};

static int vfs_selftest_expect(const char *phase, int actual, int expected)
{
    if (actual != expected) {
        rt_kprintf("VFS_SELFTEST FAIL phase=%s expected=%d actual=%d\n", phase, expected, actual);
        return -1;
    }
    return 0;
}

static int vfs_selftest_register_mock(void)
{
    int ret;

    memset(&vfs_selftest_main_ctx, 0, sizeof(vfs_selftest_main_ctx));
    memset(&vfs_selftest_cache_ctx, 0, sizeof(vfs_selftest_cache_ctx));

    if (vfs_selftest_registered != 0U) {
        return VFS_OK;
    }

    ret = vfs_register_backend(&vfs_selftest_backend);
    if (ret != VFS_OK) {
        return ret;
    }
    ret = vfs_mount("/vt", "selftest", &vfs_selftest_main_ctx);
    if (ret != VFS_OK) {
        return ret;
    }
    ret = vfs_mount("/vc", "selftest", &vfs_selftest_cache_ctx);
    if (ret != VFS_OK) {
        return ret;
    }

    vfs_selftest_registered = 1U;
    return VFS_OK;
}

static int vfs_selftest_direct_api(void)
{
    static const char payload[] = "vfs-ok";
    vfs_file_t file;
    vfs_stat_t stat;
    char buffer[sizeof(payload)] = {0};
    uint32_t count = 0U;
    int ret;

    ret = vfs_open(&file, "/vt/a.txt", VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
    if (vfs_selftest_expect("direct_open_write", ret, VFS_OK) != 0) return -1;
    ret = vfs_write(&file, payload, (uint32_t)strlen(payload), &count);
    if (vfs_selftest_expect("direct_write", ret, VFS_OK) != 0) return -1;
    if (count != strlen(payload)) return vfs_selftest_expect("direct_write_count", (int)count, (int)strlen(payload));
    ret = vfs_sync(&file);
    if (vfs_selftest_expect("direct_sync", ret, VFS_OK) != 0) return -1;
    ret = vfs_close(&file);
    if (vfs_selftest_expect("direct_close_write", ret, VFS_OK) != 0) return -1;

    ret = vfs_stat("/vt/a.txt", &stat);
    if (vfs_selftest_expect("direct_stat", ret, VFS_OK) != 0) return -1;
    if (stat.size != strlen(payload)) return vfs_selftest_expect("direct_stat_size", (int)stat.size, (int)strlen(payload));

    ret = vfs_rename("/vt/a.txt", "/vt/b.txt");
    if (vfs_selftest_expect("direct_rename", ret, VFS_OK) != 0) return -1;
    ret = vfs_open(&file, "/vt/b.txt", VFS_O_READ);
    if (vfs_selftest_expect("direct_open_read", ret, VFS_OK) != 0) return -1;
    ret = vfs_read(&file, buffer, sizeof(buffer), &count);
    if (vfs_selftest_expect("direct_read", ret, VFS_OK) != 0) return -1;
    if (count != strlen(payload) || memcmp(buffer, payload, strlen(payload)) != 0) {
        rt_kprintf("VFS_SELFTEST FAIL phase=direct_read_data count=%lu\n", (unsigned long)count);
        return -1;
    }
    ret = vfs_close(&file);
    if (vfs_selftest_expect("direct_close_read", ret, VFS_OK) != 0) return -1;
    ret = vfs_delete("/vt/b.txt");
    if (vfs_selftest_expect("direct_delete", ret, VFS_OK) != 0) return -1;
    ret = vfs_stat("/vt/b.txt", &stat);
    if (vfs_selftest_expect("direct_stat_deleted", ret, VFS_ERR_NOT_FOUND) != 0) return -1;

    return 0;
}

static int vfs_selftest_cache_api(void)
{
    static const char payload[] = "cache";
    const vfs_cache_config_t config = {
        .root_path = "/vc",
        .segment_size = 32U,
        .high_watermark_percent = 80U,
        .critical_watermark_percent = 95U,
        .worker_priority = RT_THREAD_PRIORITY_MAX - 3,
        .worker_stack_size = 1024U,
    };
    vfs_file_t file;
    uint32_t count = 0U;
    int ret;

    ret = vfs_cache_register(&config);
    if (vfs_selftest_expect("cache_register", ret, VFS_OK) != 0) return -1;
    ret = vfs_open(&file, "/vt/c.bin", VFS_O_WRITE | VFS_O_CREATE | VFS_O_CACHE);
    if (vfs_selftest_expect("cache_open", ret, VFS_OK) != 0) return -1;
    ret = vfs_read(&file, &count, sizeof(count), &count);
    if (vfs_selftest_expect("cache_read_rejected", ret, VFS_ERR_NOT_SUPPORTED) != 0) return -1;
    ret = vfs_write(&file, payload, (uint32_t)strlen(payload), &count);
    if (vfs_selftest_expect("cache_write", ret, VFS_OK) != 0) return -1;
    if (count != strlen(payload)) return vfs_selftest_expect("cache_write_count", (int)count, (int)strlen(payload));
    ret = vfs_sync(&file);
    if (vfs_selftest_expect("cache_sync", ret, VFS_OK) != 0) return -1;
    ret = vfs_close(&file);
    if (vfs_selftest_expect("cache_close", ret, VFS_OK) != 0) return -1;

    for (uint32_t i = 0U; i < VFS_SELFTEST_MAX_FILES; i++) {
        if (vfs_selftest_cache_ctx.entries[i].exists != 0U &&
            strstr(vfs_selftest_cache_ctx.entries[i].path, ".s") != RT_NULL &&
            vfs_selftest_cache_ctx.entries[i].size == strlen(payload) &&
            memcmp(vfs_selftest_cache_ctx.entries[i].data, payload, strlen(payload)) == 0) {
            return 0;
        }
    }

    rt_kprintf("VFS_SELFTEST FAIL phase=cache_sealed_segment_missing\n");
    return -1;
}

static int vfs_selftest_error_api(void)
{
    vfs_file_t file;
    vfs_stat_t stat;
    const char long_path[] = "/vt/abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijk";
    int ret;

    ret = vfs_open(&file, "/missing/a.txt", VFS_O_READ);
    if (vfs_selftest_expect("missing_mount", ret, VFS_ERR_NOT_FOUND) != 0) return -1;
    ret = vfs_open(&file, long_path, VFS_O_READ);
    if (vfs_selftest_expect("long_path", ret, VFS_ERR_PATH_TOO_LONG) != 0) return -1;
    ret = vfs_open(&file, "/flash/notready.bin", VFS_O_READ);
    if (vfs_selftest_expect("littlefs_skeleton", ret, VFS_ERR_NOT_SUPPORTED) != 0) return -1;
    ret = vfs_rename("/vt/nope", "/vc/nope");
    if (vfs_selftest_expect("cross_mount_rename", ret, VFS_ERR_NOT_SUPPORTED) != 0) return -1;
    ret = vfs_stat("/vt/nope", &stat);
    if (vfs_selftest_expect("stat_missing", ret, VFS_ERR_NOT_FOUND) != 0) return -1;

    return 0;
}

static int vfs_selftest_sd_api(void)
{
    static const char payload[] = "sd-vfs";
    vfs_file_t file;
    vfs_stat_t stat;
    char buffer[sizeof(payload)] = {0};
    uint32_t count = 0U;
    int ret;

    ret = vfs_open(&file, "/sd/VFSTST.TXT", VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
    if (ret != VFS_OK) {
        rt_kprintf("VFS_SELFTEST SKIP phase=sd_open_write ret=%d\n", ret);
        return 0;
    }

    ret = vfs_write(&file, payload, (uint32_t)strlen(payload), &count);
    if (vfs_selftest_expect("sd_write", ret, VFS_OK) != 0) return -1;
    ret = vfs_sync(&file);
    if (vfs_selftest_expect("sd_sync", ret, VFS_OK) != 0) return -1;
    ret = vfs_close(&file);
    if (vfs_selftest_expect("sd_close_write", ret, VFS_OK) != 0) return -1;
    ret = vfs_stat("/sd/VFSTST.TXT", &stat);
    if (vfs_selftest_expect("sd_stat", ret, VFS_OK) != 0) return -1;
    ret = vfs_rename("/sd/VFSTST.TXT", "/sd/VFSTST2.TXT");
    if (vfs_selftest_expect("sd_rename", ret, VFS_OK) != 0) return -1;
    ret = vfs_open(&file, "/sd/VFSTST2.TXT", VFS_O_READ);
    if (vfs_selftest_expect("sd_open_read", ret, VFS_OK) != 0) return -1;
    ret = vfs_read(&file, buffer, sizeof(buffer), &count);
    if (vfs_selftest_expect("sd_read", ret, VFS_OK) != 0) return -1;
    if (count != strlen(payload) || memcmp(buffer, payload, strlen(payload)) != 0) {
        rt_kprintf("VFS_SELFTEST FAIL phase=sd_read_data count=%lu\n", (unsigned long)count);
        return -1;
    }
    ret = vfs_close(&file);
    if (vfs_selftest_expect("sd_close_read", ret, VFS_OK) != 0) return -1;
    ret = vfs_delete("/sd/VFSTST2.TXT");
    if (vfs_selftest_expect("sd_delete", ret, VFS_OK) != 0) return -1;

    rt_kprintf("VFS_SELFTEST PASS phase=sd_direct\n");
    return 0;
}

static int vfs_selftest(int argc, char **argv)
{
    uint8_t run_sd = (argc > 1 && argv != RT_NULL && argv[1] != RT_NULL && strcmp(argv[1], "sd") == 0) ? 1U : 0U;

    rt_kprintf("VFS_SELFTEST begin\n");

    if (vfs_selftest_expect("register_mock", vfs_selftest_register_mock(), VFS_OK) != 0) return -1;
    if (vfs_selftest_direct_api() != 0) return -1;
    if (vfs_selftest_cache_api() != 0) return -1;
    if (vfs_selftest_error_api() != 0) return -1;
    if (run_sd != 0U && vfs_selftest_sd_api() != 0) return -1;

    rt_kprintf("VFS_SELFTEST PASS\n");
    return 0;
}
MSH_CMD_EXPORT(vfs_selftest, run VFS framework self-test; use 'vfs_selftest sd' for physical SD path);

#ifdef Y_TRACE_VFS_SELFTEST_AUTORUN
static int vfs_selftest_autorun(void)
{
    return vfs_selftest(0, RT_NULL);
}
INIT_APP_EXPORT(vfs_selftest_autorun);
#endif
