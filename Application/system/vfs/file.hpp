#ifndef VFS_FILE_HPP
#define VFS_FILE_HPP
#include "IFileSystem.hpp"
#include "vfs.hpp"

namespace sys
{
class file
{
    file(const char *path, const int flag)
    {
        const char *sub;
        fs = const_cast<IFileSystem *>(VFS::resolve(path, &sub));
        if (fs)
        {
            fd = fs->open(path, flag);
        }
    }

    ~file()
    {
        if (fs && fd > 0)
        {
            fs->close(fd);
        }
    }

    int read(void *buffer, size_t len)
    {
        return fs ? fs->read(fd, buffer, len) : -1;
    }

    int write(void *buffer, size_t len)
    {
        return fs ? fs->write(fd, buffer, len) : -1;
    }

    bool valid() const
    {
        return fd > 0;
    }

private:
    IFileSystem *fs{nullptr};
    int fd{-1};
};

}


#endif //VFS_FILE_HPP