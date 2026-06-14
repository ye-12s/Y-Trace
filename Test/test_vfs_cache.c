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

void setUp(void)
{
}

void tearDown(void)
{
}

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
