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
