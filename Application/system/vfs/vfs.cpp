#include "vfs.hpp"

#include <cstring>

using sys::VFS;
using sys::IFileSystem;

VFS::mountPoint VFS::mounts[MAX_MOUNT_POINT_SIZE];
size_t VFS::mount_point_num = 0;


bool VFS::mount(const char *path, const IFileSystem *fs)
{
    if (mount_point_num >= MAX_MOUNT_POINT_SIZE)
    {
        return false;
    }
    mounts[mount_point_num++] = {path, fs};
    return true;
}

bool VFS::unmount(const char *path)
{
    if (mount_point_num == 0)
    {
        return false;
    }

    for (size_t i = 0; i < mount_point_num; i++)
    {
        if (strcmp(path, mounts[i].mount_path) == 0)
        {
            // 向前移动后面的元素
            for (size_t j = i + 1; j < mount_point_num; j++)
            {
                mounts[j - 1] = mounts[j];
            }
            mount_point_num--;
            // 清空最后一个元素，防止访问悬空指针
            mounts[mount_point_num].mount_path = nullptr;
            mounts[mount_point_num].fs = nullptr;
            return true;
        }
    }
    return false;
}

const IFileSystem *VFS::resolve(const char*path, const char **subpath)
{
    // 只遍历已挂载的mount_point
    for (size_t i = 0; i < mount_point_num; i++)
    {
        auto &m = mounts[i];
        if (m.mount_path == nullptr) continue;  // 跳过已清空的元素
        
        size_t len = strlen(m.mount_path);
        if (strncmp(path, m.mount_path, len) == 0)
        {
            *subpath = path + len;
            if (**subpath == '/')(*subpath)++;
            return m.fs;
        }
    }
    return nullptr;
}





