/**
 * @file vfs_test.cpp
 * @brief VFS (Virtual File System) unit test cases
 * @date 2025-12-21
 */

#include "system/vfs/vfs.hpp"
#include "system/vfs/IFileSystem.hpp"
#include <utest.h>
#include <rtthread.h>
#include <cstring>
#include <new>

using namespace sys;

/**
 * @brief Mock file system for testing purposes
 */
class MockFileSystem : public IFileSystem
{
public:
    MockFileSystem() : last_fd(0), open_count(0), close_count(0),
                       read_count(0), write_count(0), lseek_count(0)
    {
        memset(file_data, 0, sizeof(file_data));
        memset(file_pos, 0, sizeof(file_pos));
        memset(file_len, 0, sizeof(file_len));
        memset(file_opened, 0, sizeof(file_opened));
    }

    fd_t open(const char *path, int flag) override
    {
        open_count++;
        last_fd++;
        if (last_fd >= MAX_FILES) last_fd = 1;
        
        file_opened[last_fd] = true;
        file_pos[last_fd] = 0;
        reference_count++;
        
        // 如果是截断标志，清空文件数据和长度
        if (flag & (int)OPEN_FLAG::TRUNC)
        {
            memset(file_data[last_fd], 0, sizeof(file_data[last_fd]));
            file_len[last_fd] = 0;
        }
        // 如果只是创建标志（非截断），保持现有数据
        else if (flag & (int)OPEN_FLAG::CREAT)
        {
            // 新文件，长度为0
            if (file_len[last_fd] == 0)
            {
                memset(file_data[last_fd], 0, sizeof(file_data[last_fd]));
            }
        }
        
        return last_fd;
    }

    int close(int fd) override
    {
        close_count++;
        if (fd <= 0 || (size_t)fd >= MAX_FILES || !file_opened[fd])
        {
            return -1;
        }
        
        file_opened[fd] = false;
        reference_count--;
        return 0;
    }

    int read(int fd, void *buf, size_t size) override
    {
        read_count++;
        if (fd <= 0 || (size_t)fd >= MAX_FILES || !file_opened[fd] || !buf)
        {
            return -1;
        }
        
        // 只能读取到文件实际长度
        if (file_pos[fd] >= file_len[fd])
        {
            return 0;  // 已到达文件末尾
        }
        
        size_t available = file_len[fd] - file_pos[fd];
        size_t to_read = (size < available) ? size : available;
        
        memcpy(buf, &file_data[fd][file_pos[fd]], to_read);
        file_pos[fd] += to_read;
        
        return to_read;
    }

    int write(int fd, void *buf, size_t size) override
    {
        write_count++;
        if (fd <= 0 || (size_t)fd >= MAX_FILES || !file_opened[fd] || !buf)
        {
            return -1;
        }
        
        size_t available = BUFFER_SIZE - file_pos[fd];
        size_t to_write = (size < available) ? size : available;
        
        memcpy(&file_data[fd][file_pos[fd]], buf, to_write);
        file_pos[fd] += to_write;
        
        // 更新文件长度（如果写入位置超过当前长度）
        if (file_pos[fd] > file_len[fd])
        {
            file_len[fd] = file_pos[fd];
        }
        
        return to_write;
    }

    int lseek(int fd, int offset, SEEK whence) override
    {
        lseek_count++;
        if (fd <= 0 || (size_t)fd >= MAX_FILES || !file_opened[fd])
        {
            return -1;
        }
        
        int new_pos = 0;
        switch (whence)
        {
        case SEEK::SET:
            new_pos = offset;
            break;
        case SEEK::CUR:
            new_pos = file_pos[fd] + offset;
            break;
        case SEEK::END:
            new_pos = file_len[fd] + offset;  // 基于文件实际长度
            break;
        default:
            return -1;
        }
        
        if (new_pos < 0 || (size_t)new_pos > BUFFER_SIZE)
        {
            return -1;
        }
        
        file_pos[fd] = new_pos;
        return new_pos;
    }

    // 测试辅助方法
    void reset_counters()
    {
        open_count = 0;
        close_count = 0;
        read_count = 0;
        write_count = 0;
        lseek_count = 0;
        last_fd = 0;
        // 重置文件状态
        memset(file_pos, 0, sizeof(file_pos));
        memset(file_len, 0, sizeof(file_len));
        memset(file_opened, 0, sizeof(file_opened));
        memset(file_data, 0, sizeof(file_data));
    }

