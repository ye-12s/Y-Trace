#ifndef VFS_VFS_H
#define VFS_VFS_H


#include "cstddef"
#include "IFileSystem.hpp"

namespace sys
{
class  VFS
{
public:
    static constexpr size_t MAX_MOUNT_POINT_SIZE = 4;
    static bool mount(const char *path, const IFileSystem *fs);
    static bool unmount(const char *path);
    static const IFileSystem *resolve(const char *path, const char **subpath);

private:
    struct mountPoint
    {
        const char *mount_path;
        const IFileSystem *fs;
    };
    static mountPoint mounts[MAX_MOUNT_POINT_SIZE];
    static size_t mount_point_num;
};
}


#endif //VFS_VFS_H