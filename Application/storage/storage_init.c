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