    int get_open_count() const { return open_count; }
    int get_close_count() const { return close_count; }
    int get_read_count() const { return read_count; }
    int get_write_count() const { return write_count; }
    int get_lseek_count() const { return lseek_count; }

private:
    static constexpr size_t MAX_FILES = 8;
    static constexpr size_t BUFFER_SIZE = 256;
    
    fd_t last_fd;
    int open_count;
    int close_count;
    int read_count;
    int write_count;
    int lseek_count;
    
    char file_data[MAX_FILES][BUFFER_SIZE];
    int file_pos[MAX_FILES];
    int file_len[MAX_FILES];  // 文件实际数据长度
    bool file_opened[MAX_FILES];
};

// 全局测试对象 - 使用静态对象避免动态内存分配问题
static MockFileSystem g_mock_fs_instance;
static MockFileSystem g_mock_fs2_instance;
static MockFileSystem g_mock_fs3_instance;
static MockFileSystem *g_mock_fs = nullptr;
static MockFileSystem *g_mock_fs2 = nullptr;
static MockFileSystem *g_mock_fs3 = nullptr;

// 使用extern "C"包裹测试函数以支持UTEST_TC_EXPORT宏
extern "C" {

/**
 * @brief 测试初始化函数
 */
RT_UNUSED static rt_err_t utest_tc_init(void)
{
    // 使用placement new重新初始化对象
    g_mock_fs = new (&g_mock_fs_instance) MockFileSystem();
    VFS::mount("/test", g_mock_fs);
    return RT_EOK;
}

/**
 * @brief 测试清理函数
 */
RT_UNUSED static rt_err_t utest_tc_cleanup(void)
{
    // 先卸载挂载点
    VFS::unmount("/test");
    
    // 不需要delete，只需要清空指针
    g_mock_fs = nullptr;
    
    return RT_EOK;
}

/**
 * @brief 多挂载点测试初始化函数
 */
RT_UNUSED static rt_err_t utest_multi_mount_init(void)
{
    // 初始化三个文件系统实例
    g_mock_fs = new (&g_mock_fs_instance) MockFileSystem();
    g_mock_fs2 = new (&g_mock_fs2_instance) MockFileSystem();
    g_mock_fs3 = new (&g_mock_fs3_instance) MockFileSystem();
    
    // 挂载到不同路径
    VFS::mount("/data", g_mock_fs);
    VFS::mount("/config", g_mock_fs2);
    VFS::mount("/log", g_mock_fs3);
    
    return RT_EOK;
}

/**
 * @brief 多挂载点测试清理函数
 */
RT_UNUSED static rt_err_t utest_multi_mount_cleanup(void)
{
    // 卸载所有挂载点
    VFS::unmount("/data");
    VFS::unmount("/config");
    VFS::unmount("/log");
    
    // 清空指针
    g_mock_fs = nullptr;
    g_mock_fs2 = nullptr;
    g_mock_fs3 = nullptr;
    
    return RT_EOK;
}

/**
 * @brief 测试用例：单个挂载点多文件操作测试
 */
RT_UNUSED static void test_vfs_multiple_files(void)
{
    g_mock_fs->reset_counters();
    
    // 1. 测试路径解析
    const char *subpath;
    const IFileSystem *fs = VFS::resolve("/test/file1.txt", &subpath);
    uassert_not_null(fs);
    uassert_str_equal(subpath, "file1.txt");
    
    // 2. 打开第一个文件并写入数据
    fd_t fd1 = g_mock_fs->open("file1.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd1 > 0);
    
    const char data1[] = "Hello File 1";
    int len1 = g_mock_fs->write(fd1, (void *)data1, strlen(data1));
    uassert_int_equal(len1, strlen(data1));
    
    // 3. 打开第二个文件并写入数据
    fd_t fd2 = g_mock_fs->open("file2.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd2 > 0);
    uassert_true(fd2 != fd1);
    
    const char data2[] = "Hello File 2";
    int len2 = g_mock_fs->write(fd2, (void *)data2, strlen(data2));
    uassert_int_equal(len2, strlen(data2));
    
    // 4. 打开第三个文件并写入数据
    fd_t fd3 = g_mock_fs->open("file3.bin", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd3 > 0);
    uassert_true(fd3 != fd1 && fd3 != fd2);
    
    const char data3[] = "Binary Data 3";
    int len3 = g_mock_fs->write(fd3, (void *)data3, strlen(data3));
    uassert_int_equal(len3, strlen(data3));
    
    // 5. 验证文件系统忙状态
    uassert_true(g_mock_fs->busy());
    
    // 6. 定位并读取第一个文件
    g_mock_fs->lseek(fd1, 0, SEEK::SET);
    char read_buf1[32] = {0};
    int read_len1 = g_mock_fs->read(fd1, read_buf1, strlen(data1));
    uassert_int_equal(read_len1, strlen(data1));
    uassert_str_equal(read_buf1, data1);
    
    // 7. 定位并读取第二个文件
    g_mock_fs->lseek(fd2, 0, SEEK::SET);
    char read_buf2[32] = {0};
    int read_len2 = g_mock_fs->read(fd2, read_buf2, strlen(data2));
    uassert_int_equal(read_len2, strlen(data2));
    uassert_str_equal(read_buf2, data2);
    
    // 8. 定位并读取第三个文件
    g_mock_fs->lseek(fd3, 0, SEEK::SET);
    char read_buf3[32] = {0};
    int read_len3 = g_mock_fs->read(fd3, read_buf3, strlen(data3));
    uassert_int_equal(read_len3, strlen(data3));
    uassert_str_equal(read_buf3, data3);
    
    // 9. 测试文件定位操作（使用第一个文件）
    int pos = g_mock_fs->lseek(fd1, 6, SEEK::SET);
    uassert_int_equal(pos, 6);
    
    char buf[10] = {0};
    g_mock_fs->read(fd1, buf, 6);
    uassert_str_equal(buf, "File 1");
    
    // 10. 关闭第二个文件
    int result = g_mock_fs->close(fd2);
    uassert_int_equal(result, 0);
    
    // 文件系统仍然忙（还有文件打开）
    uassert_true(g_mock_fs->busy());
    
    // 11. 关闭第一个和第三个文件
    g_mock_fs->close(fd1);
    g_mock_fs->close(fd3);
    
    // 文件系统不忙了
    uassert_false(g_mock_fs->busy());
    
    // 12. 验证操作计数
    uassert_int_equal(g_mock_fs->get_open_count(), 3);
    uassert_int_equal(g_mock_fs->get_close_count(), 3);
    uassert_true(g_mock_fs->get_read_count() >= 3);
    uassert_true(g_mock_fs->get_write_count() >= 3);
}

/**
 * @brief 测试用例：路径解析测试
 */
RT_UNUSED static void test_vfs_path_resolve(void)
{
    const char *subpath = nullptr;
    const IFileSystem *fs = nullptr;
    
    // 测试根路径解析
    fs = VFS::resolve("/test/file.txt", &subpath);
    uassert_not_null(fs);
    uassert_str_equal(subpath, "file.txt");
    
    // 测试带子目录的路径解析
    fs = VFS::resolve("/test/dir/subdir/file.bin", &subpath);
    uassert_not_null(fs);
    uassert_str_equal(subpath, "dir/subdir/file.bin");
    
    // 测试无效路径解析
    fs = VFS::resolve("/invalid/path", &subpath);
    uassert_null(fs);
    
    // 测试路径末尾带斜杠
    fs = VFS::resolve("/test/", &subpath);
    uassert_not_null(fs);
    uassert_str_equal(subpath, "");
}

/**
 * @brief 测试用例：文件截断功能测试
 */
RT_UNUSED static void test_vfs_file_truncate(void)
{
    g_mock_fs->reset_counters();
    
    // 创建文件并写入数据
    fd_t fd = g_mock_fs->open("truncate_test.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd > 0);
    
    const char original_data[] = "Original Content";
    int len = g_mock_fs->write(fd, (void *)original_data, strlen(original_data));
    uassert_int_equal(len, strlen(original_data));
    g_mock_fs->close(fd);
    
    // 使用TRUNC标志重新打开文件
    fd = g_mock_fs->open("truncate_test.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::TRUNC);
    uassert_true(fd > 0);
    
    // 写入新数据（更短）
    const char new_data[] = "New";
    len = g_mock_fs->write(fd, (void *)new_data, strlen(new_data));
    uassert_int_equal(len, strlen(new_data));
    
    // 重新读取验证
    g_mock_fs->lseek(fd, 0, SEEK::SET);
    char read_buf[32] = {0};
    int read_len = g_mock_fs->read(fd, read_buf, sizeof(read_buf));
    uassert_int_equal(read_len, strlen(new_data));
    uassert_str_equal(read_buf, new_data);
    
    g_mock_fs->close(fd);
}

/**
 * @brief 测试用例：文件追加写入测试
 */
RT_UNUSED static void test_vfs_file_append(void)
{
    g_mock_fs->reset_counters();
    
    // 创建文件并写入初始数据
    fd_t fd = g_mock_fs->open("append_test.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd > 0);
    
    const char part1[] = "Part1";
    g_mock_fs->write(fd, (void *)part1, strlen(part1));
    RT_UNUSED int pos1 = g_mock_fs->lseek(fd, 0, SEEK::CUR);
    
    // 追加写入
    const char part2[] = "Part2";
    g_mock_fs->write(fd, (void *)part2, strlen(part2));
    
    // 验证总长度
    int total_len = g_mock_fs->lseek(fd, 0, SEEK::CUR);
    uassert_int_equal(total_len, strlen(part1) + strlen(part2));
    
    // 读取并验证完整内容
    g_mock_fs->lseek(fd, 0, SEEK::SET);
    char read_buf[32] = {0};
    g_mock_fs->read(fd, read_buf, total_len);
    uassert_str_equal(read_buf, "Part1Part2");
    
    g_mock_fs->close(fd);
}

/**
 * @brief 测试用例：文件定位边界测试
 */
RT_UNUSED static void test_vfs_seek_boundary(void)
{
    g_mock_fs->reset_counters();
    
    fd_t fd = g_mock_fs->open("seek_boundary.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd > 0);
    
    const char data[] = "0123456789ABCDEF";
    g_mock_fs->write(fd, (void *)data, strlen(data));
    
    // 测试SEEK::SET到开始
    int pos = g_mock_fs->lseek(fd, 0, SEEK::SET);
    uassert_int_equal(pos, 0);
    
    // 测试SEEK::END负偏移
    pos = g_mock_fs->lseek(fd, -5, SEEK::END);
    uassert_true(pos > 0);
    
    // 测试SEEK::CUR正向移动
    g_mock_fs->lseek(fd, 5, SEEK::SET);
    pos = g_mock_fs->lseek(fd, 3, SEEK::CUR);
    uassert_int_equal(pos, 8);
    
    // 测试SEEK::CUR负向移动
    pos = g_mock_fs->lseek(fd, -3, SEEK::CUR);
    uassert_int_equal(pos, 5);
    
    // 验证读取位置正确
    char buf[5] = {0};
    g_mock_fs->read(fd, buf, 4);
    uassert_str_equal(buf, "5678");
    
    g_mock_fs->close(fd);
}

/**
 * @brief 测试用例：错误处理 - 无效文件描述符
 */
RT_UNUSED static void test_vfs_invalid_fd(void)
{
    g_mock_fs->reset_counters();
    
    char buf[10];
    
    // 测试读写无效fd=0
    int result = g_mock_fs->read(0, buf, sizeof(buf));
    uassert_int_equal(result, -1);
    
    result = g_mock_fs->write(0, buf, sizeof(buf));
    uassert_int_equal(result, -1);
    
    // 测试seek无效fd
    result = g_mock_fs->lseek(0, 0, SEEK::SET);
    uassert_int_equal(result, -1);
    
    // 测试关闭无效fd
    result = g_mock_fs->close(0);
    uassert_int_equal(result, -1);
    
    // 测试超大fd
    result = g_mock_fs->close(999);
    uassert_int_equal(result, -1);
}

/**
 * @brief 测试用例：错误处理 - 关闭已关闭的文件
 */
RT_UNUSED static void test_vfs_double_close(void)
{
    g_mock_fs->reset_counters();
    
    fd_t fd = g_mock_fs->open("double_close.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd > 0);
    
    // 第一次关闭应该成功
    int result = g_mock_fs->close(fd);
    uassert_int_equal(result, 0);
    
    // 第二次关闭应该失败
    result = g_mock_fs->close(fd);
    uassert_int_equal(result, -1);
}

/**
 * @brief 测试用例：错误处理 - 操作已关闭的文件
 */
RT_UNUSED static void test_vfs_use_closed_fd(void)
{
    g_mock_fs->reset_counters();
    
    fd_t fd = g_mock_fs->open("closed_fd.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd > 0);
    
    // 写入一些数据
    const char data[] = "test";
    g_mock_fs->write(fd, (void *)data, strlen(data));
    
    // 关闭文件
    g_mock_fs->close(fd);
    
    // 尝试读取已关闭的文件
    char buf[10];
    int result = g_mock_fs->read(fd, buf, sizeof(buf));
    uassert_int_equal(result, -1);
    
    // 尝试写入已关闭的文件
    result = g_mock_fs->write(fd, (void *)data, strlen(data));
    uassert_int_equal(result, -1);
    
    // 尝试seek已关闭的文件
    result = g_mock_fs->lseek(fd, 0, SEEK::SET);
    uassert_int_equal(result, -1);
}

/**
 * @brief 测试用例：VFS挂载卸载测试
 */
RT_UNUSED static void test_vfs_mount_unmount(void)
{
    MockFileSystem temp_fs;
    
    // 测试挂载
    bool result = VFS::mount("/temp", &temp_fs);
    uassert_true(result);
    
    // 验证挂载成功
    const char *subpath;
    const IFileSystem *fs = VFS::resolve("/temp/file.txt", &subpath);
    uassert_not_null(fs);
    uassert_true(fs == &temp_fs);
    
    // 测试卸载
    result = VFS::unmount("/temp");
    uassert_true(result);
    
    // 验证卸载成功
    fs = VFS::resolve("/temp/file.txt", &subpath);
    uassert_null(fs);
    
    // 测试卸载不存在的挂载点
    result = VFS::unmount("/nonexist");
    uassert_false(result);
}

/**
 * @brief 测试用例：空指针保护测试
 */
RT_UNUSED static void test_vfs_null_pointer(void)
{
    g_mock_fs->reset_counters();
    
    fd_t fd = g_mock_fs->open("null_test.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd > 0);
    
    // 测试read空指针
    int result = g_mock_fs->read(fd, nullptr, 10);
    uassert_int_equal(result, -1);
    
    // 测试write空指针
    result = g_mock_fs->write(fd, nullptr, 10);
    uassert_int_equal(result, -1);
    
    g_mock_fs->close(fd);
}

/**
 * @brief 测试用例：多挂载点路径解析测试
 */
RT_UNUSED static void test_vfs_multi_mount_resolve(void)
{
    const char *subpath = nullptr;
    const IFileSystem *fs = nullptr;
    
    // 测试第一个挂载点
    fs = VFS::resolve("/data/sensor.dat", &subpath);
    uassert_not_null(fs);
    uassert_true(fs == g_mock_fs);
    uassert_str_equal(subpath, "sensor.dat");
    
    // 测试第二个挂载点
    fs = VFS::resolve("/config/settings.ini", &subpath);
    uassert_not_null(fs);
    uassert_true(fs == g_mock_fs2);
    uassert_str_equal(subpath, "settings.ini");
    
    // 测试第三个挂载点
    fs = VFS::resolve("/log/system.log", &subpath);
    uassert_not_null(fs);
    uassert_true(fs == g_mock_fs3);
    uassert_str_equal(subpath, "system.log");
    
    // 测试不存在的挂载点
    fs = VFS::resolve("/invalid/file.txt", &subpath);
    uassert_null(fs);
}

/**
 * @brief 测试用例：多挂载点并发文件操作
 */
RT_UNUSED static void test_vfs_multi_mount_concurrent(void)
{
    g_mock_fs->reset_counters();
    g_mock_fs2->reset_counters();
    g_mock_fs3->reset_counters();
    
    // 在第一个文件系统上打开文件
    fd_t fd1 = g_mock_fs->open("data1.bin", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd1 > 0);
    const char data1[] = "Data FS 1";
    g_mock_fs->write(fd1, (void *)data1, strlen(data1));
    
    // 在第二个文件系统上打开文件
    fd_t fd2 = g_mock_fs2->open("config.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd2 > 0);
    const char data2[] = "Config FS 2";
    g_mock_fs2->write(fd2, (void *)data2, strlen(data2));
    
    // 在第三个文件系统上打开文件
    fd_t fd3 = g_mock_fs3->open("app.log", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd3 > 0);
    const char data3[] = "Log FS 3";
    g_mock_fs3->write(fd3, (void *)data3, strlen(data3));
    
    // 验证每个文件系统的忙状态
    uassert_true(g_mock_fs->busy());
    uassert_true(g_mock_fs2->busy());
    uassert_true(g_mock_fs3->busy());
    
    // 读取第一个文件系统的数据
    g_mock_fs->lseek(fd1, 0, SEEK::SET);
    char read_buf1[32] = {0};
    g_mock_fs->read(fd1, read_buf1, strlen(data1));
    uassert_str_equal(read_buf1, data1);
    
    // 读取第二个文件系统的数据
    g_mock_fs2->lseek(fd2, 0, SEEK::SET);
    char read_buf2[32] = {0};
    g_mock_fs2->read(fd2, read_buf2, strlen(data2));
    uassert_str_equal(read_buf2, data2);
    
    // 读取第三个文件系统的数据
    g_mock_fs3->lseek(fd3, 0, SEEK::SET);
    char read_buf3[32] = {0};
    g_mock_fs3->read(fd3, read_buf3, strlen(data3));
    uassert_str_equal(read_buf3, data3);
    
    // 关闭所有文件
    g_mock_fs->close(fd1);
    g_mock_fs2->close(fd2);
    g_mock_fs3->close(fd3);
    
    // 验证所有文件系统不忙
    uassert_false(g_mock_fs->busy());
    uassert_false(g_mock_fs2->busy());
    uassert_false(g_mock_fs3->busy());
    
    // 验证操作计数
    uassert_int_equal(g_mock_fs->get_open_count(), 1);
    uassert_int_equal(g_mock_fs2->get_open_count(), 1);
    uassert_int_equal(g_mock_fs3->get_open_count(), 1);
}

/**
 * @brief 测试用例：多挂载点跨文件系统操作
 */
RT_UNUSED static void test_vfs_multi_mount_cross_fs(void)
{
    g_mock_fs->reset_counters();
    g_mock_fs2->reset_counters();
    g_mock_fs3->reset_counters();
    
    // 在不同文件系统上打开多个文件
    fd_t fd1 = g_mock_fs->open("file1.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    fd_t fd2 = g_mock_fs2->open("file2.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    fd_t fd3 = g_mock_fs->open("file3.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    fd_t fd4 = g_mock_fs3->open("file4.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    
    // 验证所有文件都成功打开
    uassert_true(fd1 > 0);
    uassert_true(fd2 > 0);
    uassert_true(fd3 > 0);
    uassert_true(fd4 > 0);
    
    // 验证引用计数
    uassert_int_equal(g_mock_fs->get_open_count(), 2);  // fd1, fd3
    uassert_int_equal(g_mock_fs2->get_open_count(), 1); // fd2
    uassert_int_equal(g_mock_fs3->get_open_count(), 1); // fd4
    
    // 写入不同数据
    g_mock_fs->write(fd1, (void *)"AAA", 3);
    g_mock_fs2->write(fd2, (void *)"BBB", 3);
    g_mock_fs->write(fd3, (void *)"CCC", 3);
    g_mock_fs3->write(fd4, (void *)"DDD", 3);
    
    // 验证读取
    char buf[4] = {0};
    
    g_mock_fs->lseek(fd1, 0, SEEK::SET);
    g_mock_fs->read(fd1, buf, 3);
    uassert_str_equal(buf, "AAA");
    
    memset(buf, 0, sizeof(buf));
    g_mock_fs2->lseek(fd2, 0, SEEK::SET);
    g_mock_fs2->read(fd2, buf, 3);
    uassert_str_equal(buf, "BBB");
    
    memset(buf, 0, sizeof(buf));
    g_mock_fs->lseek(fd3, 0, SEEK::SET);
    g_mock_fs->read(fd3, buf, 3);
    uassert_str_equal(buf, "CCC");
    
    memset(buf, 0, sizeof(buf));
    g_mock_fs3->lseek(fd4, 0, SEEK::SET);
    g_mock_fs3->read(fd4, buf, 3);
    uassert_str_equal(buf, "DDD");
    
    // 关闭所有文件
    g_mock_fs->close(fd1);
    g_mock_fs2->close(fd2);
    g_mock_fs->close(fd3);
    g_mock_fs3->close(fd4);
}

/**
 * @brief 测试用例：多挂载点动态挂载卸载
 */
RT_UNUSED static void test_vfs_multi_mount_dynamic(void)
{
    MockFileSystem temp_fs;
    const char *subpath = nullptr;
    const IFileSystem *fs = nullptr;
    
    // 初始状态：已有三个挂载点
    fs = VFS::resolve("/data/test.txt", &subpath);
    uassert_not_null(fs);
    
    // 动态添加第四个挂载点
    bool result = VFS::mount("/temp", &temp_fs);
    uassert_true(result);
    
    // 验证新挂载点可用
    fs = VFS::resolve("/temp/file.txt", &subpath);
    uassert_not_null(fs);
    uassert_true(fs == &temp_fs);
    
    // 卸载中间的一个挂载点
    result = VFS::unmount("/config");
    uassert_true(result);
    
    // 验证被卸载的挂载点不可用
    fs = VFS::resolve("/config/test.txt", &subpath);
    uassert_null(fs);
    
    // 验证其他挂载点仍然可用
    fs = VFS::resolve("/data/test.txt", &subpath);
    uassert_not_null(fs);
    uassert_true(fs == g_mock_fs);
    
    fs = VFS::resolve("/log/test.txt", &subpath);
    uassert_not_null(fs);
    uassert_true(fs == g_mock_fs3);
    
    fs = VFS::resolve("/temp/test.txt", &subpath);
    uassert_not_null(fs);
    uassert_true(fs == &temp_fs);
    
    // 重新挂载config
    result = VFS::mount("/config", g_mock_fs2);
    uassert_true(result);
    
    fs = VFS::resolve("/config/test.txt", &subpath);
    uassert_not_null(fs);
    uassert_true(fs == g_mock_fs2);
    
    // 清理临时挂载点
    VFS::unmount("/temp");
}

// 导出测试用例（必须在extern "C"块中）
UTEST_TC_EXPORT(test_vfs_multiple_files, "vfs.01.multiple_files", utest_tc_init, utest_tc_cleanup, 10);
UTEST_TC_EXPORT(test_vfs_path_resolve, "vfs.02.path_resolve", utest_tc_init, utest_tc_cleanup, 10);
UTEST_TC_EXPORT(test_vfs_file_truncate, "vfs.03.file_truncate", utest_tc_init, utest_tc_cleanup, 10);
UTEST_TC_EXPORT(test_vfs_file_append, "vfs.04.file_append", utest_tc_init, utest_tc_cleanup, 10);
UTEST_TC_EXPORT(test_vfs_seek_boundary, "vfs.05.seek_boundary", utest_tc_init, utest_tc_cleanup, 10);
UTEST_TC_EXPORT(test_vfs_invalid_fd, "vfs.06.invalid_fd", utest_tc_init, utest_tc_cleanup, 10);
UTEST_TC_EXPORT(test_vfs_double_close, "vfs.07.double_close", utest_tc_init, utest_tc_cleanup, 10);
UTEST_TC_EXPORT(test_vfs_use_closed_fd, "vfs.08.use_closed_fd", utest_tc_init, utest_tc_cleanup, 10);
UTEST_TC_EXPORT(test_vfs_mount_unmount, "vfs.09.mount_unmount", utest_tc_init, utest_tc_cleanup, 10);
UTEST_TC_EXPORT(test_vfs_null_pointer, "vfs.10.null_pointer", utest_tc_init, utest_tc_cleanup, 10);

// 多挂载点测试用例
UTEST_TC_EXPORT(test_vfs_multi_mount_resolve, "vfs.11.multi_mount_resolve", utest_multi_mount_init, utest_multi_mount_cleanup, 10);
UTEST_TC_EXPORT(test_vfs_multi_mount_concurrent, "vfs.12.multi_mount_concurrent", utest_multi_mount_init, utest_multi_mount_cleanup, 10);
UTEST_TC_EXPORT(test_vfs_multi_mount_cross_fs, "vfs.13.multi_mount_cross_fs", utest_multi_mount_init, utest_multi_mount_cleanup, 10);
UTEST_TC_EXPORT(test_vfs_multi_mount_dynamic, "vfs.14.multi_mount_dynamic", utest_multi_mount_init, utest_multi_mount_cleanup, 10);

} // extern "C"
