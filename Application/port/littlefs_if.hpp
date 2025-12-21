#ifndef __LITTLE_FS_IF_HPP__
#define __LITTLE_FS_IF_HPP__

#include "lfs.h"
#include "system/vfs/IFileSystem.hpp"
#include "drivers/drv_flash.h"
#include <cstddef>
#include <cstring>

namespace littlefs
{
class fs : public sys::IFileSystem
{
public:
    ~fs() = default;

    fs() : initialized_(false) {}

    
    bool init()
    {
        if (initialized_)
        {
            return true;
        }

        int err = lfs_mount(&lfs, &cfg);
        if (err)
        {
            lfs_format(&lfs, &cfg);
            err = lfs_mount(&lfs, &cfg);
            if (err)
            {
                return false;
            }
        }
        initialized_ = true;
        return true;
    }

    bool is_initialized() const
    {
        return initialized_;
    }

    sys::fd_t open(const char *path, int flag) override
    {
        int lfs_flag = to_lfs_open_flag(flag);
        lfs_file_t *file = new lfs_file_t;
        int res = lfs_file_open(&lfs, file, path, lfs_flag);
        if (res < 0)
        {
            delete file;
            return -1;
        }
        reference_count++;  // 增加引用计数
        return reinterpret_cast<sys::fd_t>(file);
    }
    int close(int fd) override
    {
        lfs_file_t *file = reinterpret_cast<lfs_file_t *>(fd);
        int res = lfs_file_close(&lfs, file);
        delete file;
        if (res >= 0 && reference_count > 0)
        {
            reference_count--;  // 减少引用计数
        }
        return res;
    }
    int read(int fd, void *buf, size_t size) override
    {
        lfs_file_t *file = reinterpret_cast<lfs_file_t *>(fd);
        return static_cast<int>(lfs_file_read(&lfs, file, buf, static_cast<lfs_size_t>(size)));
    }
    int write(int fd, void *buf, size_t size) override
    {
        lfs_file_t *file = reinterpret_cast<lfs_file_t *>(fd);
        return static_cast<int>(lfs_file_write(&lfs, file, buf, static_cast<lfs_size_t>(size)));
    }
    int lseek(int fd, int offset, sys::SEEK whence) override
    {
        lfs_file_t *file = reinterpret_cast<lfs_file_t *>(fd);

        int whence_val = to_seek_whence(whence);
        if (whence_val < 0)
        {
            return -1; // Invalid whence
        }
        return lfs_file_seek(&lfs, file, static_cast<lfs_soff_t>(offset), whence_val);
    }

private:
    static int to_lfs_open_flag(int flag)
    {
        int lfs_flag = 0;
        // 优先检查RDWR，因为RDWR = READ | WRITE
        if (flag & static_cast<int>(sys::OPEN_FLAG::RDWR))
        {
            lfs_flag |= LFS_O_RDWR;
        }
        else if (flag & static_cast<int>(sys::OPEN_FLAG::WRITE))
        {
            lfs_flag |= LFS_O_WRONLY;
        }
        else if (flag & static_cast<int>(sys::OPEN_FLAG::READ))
        {
            lfs_flag |= LFS_O_RDONLY;
        }
        if (flag & static_cast<int>(sys::OPEN_FLAG::CREAT))
        {
            lfs_flag |= LFS_O_CREAT;
        }
        if (flag & static_cast<int>(sys::OPEN_FLAG::TRUNC))
        {
            lfs_flag |= LFS_O_TRUNC;
        }
        if (flag & static_cast<int>(sys::OPEN_FLAG::APPEND))
        {
            lfs_flag |= LFS_O_APPEND;
        }
        return lfs_flag;
    }
    static int to_seek_whence(sys::SEEK whence)
    {
        switch (whence)
        {
        case sys::SEEK::SET:
            return LFS_SEEK_SET;
        case sys::SEEK::CUR:
            return LFS_SEEK_CUR;
        case sys::SEEK::END:
            return LFS_SEEK_END;
        default:
            return -1; // Invalid whence
        }
    }
    lfs_t lfs;
    bool initialized_; // 初始化标志
    lfs_config cfg =
    {
        .read = lfs_diskio_read,
        .prog = lfs_diskio_prog,
        .erase = lfs_diskio_erase,
        .sync = lfs_diskio_sync,

        .read_size = 1,
        .prog_size = 4,
        .block_size = SECTOR_SIZE,
        .block_count = FLASH_FILESYSTEM_SIZE / SECTOR_SIZE,
        .block_cycles = 500,
        .cache_size = 16,
        .lookahead_size = 16,
    };
    static int lfs_diskio_read(const struct lfs_config *c, lfs_block_t block,
                               lfs_off_t off, void *buffer, lfs_size_t size)
    {
        const std::uint32_t addr = FLASH_FILESYSTEM_START_ADDR + (block * c->block_size) + off;
        drv_flash_read(addr, static_cast<std::uint8_t *>(buffer), size);
        return LFS_ERR_OK;
    }

    static int lfs_diskio_prog(const struct lfs_config *c, lfs_block_t block,
                               lfs_off_t off, const void *buffer, lfs_size_t size)
    {
        const std::uint32_t addr = FLASH_FILESYSTEM_START_ADDR + (block * c->block_size) + off;
        drv_flash_write_nocheck(addr, const_cast<std::uint8_t *>(static_cast<const std::uint8_t *>(buffer)), size);
        return LFS_ERR_OK;
    }

    static int lfs_diskio_erase(const struct lfs_config *c, lfs_block_t block)
    {
        drv_flash_erase_sector(FLASH_FILESYSTEM_START_ADDR + (block * c->block_size));
        return LFS_ERR_OK;
    }

    static int lfs_diskio_sync(const struct lfs_config *c)
    {
        return LFS_ERR_OK;
    }

};
}




#endif