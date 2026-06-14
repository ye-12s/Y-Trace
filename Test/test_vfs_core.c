#include "storage/vfs.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

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

void setUp(void)
{
}

void tearDown(void)
{
}

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
    TEST_ASSERT_NOT_NULL(strstr(buffer, "static FATFS"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "f_mount"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "FR_OK"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "VFS_ERR_IO"));
}

static void test_vfs_selftest_exposes_command_and_autorun_gate(void)
{
    FILE *source_file = fopen(Y_TRACE_VFS_SELFTEST_SOURCE_PATH, "rb");
    FILE *options_file = fopen(Y_TRACE_CMAKE_OPTIONS_PATH, "rb");
    FILE *sources_file = fopen(Y_TRACE_CMAKE_SOURCES_PATH, "rb");
    char source[20000] = {0};
    char options[4096] = {0};
    char sources[4096] = {0};

    TEST_ASSERT_NOT_NULL(source_file);
    TEST_ASSERT_NOT_NULL(options_file);
    TEST_ASSERT_NOT_NULL(sources_file);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, fread(source, 1U, sizeof(source) - 1U, source_file));
    TEST_ASSERT_GREATER_THAN_UINT32(0U, fread(options, 1U, sizeof(options) - 1U, options_file));
    TEST_ASSERT_GREATER_THAN_UINT32(0U, fread(sources, 1U, sizeof(sources) - 1U, sources_file));
    fclose(source_file);
    fclose(options_file);
    fclose(sources_file);

    TEST_ASSERT_NOT_NULL(strstr(source, "MSH_CMD_EXPORT(vfs_selftest"));
    TEST_ASSERT_NOT_NULL(strstr(source, "Y_TRACE_VFS_SELFTEST_AUTORUN"));
    TEST_ASSERT_NOT_NULL(strstr(source, "INIT_APP_EXPORT(vfs_selftest_autorun"));
    TEST_ASSERT_NOT_NULL(strstr(source, "VFS_SELFTEST PASS"));
    TEST_ASSERT_NOT_NULL(strstr(options, "option(Y_TRACE_VFS_SELFTEST_AUTORUN"));
    TEST_ASSERT_NOT_NULL(strstr(options, "Y_TRACE_VFS_SELFTEST_AUTORUN"));
    TEST_ASSERT_NOT_NULL(strstr(sources, "Application/storage/vfs_selftest.c"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_vfs_errors_do_not_overlap_rtthread_common_negative_errors);
    RUN_TEST(test_vfs_path_limit_includes_null_terminator);
    RUN_TEST(test_vfs_cache_flag_uses_policy_bit_range);
    RUN_TEST(test_vfs_direct_open_dispatches_to_longest_mount_prefix);
    RUN_TEST(test_vfs_direct_file_operations_forward_to_backend);
    RUN_TEST(test_vfs_rejects_missing_mount_and_long_paths);
    RUN_TEST(test_vfs_fatfs_backend_source_preserves_error_mapping_boundary);
    RUN_TEST(test_vfs_selftest_exposes_command_and_autorun_gate);
    return UNITY_END();
}
