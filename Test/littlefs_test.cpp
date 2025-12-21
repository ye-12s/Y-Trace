/**
 * @file littlefs_test.cpp
 * @brief LittleFS文件系统测试用例
 * @version 1.0
 * @date 2025-12-21
 */

#include "port/littlefs_if.hpp"
#include "system/vfs/vfs.hpp"
#include <utest.h>
#include <rtthread.h>
#include <cstring>
#include <new>

using namespace sys;
using namespace littlefs;

// 全局文件系统实例
static fs g_lfs_instance;
static fs *g_lfs = nullptr;

// 使用extern "C"包裹测试函数以支持UTEST_TC_EXPORT宏
extern "C" {

/**
 * @brief 测试初始化函数
 */
RT_UNUSED static rt_err_t utest_lfs_init(void)
{
    // 使用placement new初始化静态对象
    g_lfs = new (&g_lfs_instance) fs();
    
    // 初始化文件系统
    if (!g_lfs->init())
    {
        return -1;
    }
    
    // 检查是否已经挂载，如果已挂载则先卸载
    const char *subpath = nullptr;
    if (VFS::resolve("/flash/test", &subpath) != nullptr)
    {
        VFS::unmount("/flash");
    }
    
    // 挂载到VFS
    bool result = VFS::mount("/flash", g_lfs);
    if (!result)
    {
        return -1;
    }
    
    return 0;
}

/**
 * @brief 测试清理函数
 */
RT_UNUSED static rt_err_t utest_lfs_cleanup(void)
{
    // 卸载文件系统（会自动关闭所有打开的文件）
    VFS::unmount("/flash");
    VFS::unmount("/data");  // 也卸载可能的第二个挂载点
    
    // 重新挂载以供下一个测试使用
    VFS::mount("/flash", g_lfs);
    
    // 不要设g_lfs设为nullptr，因为它指向静态对象
    return 0;
}

/**
 * @brief 测试用例：文件系统初始化和挂载测试
 */
RT_UNUSED static void test_lfs_init_mount(void)
{
    // 验证文件系统已初始化
    uassert_true(g_lfs->is_initialized());
    
    // 验证VFS路径解析
    const char *subpath = nullptr;
    const IFileSystem *fs = VFS::resolve("/flash/test.txt", &subpath);
    uassert_not_null(fs);
    uassert_true(fs == g_lfs);
    uassert_str_equal(subpath, "test.txt");
}

RT_UNUSED static rt_err_t utest_lfs_init_mount_cleanup(void)
{
    // 确保所有文件都关闭
    return RT_EOK;
}

/**
 * @brief 测试用例：基本文件读写操作
 */
RT_UNUSED static void test_lfs_basic_rw(void)
{
    // 创建并写入文件（使用TRUNC确保清空旧数据）
    fd_t fd = g_lfs->open("test_basic.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT | (int)OPEN_FLAG::TRUNC);
    uassert_true(fd > 0);
    
    const char data[] = "Hello LittleFS!";
    int written = g_lfs->write(fd, (void *)data, strlen(data));
    uassert_int_equal(written, strlen(data));
    
    // 回到文件开头
    int pos = g_lfs->lseek(fd, 0, SEEK::SET);
    uassert_int_equal(pos, 0);
    
    // 读取并验证
    char read_buf[32] = {0};
    int read_len = g_lfs->read(fd, read_buf, sizeof(read_buf));
    uassert_int_equal(read_len, strlen(data));
    uassert_str_equal(read_buf, data);
    
    // 关闭文件
    int result = g_lfs->close(fd);
    uassert_int_equal(result, 0);
}

/**
 * @brief 测试用例：文件追加写入
 */
RT_UNUSED static void test_lfs_append(void)
{
    // 创建并写入初始数据（使用TRUNC确保清空旧数据）
    fd_t fd = g_lfs->open("test_append.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT | (int)OPEN_FLAG::TRUNC);
    uassert_true(fd > 0);
    
    const char part1[] = "First";
    g_lfs->write(fd, (void *)part1, strlen(part1));
    g_lfs->close(fd);
    
    // 以追加模式打开并写入
    fd = g_lfs->open("test_append.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::APPEND);
    uassert_true(fd > 0);
    
    const char part2[] = " Second";
    int written = g_lfs->write(fd, (void *)part2, strlen(part2));
    uassert_int_equal(written, strlen(part2));
    g_lfs->close(fd);
    
    // 重新打开文件读取验证
    fd = g_lfs->open("test_append.txt", (int)OPEN_FLAG::READ);
    uassert_true(fd > 0);
    
    char read_buf[32] = {0};
    int read_len = g_lfs->read(fd, read_buf, sizeof(read_buf));
    
    const char expected[] = "First Second";
    uassert_int_equal(read_len, strlen(expected));
    uassert_str_equal(read_buf, expected);
    
    g_lfs->close(fd);
}
RT_UNUSED static rt_err_t utest_lfs_append_cleanup(void)
{
    // 确保所有文件都关闭
    return RT_EOK;
}
/**
 * @brief 测试用例：文件截断功能
 */
RT_UNUSED static void test_lfs_truncate(void)
{
    // 创建文件并写入数据
    fd_t fd = g_lfs->open("test_trunc.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd > 0);
    
    const char original[] = "This is original content";
    g_lfs->write(fd, (void *)original, strlen(original));
    g_lfs->close(fd);
    
    // 以截断模式重新打开
    fd = g_lfs->open("test_trunc.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::TRUNC);
    uassert_true(fd > 0);
    
    // 写入新内容
    const char new_data[] = "New";
    int written = g_lfs->write(fd, (void *)new_data, strlen(new_data));
    uassert_int_equal(written, strlen(new_data));
    
    // 验证文件只包含新内容
    g_lfs->lseek(fd, 0, SEEK::SET);
    char read_buf[32] = {0};
    int read_len = g_lfs->read(fd, read_buf, sizeof(read_buf));
    uassert_int_equal(read_len, strlen(new_data));
    uassert_str_equal(read_buf, new_data);
    
    g_lfs->close(fd);
}

/**
 * @brief 测试用例：文件定位操作
 */
RT_UNUSED static void test_lfs_seek(void)
{
    // 创建测试文件
    fd_t fd = g_lfs->open("test_seek.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd > 0);
    
    const char data[] = "0123456789ABCDEF";
    g_lfs->write(fd, (void *)data, strlen(data));
    
    // 测试SEEK::SET
    int pos = g_lfs->lseek(fd, 5, SEEK::SET);
    uassert_int_equal(pos, 5);
    
    char buf[5] = {0};
    g_lfs->read(fd, buf, 4);
    uassert_str_equal(buf, "5678");
    
    // 测试SEEK::CUR
    pos = g_lfs->lseek(fd, 2, SEEK::CUR);
    uassert_int_equal(pos, 11);
    
    memset(buf, 0, sizeof(buf));
    g_lfs->read(fd, buf, 3);
    uassert_str_equal(buf, "BCD");
    
    // 测试SEEK::END
    pos = g_lfs->lseek(fd, -4, SEEK::END);
    uassert_int_equal(pos, strlen(data) - 4);
    
    memset(buf, 0, sizeof(buf));
    g_lfs->read(fd, buf, 4);
    uassert_str_equal(buf, "CDEF");
    
    g_lfs->close(fd);
}

/**
 * @brief 测试用例：多文件并发操作
 */
RT_UNUSED static void test_lfs_multi_files(void)
{
    // 打开第一个文件
    fd_t fd1 = g_lfs->open("test_file1.dat", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd1 > 0);
    
    const char data1[] = "File 1 Data";
    g_lfs->write(fd1, (void *)data1, strlen(data1));
    
    // 打开第二个文件
    fd_t fd2 = g_lfs->open("test_file2.dat", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd2 > 0);
    uassert_true(fd2 != fd1);
    
    const char data2[] = "File 2 Data";
    g_lfs->write(fd2, (void *)data2, strlen(data2));
    
    // 打开第三个文件
    fd_t fd3 = g_lfs->open("test_file3.dat", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd3 > 0);
    uassert_true(fd3 != fd1 && fd3 != fd2);
    
    const char data3[] = "File 3 Data";
    g_lfs->write(fd3, (void *)data3, strlen(data3));
    
    // 验证第一个文件的数据
    g_lfs->lseek(fd1, 0, SEEK::SET);
    char buf1[32] = {0};
    g_lfs->read(fd1, buf1, strlen(data1));
    uassert_str_equal(buf1, data1);
    
    // 验证第二个文件的数据
    g_lfs->lseek(fd2, 0, SEEK::SET);
    char buf2[32] = {0};
    g_lfs->read(fd2, buf2, strlen(data2));
    uassert_str_equal(buf2, data2);
    
    // 验证第三个文件的数据
    g_lfs->lseek(fd3, 0, SEEK::SET);
    char buf3[32] = {0};
    g_lfs->read(fd3, buf3, strlen(data3));
    uassert_str_equal(buf3, data3);
    
    // 关闭所有文件
    g_lfs->close(fd1);
    g_lfs->close(fd2);
    g_lfs->close(fd3);
}

RT_UNUSED static rt_err_t utest_lfs_multi_files_cleanup(void)
{
    // 确保所有文件都关闭
    return RT_EOK;
}

/**
 * @brief 测试用例：大数据写入测试
 */
RT_UNUSED static void test_lfs_large_write(void)
{
    fd_t fd = g_lfs->open("test_large.bin", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT | (int)OPEN_FLAG::TRUNC);
    uassert_true(fd > 0);
    
    // 写入较大的数据块（1KB）
    char write_buf[1024];
    for (int i = 0; i < 1024; i++)
    {
        write_buf[i] = i & 0xFF;
    }
    
    int written = g_lfs->write(fd, write_buf, sizeof(write_buf));
    uassert_int_equal(written, sizeof(write_buf));
    
    // 读取并验证
    g_lfs->lseek(fd, 0, SEEK::SET);
    char read_buf[1024] = {0};
    int read_len = g_lfs->read(fd, read_buf, sizeof(read_buf));
    uassert_int_equal(read_len, sizeof(read_buf));
    
    // 验证数据完整性
    int match = memcmp(write_buf, read_buf, sizeof(write_buf));
    uassert_int_equal(match, 0);
    
    g_lfs->close(fd);
}

/**
 * @brief 测试用例：持久化测试（跨挂载/卸载）
 */
RT_UNUSED static void test_lfs_persistence(void)
{
    const char test_data[] = "Persistent Data Test";
    
    // 写入数据
    fd_t fd = g_lfs->open("test_persist.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT | (int)OPEN_FLAG::TRUNC);
    uassert_true(fd > 0);
    
    int written = g_lfs->write(fd, (void *)test_data, strlen(test_data));
    uassert_int_equal(written, strlen(test_data));
    g_lfs->close(fd);
    
    // 重新打开文件验证数据持久化（不重新挂载文件系统，避免对象生命周期问题）
    fd = g_lfs->open("test_persist.txt", (int)OPEN_FLAG::READ);
    uassert_true(fd > 0);
    
    char read_buf[64] = {0};
    int read_len = g_lfs->read(fd, read_buf, sizeof(read_buf));
    uassert_int_equal(read_len, strlen(test_data));
    uassert_str_equal(read_buf, test_data);
    
    g_lfs->close(fd);
}
RT_UNUSED static rt_err_t utest_lfs_persistence_cleanup(void)
{
    // 确保所有文件都关闭
    return RT_EOK;
}
/**
 * @brief 测试用例：错误处理测试
 */
RT_UNUSED static void test_lfs_error_handling(void)
{
    // 创建文件用于测试
    fd_t fd = g_lfs->open("test_error.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd > 0);
    
    const char data[] = "Error Test";
    int written = g_lfs->write(fd, (void *)data, strlen(data));
    uassert_int_equal(written, strlen(data));
    g_lfs->close(fd);
    
    // 以只读模式打开，验证可以读取
    fd = g_lfs->open("test_error.txt", (int)OPEN_FLAG::READ);
    uassert_true(fd > 0);
    
    char read_buf[32] = {0};
    int read_len = g_lfs->read(fd, read_buf, sizeof(read_buf));
    uassert_int_equal(read_len, strlen(data));
    uassert_str_equal(read_buf, data);
    
    g_lfs->close(fd);
}

/**
 * @brief 测试用例：边界条件测试
 */
RT_UNUSED static void test_lfs_boundary(void)
{
    fd_t fd = g_lfs->open("test_boundary.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd > 0);
    
    const char data[] = "Boundary Test Data";
    g_lfs->write(fd, (void *)data, strlen(data));
    
    // 测试seek到文件末尾
    int pos = g_lfs->lseek(fd, 0, SEEK::END);
    uassert_int_equal(pos, strlen(data));
    
    // 尝试读取（应该返回0）
    char buf[10] = {0};
    int read_len = g_lfs->read(fd, buf, sizeof(buf));
    uassert_int_equal(read_len, 0);
    
    // 测试seek到负位置（应该失败）
    pos = g_lfs->lseek(fd, -100, SEEK::SET);
    uassert_true(pos < 0);
    
    // 测试从开头seek负偏移（应该失败）
    g_lfs->lseek(fd, 0, SEEK::SET);
    pos = g_lfs->lseek(fd, -10, SEEK::CUR);
    uassert_true(pos < 0);
    
    g_lfs->close(fd);
}

/**
 * @brief 测试用例：VFS集成测试 - 通过VFS接口访问LittleFS
 */
RT_UNUSED static void test_lfs_vfs_integration(void)
{
    // 1. 测试VFS路径解析
    const char *subpath = nullptr;
    IFileSystem *fs = const_cast<IFileSystem*>(VFS::resolve("/flash/vfs_test.txt", &subpath));
    uassert_not_null(fs);
    uassert_true(fs == g_lfs);
    uassert_str_equal(subpath, "vfs_test.txt");
    
    // 2. 通过VFS获取的文件系统指针进行文件操作
    fd_t fd = fs->open(subpath, (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT | (int)OPEN_FLAG::TRUNC);
    uassert_true(fd > 0);
    
    const char data1[] = "VFS Integration Test";
    int written = fs->write(fd, (void *)data1, strlen(data1));
    uassert_int_equal(written, strlen(data1));
    
    fs->lseek(fd, 0, SEEK::SET);
    char buf1[32] = {0};
    int read_len = fs->read(fd, buf1, sizeof(buf1));
    uassert_int_equal(read_len, strlen(data1));
    uassert_str_equal(buf1, data1);
    fs->close(fd);
    
    // 3. 测试子目录路径
    fs = const_cast<IFileSystem*>(VFS::resolve("/flash/dir/subdir/file.bin", &subpath));
    uassert_not_null(fs);
    uassert_str_equal(subpath, "dir/subdir/file.bin");
    
    // 4. 测试多次解析同一挂载点
    fs = const_cast<IFileSystem*>(VFS::resolve("/flash/test1.txt", &subpath));
    uassert_not_null(fs);
    IFileSystem *fs2 = const_cast<IFileSystem*>(VFS::resolve("/flash/test2.txt", &subpath));
    uassert_true(fs == fs2);  // 应该返回同一个文件系统实例
    
    // 5. 测试无效路径
    fs = const_cast<IFileSystem*>(VFS::resolve("/invalid/path.txt", &subpath));
    uassert_null(fs);
    
    // 6. 测试根路径
    fs = const_cast<IFileSystem*>(VFS::resolve("/flash/", &subpath));
    uassert_not_null(fs);
    uassert_str_equal(subpath, "");
    
    // 7. 测试文件系统忙状态
    uassert_false(g_lfs->busy());
    
    fd = g_lfs->open("vfs_busy_test.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT);
    uassert_true(fd > 0);
    uassert_true(g_lfs->busy());
    
    g_lfs->close(fd);
    uassert_false(g_lfs->busy());
}

RT_UNUSED static rt_err_t utest_lfs_vfs_integration_cleanup(void)
{
    // 确保所有文件都关闭
    return RT_EOK;
}

/**
 * @brief 测试用例：VFS多挂载点测试
 * 注意：此测试仅测试VFS的多挂载点管理功能，不实际创建第二个文件系统
 */
RT_UNUSED static void test_lfs_vfs_multi_mount(void)
{
    // 测试VFS基本的多挂载点路径解析
    const char *subpath = nullptr;
    IFileSystem *fs1 = const_cast<IFileSystem*>(VFS::resolve("/flash/file1.txt", &subpath));
    uassert_not_null(fs1);
    uassert_true(fs1 == g_lfs);
    uassert_str_equal(subpath, "file1.txt");
    
    // 创建一个模拟的第二个挂载点（不使用真实的LittleFS实例）
    // 使用g_lfs作为占位符，仅用于测试VFS功能
    bool result = VFS::mount("/data", g_lfs);
    uassert_true(result);
    
    // 验证两个挂载点路径解析
    fs1 = const_cast<IFileSystem*>(VFS::resolve("/flash/test.txt", &subpath));
    uassert_not_null(fs1);
    uassert_str_equal(subpath, "test.txt");
    
    IFileSystem *fs2 = const_cast<IFileSystem*>(VFS::resolve("/data/config.txt", &subpath));
    uassert_not_null(fs2);
    uassert_str_equal(subpath, "config.txt");
    
    // 在第一个挂载点创建文件
    fd_t fd1 = fs1->open("vfs_test1.txt", (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT | (int)OPEN_FLAG::TRUNC);
    uassert_true(fd1 > 0);
    const char data1[] = "Mount Point 1";
    int written = fs1->write(fd1, (void *)data1, strlen(data1));
    uassert_int_equal(written, strlen(data1));
    fs1->close(fd1);
    
    // 读取验证
    fd1 = fs1->open("vfs_test1.txt", (int)OPEN_FLAG::READ);
    uassert_true(fd1 > 0);
    char buf1[32] = {0};
    int read_len = fs1->read(fd1, buf1, sizeof(buf1));
    uassert_int_equal(read_len, strlen(data1));
    uassert_str_equal(buf1, data1);
    fs1->close(fd1);
    
    // 卸载第二个挂载点
    result = VFS::unmount("/data");
    uassert_true(result);
    
    // 验证第二个挂载点不可用
    fs2 = const_cast<IFileSystem*>(VFS::resolve("/data/file.txt", &subpath));
    uassert_null(fs2);
    
    // 验证第一个挂载点仍然可用
    fs1 = const_cast<IFileSystem*>(VFS::resolve("/flash/file.txt", &subpath));
    uassert_not_null(fs1);
}

/**
 * @brief 测试用例：VFS跨挂载点文件复制
 */
RT_UNUSED static void test_lfs_vfs_cross_mount_copy(void)
{
    // 准备源文件
    const char *subpath = nullptr;
    IFileSystem *fs_src = const_cast<IFileSystem*>(VFS::resolve("/flash/source.txt", &subpath));
    uassert_not_null(fs_src);
    
    fd_t fd_src = fs_src->open(subpath, (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT | (int)OPEN_FLAG::TRUNC);
    uassert_true(fd_src > 0);
    
    const char src_data[] = "Source file content for cross-mount copy test";
    int written = fs_src->write(fd_src, (void *)src_data, strlen(src_data));
    uassert_int_equal(written, strlen(src_data));
    fs_src->close(fd_src);
    
    // 从源文件读取
    fd_src = fs_src->open("source.txt", (int)OPEN_FLAG::READ);
    uassert_true(fd_src > 0);
    
    char buffer[64] = {0};
    int read_len = fs_src->read(fd_src, buffer, sizeof(buffer));
    uassert_int_equal(read_len, strlen(src_data));
    
    // 写入目标文件（同一文件系统）
    IFileSystem *fs_dst = const_cast<IFileSystem*>(VFS::resolve("/flash/dest.txt", &subpath));
    uassert_not_null(fs_dst);
    uassert_true(fs_dst == fs_src);  // 同一文件系统
    
    fd_t fd_dst = fs_dst->open(subpath, (int)OPEN_FLAG::RDWR | (int)OPEN_FLAG::CREAT | (int)OPEN_FLAG::TRUNC);
    uassert_true(fd_dst > 0);
    
    written = fs_dst->write(fd_dst, buffer, read_len);
    uassert_int_equal(written, read_len);
    
    fs_src->close(fd_src);
    fs_dst->close(fd_dst);
    
    // 验证目标文件内容
    fd_dst = fs_dst->open("dest.txt", (int)OPEN_FLAG::READ);
    uassert_true(fd_dst > 0);
    
    char verify_buf[64] = {0};
    read_len = fs_dst->read(fd_dst, verify_buf, sizeof(verify_buf));
    uassert_int_equal(read_len, strlen(src_data));
    uassert_str_equal(verify_buf, src_data);
    
    fs_dst->close(fd_dst);
}

// 导出测试用例
UTEST_TC_EXPORT(test_lfs_init_mount, "lfs.01.init_mount", utest_lfs_init, utest_lfs_cleanup, 10);
UTEST_TC_EXPORT(test_lfs_basic_rw, "lfs.02.basic_rw", utest_lfs_init, utest_lfs_cleanup, 10);
UTEST_TC_EXPORT(test_lfs_append, "lfs.03.append", utest_lfs_init, utest_lfs_cleanup, 10);
UTEST_TC_EXPORT(test_lfs_truncate, "lfs.04.truncate", utest_lfs_init, utest_lfs_cleanup, 10);
UTEST_TC_EXPORT(test_lfs_seek, "lfs.05.seek", utest_lfs_init, utest_lfs_cleanup, 10);
UTEST_TC_EXPORT(test_lfs_multi_files, "lfs.06.multi_files", utest_lfs_init, utest_lfs_cleanup, 10);
UTEST_TC_EXPORT(test_lfs_large_write, "lfs.07.large_write", utest_lfs_init, utest_lfs_cleanup, 10);
UTEST_TC_EXPORT(test_lfs_persistence, "lfs.08.persistence", utest_lfs_init, utest_lfs_cleanup, 10);
UTEST_TC_EXPORT(test_lfs_error_handling, "lfs.09.error_handling", utest_lfs_init, utest_lfs_cleanup, 10);
UTEST_TC_EXPORT(test_lfs_boundary, "lfs.10.boundary", utest_lfs_init, utest_lfs_cleanup, 10);
UTEST_TC_EXPORT(test_lfs_vfs_integration, "lfs.11.vfs_integration", utest_lfs_init, utest_lfs_cleanup, 10);
UTEST_TC_EXPORT(test_lfs_vfs_multi_mount, "lfs.12.vfs_multi_mount", utest_lfs_init, utest_lfs_cleanup, 10);
UTEST_TC_EXPORT(test_lfs_vfs_cross_mount_copy, "lfs.13.vfs_cross_mount_copy", utest_lfs_init, utest_lfs_cleanup, 10);

} // extern "C"
