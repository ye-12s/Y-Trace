#ifndef VFS_IFILESYSTEM_HPP
#define VFS_IFILESYSTEM_HPP

#include <cstddef>
#include <cstdint>

namespace sys
{
using fd_t = uintptr_t;
enum class SEEK : uint8_t
{
    SET = 0,    // 文件开头
    CUR = 1,    // 当前文件位置
    END = 2,    // 文件结尾
};
enum class OPEN_FLAG : int
{
    READ = 1 << 0,    // 只读
    WRITE = 1 << 1,    // 只写
    RDWR   = READ | WRITE, // 读写
    CREAT  = 1 << 2,    // 创建文件
    TRUNC  = 1 << 3,    // 截断文件
    APPEND = 1 << 4,    // 追加写入
};
class IFileSystem
{
public:
    virtual ~IFileSystem() {};

    virtual fd_t open(const char *path, int flag) = 0;
    virtual int close(int fd) = 0;
    virtual int read(int fd, void *buf, size_t size) = 0;
    virtual int write(int fd, void *buf, size_t size) = 0;
    virtual int lseek(int fd, int offset, SEEK whence) = 0;

    bool busy() const
    {
        return reference_count > 0;
    }

protected:
    size_t reference_count = 0;   // 引用计数

};
}


#endif //VFS_IFILESYSTEM_HPP