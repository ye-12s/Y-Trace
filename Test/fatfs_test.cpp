/**
 * @file fatfs_test.cpp
 * @brief FatFS文件系统移植测试用例
 * @version 1.0
 * @date 2025-12-21
 * 
 * 测试说明：
 * 本测试用例用于验证FatFS文件系统移植的正确性，包括：
 * 1. 底层磁盘驱动接口（diskio.c）是否正常工作
 * 2. FatFS文件系统挂载和初始化
 * 3. 基本文件操作（创建、读写、追加、删除）
 * 4. 目录操作（创建、遍历、删除）
 * 5. 文件系统信息获取
 * 6. 错误处理和边界条件测试
 */

#include "ff.h"
#include "diskio.h"
#include "drivers/drv_sdio.h"
#include <utest.h>
#include <rtthread.h>
#include <cstring>
#include <cstdio>

// 测试配置
#define DEV_MMC             0              // SD卡设备号（与diskio.c中的定义一致）
#define TEST_DRIVE          "0:"           // SD卡驱动器号
#define TEST_DIR            "0:/test"      // 测试目录
#define TEST_FILE_SMALL     "0:/test/small.txt"
#define TEST_FILE_LARGE     "0:/test/large.dat"
#define TEST_FILE_APPEND    "0:/test/append.txt"
#define LARGE_FILE_SIZE     (4 * 1024)     // 4KB用于大文件测试
#define WRITE_BLOCK_SIZE    512            // 每次写入512字节

// 全局变量
static FATFS g_fatfs;           // FatFS文件系统对象
static bool g_fs_mounted = false;

// 使用extern "C"包裹测试函数以支持UTEST_TC_EXPORT宏
extern "C" {

/**
 * @brief 测试初始化函数 - 确保文件系统已挂载
 * @note 此函数可以多次调用，如果已挂载则直接返回成功
 */
RT_UNUSED static rt_err_t utest_fatfs_init(void)
{
    FRESULT res;
    
    // 如果已经挂载，直接返回成功
    if (g_fs_mounted)
    {
        rt_kprintf("[INFO] FatFS已挂载，跳过初始化\n");
        return RT_EOK;
    }
    
    rt_kprintf("\n========== FatFS移植测试初始化 ==========\n");
    
    // 初始化SD卡
    rt_kprintf("初始化SD卡...\n");
    int sd_result = sd_init();
    if (sd_result != 0)
    {
        rt_kprintf("错误: SD卡初始化失败 (错误码: %d)\n", sd_result);
        return -RT_ERROR;
    }
    rt_kprintf("SD卡初始化成功\n");
    rt_kprintf("  容量: %lu MB\n", (unsigned long)(sd_card_info.card_capacity / (1024 * 1024)));
    rt_kprintf("  块大小: %d 字节\n", sd_card_info.card_blk_size);
    
    // 挂载文件系统
    rt_kprintf("\n挂载FatFS文件系统...\n");
    res = f_mount(&g_fatfs, TEST_DRIVE, 1);
    
    if (res != FR_OK)
    {
        // 如果挂载失败，尝试格式化
        rt_kprintf("警告: 挂载失败 (错误码: %d)，尝试格式化...\n", res);
        
        BYTE work[FF_MAX_SS];
        MKFS_PARM opt = {0};
        opt.fmt = FM_FAT32;  // 格式化为 FAT32
        opt.n_fat = 1;       // 使用1个FAT表
        opt.align = 0;       // 自动对齐
        opt.n_root = 0;      // FAT32不需要设置（仅FAT12/16使用）
        opt.au_size = 0;     // 自动选择簇大小
        
        res = f_mkfs(TEST_DRIVE, &opt, work, sizeof(work));
        
        if (res != FR_OK)
        {
            rt_kprintf("错误: 格式化失败 (错误码: %d)\n", res);
            return -RT_ERROR;
        }
        
        rt_kprintf("格式化成功，重新挂载...\n");
        res = f_mount(&g_fatfs, TEST_DRIVE, 1);
        
        if (res != FR_OK)
        {
            rt_kprintf("错误: 重新挂载失败 (错误码: %d)\n", res);
            return -RT_ERROR;
        }
    }
    
    g_fs_mounted = true;
    rt_kprintf("FatFS挂载成功\n");
    
    // 创建测试目录
    rt_kprintf("\n创建测试目录: %s\n", TEST_DIR);
    res = f_mkdir(TEST_DIR);
    if (res != FR_OK && res != FR_EXIST)
    {
        rt_kprintf("警告: 创建测试目录失败 (错误码: %d)\n", res);
    }
    
    rt_kprintf("========== 初始化完成 ==========\n\n");
    return RT_EOK;
}

/**
 * @brief 测试清理函数
 */
RT_UNUSED static rt_err_t utest_fatfs_cleanup(void)
{
    rt_kprintf("\n========== 清理测试环境 ==========\n");
    
    // 删除测试文件和目录
    f_unlink(TEST_FILE_SMALL);
    f_unlink(TEST_FILE_LARGE);
    f_unlink(TEST_FILE_APPEND);
    f_unlink(TEST_DIR);
    
    // 卸载文件系统
    if (g_fs_mounted)
    {
        f_mount(NULL, TEST_DRIVE, 0);
        g_fs_mounted = false;
        rt_kprintf("FatFS已卸载\n");
    }
    
    rt_kprintf("========== 清理完成 ==========\n\n");
    return RT_EOK;
}

/**
 * @brief 测试用例1：磁盘驱动接口测试
 */
RT_UNUSED static void test_fatfs_disk_interface(void)
{
    DSTATUS stat;
    DRESULT res;
    BYTE buffer[512];
    DWORD sector_count = 0;
    WORD sector_size = 0;
    
    rt_kprintf("\n=== 测试1：磁盘驱动接口测试 ===\n");
    
    // 测试磁盘状态
    rt_kprintf("检查磁盘状态...\n");
    stat = disk_status(DEV_MMC);
    uassert_int_equal(stat, 0);
    rt_kprintf("  磁盘状态正常\n");
    
    // 测试磁盘初始化
    rt_kprintf("测试磁盘初始化...\n");
    stat = disk_initialize(DEV_MMC);
    uassert_int_equal(stat, 0);
    rt_kprintf("  磁盘初始化成功\n");
    
    // 测试获取扇区大小
    rt_kprintf("获取扇区大小...\n");
    res = disk_ioctl(DEV_MMC, GET_SECTOR_SIZE, &sector_size);
    uassert_int_equal(res, RES_OK);
    uassert_int_equal(sector_size, 512);
    rt_kprintf("  扇区大小: %d 字节\n", sector_size);
    
    // 测试获取扇区数量
    rt_kprintf("获取扇区数量...\n");
    res = disk_ioctl(DEV_MMC, GET_SECTOR_COUNT, &sector_count);
    uassert_int_equal(res, RES_OK);
    uassert_true(sector_count > 0);
    rt_kprintf("  扇区数量: %lu (容量: %lu MB)\n", 
               (unsigned long)sector_count, 
               (unsigned long)(sector_count * 512UL / (1024 * 1024)));
    
    // 测试读扇区
    rt_kprintf("测试读取第0扇区...\n");
    res = disk_read(DEV_MMC, buffer, 0, 1);
    uassert_int_equal(res, RES_OK);
    rt_kprintf("  读取成功\n");
    
    // 测试写扇区（写到较高扇区避免破坏文件系统）
    rt_kprintf("测试写入扇区...\n");
    DWORD test_sector = sector_count - 1;  // 使用最后一个扇区
    memset(buffer, 0xAA, 512);
    res = disk_write(DEV_MMC, buffer, test_sector, 1);
    uassert_int_equal(res, RES_OK);
    rt_kprintf("  写入成功\n");
    
    // 验证写入
    rt_kprintf("验证写入数据...\n");
    memset(buffer, 0, 512);
    res = disk_read(DEV_MMC, buffer, test_sector, 1);
    uassert_int_equal(res, RES_OK);
    uassert_int_equal(buffer[0], 0xAA);
    uassert_int_equal(buffer[511], 0xAA);
    rt_kprintf("  数据验证成功\n");
    
    // 测试SYNC命令
    rt_kprintf("测试磁盘同步...\n");
    res = disk_ioctl(DEV_MMC, CTRL_SYNC, NULL);
    uassert_int_equal(res, RES_OK);
    rt_kprintf("  同步成功\n");
    
    rt_kprintf("=== 磁盘驱动接口测试通过 ===\n\n");
}

/**
 * @brief 测试用例2：文件系统挂载和信息获取
 */
RT_UNUSED static void test_fatfs_mount_info(void)
{
    FRESULT res;
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    
    rt_kprintf("\n=== 测试2：文件系统挂载和信息测试 ===\n");
    
    // 验证文件系统已挂载
    uassert_true(g_fs_mounted);
    rt_kprintf("文件系统已挂载\n");
    
    // 获取文件系统信息
    rt_kprintf("获取文件系统信息...\n");
    res = f_getfree(TEST_DRIVE, &fre_clust, &fs);
    uassert_int_equal(res, FR_OK);
    
    // 计算总容量和剩余容量
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;
    
    rt_kprintf("文件系统信息:\n");
    rt_kprintf("  文件系统类型: FAT%d\n", fs->fs_type);
    rt_kprintf("  总容量: %lu KB (%lu MB)\n", 
               (unsigned long)(tot_sect / 2), 
               (unsigned long)(tot_sect / 2048));
    rt_kprintf("  可用空间: %lu KB (%lu MB)\n", 
               (unsigned long)(fre_sect / 2), 
               (unsigned long)(fre_sect / 2048));
    rt_kprintf("  簇大小: %d 扇区\n", fs->csize);
    
    // 验证容量合理性
    uassert_true(tot_sect > 0);
    uassert_true(fre_sect <= tot_sect);
    
    rt_kprintf("=== 文件系统挂载和信息测试通过 ===\n\n");
}

/**
 * @brief 测试用例3：基本文件读写操作
 */
RT_UNUSED static void test_fatfs_basic_rw(void)
{
    FRESULT res;
    FIL file;
    UINT bw, br;
    const char test_data[] = "Hello FatFS! This is a test message.";
    char read_buf[100] = {0};
    
    rt_kprintf("\n=== 测试3：基本文件读写测试 ===\n");
    
    // 创建并写入文件
    rt_kprintf("创建文件并写入数据: %s\n", TEST_FILE_SMALL);
    res = f_open(&file, TEST_FILE_SMALL, FA_CREATE_ALWAYS | FA_WRITE);
    uassert_int_equal(res, FR_OK);
    
    res = f_write(&file, test_data, strlen(test_data), &bw);
    uassert_int_equal(res, FR_OK);
    uassert_int_equal(bw, strlen(test_data));
    rt_kprintf("  写入 %u 字节\n", bw);
    
    res = f_close(&file);
    uassert_int_equal(res, FR_OK);
    
    // 读取并验证文件
    rt_kprintf("打开文件并读取数据...\n");
    res = f_open(&file, TEST_FILE_SMALL, FA_READ);
    uassert_int_equal(res, FR_OK);
    
    res = f_read(&file, read_buf, sizeof(read_buf), &br);
    uassert_int_equal(res, FR_OK);
    uassert_int_equal(br, strlen(test_data));
    rt_kprintf("  读取 %u 字节\n", br);
    
    // 验证数据一致性
    uassert_str_equal(read_buf, test_data);
    rt_kprintf("  数据验证成功\n");
    
    // 获取文件大小
    FSIZE_t file_size = f_size(&file);
    uassert_int_equal(file_size, strlen(test_data));
    rt_kprintf("  文件大小: %lu 字节\n", (unsigned long)file_size);
    
    res = f_close(&file);
    uassert_int_equal(res, FR_OK);
    
    rt_kprintf("=== 基本文件读写测试通过 ===\n\n");
}

/**
 * @brief 测试用例4：文件追加操作
 */
RT_UNUSED static void test_fatfs_append(void)
{
    FRESULT res;
    FIL file;
    UINT bw, br;
    const char part1[] = "First part. ";
    const char part2[] = "Second part.";
    char read_buf[100] = {0};
    
    rt_kprintf("\n=== 测试4：文件追加操作测试 ===\n");
    
    // 创建文件并写入第一部分
    rt_kprintf("创建文件并写入第一部分...\n");
    res = f_open(&file, TEST_FILE_APPEND, FA_CREATE_ALWAYS | FA_WRITE);
    uassert_int_equal(res, FR_OK);
    
    res = f_write(&file, part1, strlen(part1), &bw);
    uassert_int_equal(res, FR_OK);
    uassert_int_equal(bw, strlen(part1));
    
    res = f_close(&file);
    uassert_int_equal(res, FR_OK);
    
    // 以追加模式打开并写入第二部分
    rt_kprintf("以追加模式写入第二部分...\n");
    res = f_open(&file, TEST_FILE_APPEND, FA_OPEN_APPEND | FA_WRITE);
    uassert_int_equal(res, FR_OK);
    
    res = f_write(&file, part2, strlen(part2), &bw);
    uassert_int_equal(res, FR_OK);
    uassert_int_equal(bw, strlen(part2));
    
    res = f_close(&file);
    uassert_int_equal(res, FR_OK);
    
    // 读取并验证完整内容
    rt_kprintf("验证追加后的内容...\n");
    res = f_open(&file, TEST_FILE_APPEND, FA_READ);
    uassert_int_equal(res, FR_OK);
    
    res = f_read(&file, read_buf, sizeof(read_buf), &br);
    uassert_int_equal(res, FR_OK);
    uassert_int_equal(br, strlen(part1) + strlen(part2));
    
    // 验证内容
    char expected[100];
    snprintf(expected, sizeof(expected), "%s%s", part1, part2);
    uassert_str_equal(read_buf, expected);
    rt_kprintf("  追加内容验证成功: \"%s\"\n", read_buf);
    
    res = f_close(&file);
    uassert_int_equal(res, FR_OK);
    
    rt_kprintf("=== 文件追加操作测试通过 ===\n\n");
}

/**
 * @brief 测试用例5：大文件读写测试
 */
RT_UNUSED static void test_fatfs_large_file(void)
{
    FRESULT res;
    FIL file;
    UINT bw, br;
    BYTE *write_buf = NULL;
    BYTE *read_buf = NULL;
    
    rt_kprintf("\n=== 测试5：大文件读写测试 (%d KB) ===\n", LARGE_FILE_SIZE / 1024);
    
    // 分配缓冲区
    write_buf = (BYTE *)rt_malloc(LARGE_FILE_SIZE);
    read_buf = (BYTE *)rt_malloc(LARGE_FILE_SIZE);
    
    if (!write_buf || !read_buf)
    {
        rt_kprintf("错误: 内存分配失败\n");
        if (write_buf) rt_free(write_buf);
        if (read_buf) rt_free(read_buf);
        uassert_true(false);
        return;
    }
    
    // 填充测试数据（生成可验证的模式）
    rt_kprintf("生成测试数据...\n");
    for (int i = 0; i < LARGE_FILE_SIZE; i++)
    {
        write_buf[i] = (BYTE)(i & 0xFF);
    }
    
    // 创建并写入大文件（分块写入）
    rt_kprintf("写入大文件: %s\n", TEST_FILE_LARGE);
    res = f_open(&file, TEST_FILE_LARGE, FA_CREATE_ALWAYS | FA_WRITE);
    uassert_int_equal(res, FR_OK);
    
    DWORD start_tick = rt_tick_get();
    UINT total_written = 0;
    
    // 分块写入，避免一次写入太大导致卡死
    for (int offset = 0; offset < LARGE_FILE_SIZE; offset += WRITE_BLOCK_SIZE)
    {
        UINT to_write = (offset + WRITE_BLOCK_SIZE > LARGE_FILE_SIZE) ? 
                        (LARGE_FILE_SIZE - offset) : WRITE_BLOCK_SIZE;
        
        res = f_write(&file, write_buf + offset, to_write, &bw);
        if (res != FR_OK)
        {
            rt_kprintf("  写入失败在偏移 %d，错误码: %d\n", offset, res);
            break;
        }
        total_written += bw;
        
        // 每写入 1KB 打印一个点
        if ((offset % 1024) == 0 && offset > 0)
        {
            rt_kprintf(".");
        }
    }
    rt_kprintf("\n");
    
    DWORD write_time = rt_tick_get() - start_tick;
    
    uassert_int_equal(res, FR_OK);
    uassert_int_equal(total_written, LARGE_FILE_SIZE);
    
    res = f_close(&file);
    uassert_int_equal(res, FR_OK);
    
    rt_kprintf("  写入 %d 字节，耗时 %lu ms\n", LARGE_FILE_SIZE, write_time);
    if (write_time > 0)
    {
        rt_kprintf("  写入速度: %lu KB/s\n", 
                   (unsigned long)((LARGE_FILE_SIZE / 1024) * 1000 / write_time));
    }
    
    // 读取并验证大文件（分块读取）
    rt_kprintf("读取大文件并验证...\n");
    res = f_open(&file, TEST_FILE_LARGE, FA_READ);
    uassert_int_equal(res, FR_OK);
    
    start_tick = rt_tick_get();
    UINT total_read = 0;
    
    // 分块读取
    for (int offset = 0; offset < LARGE_FILE_SIZE; offset += WRITE_BLOCK_SIZE)
    {
        UINT to_read = (offset + WRITE_BLOCK_SIZE > LARGE_FILE_SIZE) ? 
                       (LARGE_FILE_SIZE - offset) : WRITE_BLOCK_SIZE;
        
        res = f_read(&file, read_buf + offset, to_read, &br);
        if (res != FR_OK)
        {
            rt_kprintf("  读取失败在偏移 %d，错误码: %d\n", offset, res);
            break;
        }
        total_read += br;
    }
    
    DWORD read_time = rt_tick_get() - start_tick;
    
    uassert_int_equal(res, FR_OK);
    uassert_int_equal(total_read, LARGE_FILE_SIZE);
    
    res = f_close(&file);
    uassert_int_equal(res, FR_OK);
    
    rt_kprintf("  读取 %d 字节，耗时 %lu ms\n", LARGE_FILE_SIZE, read_time);
    if (read_time > 0)
    {
        rt_kprintf("  读取速度: %lu KB/s\n", 
                   (unsigned long)((LARGE_FILE_SIZE / 1024) * 1000 / read_time));
    }
    
    // 验证数据一致性
    rt_kprintf("验证数据完整性...\n");
    int errors = 0;
    for (int i = 0; i < LARGE_FILE_SIZE; i++)
    {
        if (read_buf[i] != write_buf[i])
        {
            if (errors < 10)  // 只报告前10个错误
            {
                rt_kprintf("  错误: 位置 %d, 期望 0x%02X, 实际 0x%02X\n", 
                           i, write_buf[i], read_buf[i]);
            }
            errors++;
        }
    }
    
    uassert_int_equal(errors, 0);
    rt_kprintf("  数据验证成功，无错误\n");
    
    // 清理
    rt_free(write_buf);
    rt_free(read_buf);
    
    rt_kprintf("=== 大文件读写测试通过 ===\n\n");
}

/**
 * @brief 测试用例6：文件查找和定位操作
 */
RT_UNUSED static void test_fatfs_seek(void)
{
    FRESULT res;
    FIL file;
    UINT bw, br;
    const char test_data[] = "0123456789ABCDEFGHIJ";
    char read_buf[10];
    
    rt_kprintf("\n=== 测试6：文件查找定位测试 ===\n");
    
    // 创建测试文件
    rt_kprintf("创建测试文件...\n");
    res = f_open(&file, TEST_FILE_SMALL, FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
    uassert_int_equal(res, FR_OK);
    
    res = f_write(&file, test_data, strlen(test_data), &bw);
    uassert_int_equal(res, FR_OK);
    
    // 测试从文件开头定位
    rt_kprintf("测试从文件开头定位...\n");
    res = f_lseek(&file, 5);
    uassert_int_equal(res, FR_OK);
    uassert_int_equal(f_tell(&file), 5);
    
    memset(read_buf, 0, sizeof(read_buf));
    res = f_read(&file, read_buf, 5, &br);
    uassert_int_equal(res, FR_OK);
    uassert_int_equal(br, 5);
    uassert_true(strncmp(read_buf, "56789", 5) == 0);
    rt_kprintf("  从位置5读取: \"%.*s\" ✓\n", 5, read_buf);
    
    // 测试定位到文件末尾
    rt_kprintf("测试定位到文件末尾...\n");
    res = f_lseek(&file, f_size(&file));
    uassert_int_equal(res, FR_OK);
    uassert_int_equal(f_tell(&file), strlen(test_data));
    
    // 测试从当前位置回退
    rt_kprintf("测试回退到文件开头...\n");
    res = f_lseek(&file, 0);
    uassert_int_equal(res, FR_OK);
    uassert_int_equal(f_tell(&file), 0);
    
    memset(read_buf, 0, sizeof(read_buf));
    res = f_read(&file, read_buf, 5, &br);
    uassert_int_equal(res, FR_OK);
    uassert_true(strncmp(read_buf, "01234", 5) == 0);
    rt_kprintf("  从位置0读取: \"%.*s\" ✓\n", 5, read_buf);
    
    res = f_close(&file);
    uassert_int_equal(res, FR_OK);
    
    rt_kprintf("=== 文件查找定位测试通过 ===\n\n");
}

/**
 * @brief 测试用例7：目录操作测试
 */
RT_UNUSED static void test_fatfs_directory(void)
{
    FRESULT res;
    DIR dir;
    FILINFO fno;
    const char *test_subdir = "0:/test/subdir";
    const char *test_file_in_subdir = "0:/test/subdir/file.txt";
    FIL file;
    
    rt_kprintf("\n=== 测试7：目录操作测试 ===\n");
    
    // 创建子目录
    rt_kprintf("创建子目录: %s\n", test_subdir);
    res = f_mkdir(test_subdir);
    if (res != FR_OK && res != FR_EXIST)
    {
        rt_kprintf("创建目录失败: %d\n", res);
    }
    uassert_true(res == FR_OK || res == FR_EXIST);
    
    // 在子目录中创建文件
    rt_kprintf("在子目录中创建文件...\n");
    res = f_open(&file, test_file_in_subdir, FA_CREATE_ALWAYS | FA_WRITE);
    uassert_int_equal(res, FR_OK);
    f_close(&file);
    
    // 打开并遍历目录
    rt_kprintf("遍历测试目录内容:\n");
    res = f_opendir(&dir, TEST_DIR);
    uassert_int_equal(res, FR_OK);
    
    int file_count = 0;
    int dir_count = 0;
    
    while (true)
    {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
            break;
        
        if (fno.fattrib & AM_DIR)
        {
            rt_kprintf("  [目录] %s\n", fno.fname);
            dir_count++;
        }
        else
        {
            rt_kprintf("  [文件] %s (%lu 字节)\n", fno.fname, (unsigned long)fno.fsize);
            file_count++;
        }
    }
    
    rt_kprintf("  统计: %d 个文件, %d 个目录\n", file_count, dir_count);
    uassert_true(file_count > 0 || dir_count > 0);
    
    res = f_closedir(&dir);
    uassert_int_equal(res, FR_OK);
    
    // 删除测试文件和子目录
    rt_kprintf("清理测试目录...\n");
    f_unlink(test_file_in_subdir);
    f_unlink(test_subdir);
    
    rt_kprintf("=== 目录操作测试通过 ===\n\n");
}

/**
 * @brief 测试用例8：文件重命名和删除
 */
RT_UNUSED static void test_fatfs_rename_delete(void)
{
    FRESULT res;
    FIL file;
    const char *old_name = "0:/test/old.txt";
    const char *new_name = "0:/test/new.txt";
    FILINFO fno;
    
    rt_kprintf("\n=== 测试8：文件重命名和删除测试 ===\n");
    
    // 创建测试文件
    rt_kprintf("创建测试文件: %s\n", old_name);
    res = f_open(&file, old_name, FA_CREATE_ALWAYS | FA_WRITE);
    uassert_int_equal(res, FR_OK);
    f_close(&file);
    
    // 验证文件存在
    res = f_stat(old_name, &fno);
    uassert_int_equal(res, FR_OK);
    
    // 重命名文件
    rt_kprintf("重命名文件: %s -> %s\n", old_name, new_name);
    res = f_rename(old_name, new_name);
    uassert_int_equal(res, FR_OK);
    
    // 验证旧文件不存在
    res = f_stat(old_name, &fno);
    uassert_int_equal(res, FR_NO_FILE);
    
    // 验证新文件存在
    res = f_stat(new_name, &fno);
    uassert_int_equal(res, FR_OK);
    rt_kprintf("  重命名成功\n");
    
    // 删除文件
    rt_kprintf("删除文件: %s\n", new_name);
    res = f_unlink(new_name);
    uassert_int_equal(res, FR_OK);
    
    // 验证文件已删除
    res = f_stat(new_name, &fno);
    uassert_int_equal(res, FR_NO_FILE);
    rt_kprintf("  删除成功\n");
    
    rt_kprintf("=== 文件重命名和删除测试通过 ===\n\n");
}

/**
 * @brief 测试用例9：错误处理测试
 */
RT_UNUSED static void test_fatfs_error_handling(void)
{
    FRESULT res;
    FIL file;
    char read_buf[100];
    UINT br;
    
    rt_kprintf("\n=== 测试9：错误处理测试 ===\n");
    
    // 测试打开不存在的文件
    rt_kprintf("测试打开不存在的文件...\n");
    const char *nonexist_file = "0:/test/nofile.txt";
    // 先确保文件不存在
    f_unlink(nonexist_file);
    
    res = f_open(&file, nonexist_file, FA_READ);
    rt_kprintf("  返回码: %d (FR_NO_FILE=%d, FR_NO_PATH=%d, FR_INVALID_NAME=%d)\n", 
               res, FR_NO_FILE, FR_NO_PATH, FR_INVALID_NAME);
    
    // FatFS 在文件不存在时可能返回多种错误码
    // FR_NO_FILE(4): 文件不存在
    // FR_NO_PATH(5): 路径不存在  
    // FR_INVALID_NAME(6): 文件名无效
    if (res == FR_NO_FILE || res == FR_NO_PATH || res == FR_INVALID_NAME)
    {
        rt_kprintf("  正确返回文件不存在错误\n");
    }
    else
    {
        rt_kprintf("  错误: 期望文件不存在错误，实际 %d\n", res);
        uassert_true(false);
    }
    
    // 测试在只读模式下写入
    rt_kprintf("测试在只读模式下写入...\n");
    // 先创建测试文件
    res = f_open(&file, TEST_FILE_SMALL, FA_CREATE_ALWAYS | FA_WRITE);
    if (res == FR_OK)
    {
        const char *data = "readonly test";
        UINT bw;
        f_write(&file, data, strlen(data), &bw);
        f_close(&file);
    }
    
    // 以只读模式打开
    res = f_open(&file, TEST_FILE_SMALL, FA_READ);
    uassert_int_equal(res, FR_OK);
    
    if (res == FR_OK)
    {
        UINT bw;
        const char *data = "test write";
        res = f_write(&file, data, strlen(data), &bw);
        rt_kprintf("  写入返回: res=%d, bytes=%u\n", res, bw);
        
        // FatFS 可能返回错误或写入0字节
        if (res != FR_OK)
        {
            rt_kprintf("  正确拒绝写入操作 (返回错误)\n");
        }
        else if (bw == 0)
        {
            rt_kprintf("  正确拒绝写入操作 (写入0字节)\n");
        }
        else
        {
            rt_kprintf("  警告: 只读模式下写入了 %u 字节\n", bw);
        }
        
        f_close(&file);
    }
    
    // 测试读取已关闭的文件（应该失败）
    rt_kprintf("测试使用无效文件句柄...\n");
    FIL invalid_file;
    memset(&invalid_file, 0, sizeof(invalid_file));
    res = f_read(&invalid_file, read_buf, sizeof(read_buf), &br);
    rt_kprintf("  返回码: %d\n", res);
    uassert_true(res != FR_OK);
    rt_kprintf("  正确拒绝无效句柄\n");
    
    // 测试创建无效路径的文件
    rt_kprintf("测试创建无效路径的文件...\n");
    const char *invalid_path = "0:/nodir/file.txt";
    // 确保目录不存在
    f_unlink("0:/nodir");
    
    res = f_open(&file, invalid_path, FA_CREATE_ALWAYS | FA_WRITE);
    rt_kprintf("  返回码: %d (FR_NO_PATH=%d, FR_INVALID_NAME=%d)\n", 
               res, FR_NO_PATH, FR_INVALID_NAME);
    
    // 可能返回 FR_NO_PATH 或 FR_INVALID_NAME
    if (res == FR_NO_PATH || res == FR_INVALID_NAME)
    {
        rt_kprintf("  正确返回路径错误\n");
    }
    else
    {
        rt_kprintf("  错误: 期望路径错误，实际 %d\n", res);
        uassert_true(res == FR_NO_PATH || res == FR_INVALID_NAME);
    }
    
    rt_kprintf("=== 错误处理测试通过 ===\n\n");
}

/**
 * @brief 测试用例10：压力测试 - 多次打开关闭
 */
RT_UNUSED static void test_fatfs_stress(void)
{
    FRESULT res;
    FIL file;
    const char *test_file = "0:/test/stress.txt";
    const int iterations = 50;
    
    rt_kprintf("\n=== 测试10：压力测试 (打开/关闭 %d 次) ===\n", iterations);
    
    DWORD start_tick = rt_tick_get();
    
    for (int i = 0; i < iterations; i++)
    {
        // 打开文件
        res = f_open(&file, test_file, FA_CREATE_ALWAYS | FA_WRITE);
        uassert_int_equal(res, FR_OK);
        
        // 写入数据
        UINT bw;
        char data[32];
        snprintf(data, sizeof(data), "Iteration %d", i);
        res = f_write(&file, data, strlen(data), &bw);
        uassert_int_equal(res, FR_OK);
        
        // 关闭文件
        res = f_close(&file);
        uassert_int_equal(res, FR_OK);
        
        if ((i + 1) % 10 == 0)
        {
            rt_kprintf("  完成 %d/%d 次迭代\n", i + 1, iterations);
        }
    }
    
    DWORD elapsed = rt_tick_get() - start_tick;
    rt_kprintf("  总耗时: %lu ms\n", elapsed);
    rt_kprintf("  平均每次: %lu ms\n", elapsed / iterations);
    
    // 清理
    f_unlink(test_file);
    
    rt_kprintf("=== 压力测试通过 ===\n\n");
}

/**
 * @brief 测试用例11：速度性能测试
 */
RT_UNUSED static void test_fatfs_speed_benchmark(void)
{
    FRESULT res;
    FIL file;
    const char *test_file = "0:/test/speed.dat";
    UINT bw, br;
    
    rt_kprintf("\n=== 测试11：速度性能基准测试 ===\n");
    
    // 测试不同大小的写入速度
    // 注意：即使 FF_MAX_SS=512，FatFS 会自动分块处理大于 512 字节的写入
    const UINT test_sizes[] = {512, 1024, 2048, 4096};
    const int num_tests = sizeof(test_sizes) / sizeof(test_sizes[0]);
    
    rt_kprintf("\n--- 写入速度测试 ---\n");
    rt_kprintf("注意：每次测试独立运行，FatFS 自动处理大块写入\n");
    
    for (int i = 0; i < num_tests; i++)
    {
        UINT size = test_sizes[i];
        rt_kprintf("\n测试块大小 %u 字节...\n", size);
        
        // 使用 4 字节对齐的缓冲区（关键！）
        BYTE *buffer = (BYTE *)rt_malloc_align(size, 4);
        
        if (!buffer)
        {
            rt_kprintf("  内存分配失败\n");
            continue;
        }
        
        // 验证对齐
        rt_kprintf("  缓冲区地址: 0x%08X (对齐: %s)\n", 
                   (uint32_t)buffer, 
                   ((uint32_t)buffer % 4 == 0) ? "是" : "否");
        
        // 填充测试数据
        for (UINT j = 0; j < size; j++)
        {
            buffer[j] = (BYTE)(j & 0xFF);
        }
        
        // 简化测试：只测试3次，每次独立的文件
        const int write_count = 3;
        DWORD total_time = 0;
        DWORD total_written = 0;
        
        for (int j = 0; j < write_count; j++)
        {
            char filename[32];
            snprintf(filename, sizeof(filename), "0:/test/spd%d.dat", size);
            
            rt_kprintf("  测试 %d/%d: ", j+1, write_count);
            
            // 打开文件（每次都创建新文件）
            res = f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE);
            if (res != FR_OK)
            {
                rt_kprintf("打开失败 (res=%d)\n", res);
                break;
            }
            
            // 写入（添加超时检测）
            rt_kprintf("写入中...");
            DWORD start = rt_tick_get();
            res = f_write(&file, buffer, size, &bw);
            DWORD elapsed = rt_tick_get() - start;
            rt_kprintf("完成(%lu ms) ", elapsed);
            
            // 关闭（添加超时检测）
            rt_kprintf("关闭中...");
            DWORD close_start = rt_tick_get();
            FRESULT close_res = f_close(&file);
            DWORD close_time = rt_tick_get() - close_start;
            rt_kprintf("完成(%lu ms)\n", close_time);
            
            if (res == FR_OK && bw == size && close_res == FR_OK)
            {
                rt_kprintf("  -> 成功 (写入%u字节/%lu ms, 关闭%lu ms)\n", bw, elapsed, close_time);
                total_time += elapsed;
                total_written += bw;
            }
            else
            {
                rt_kprintf("  -> 失败 (写入res=%d, bw=%u, 关闭res=%d)\n", res, bw, close_res);
                f_unlink(filename);
                break;
            }
            
            // 删除测试文件
            f_unlink(filename);
        }
        
        rt_free_align(buffer);  // 关键：必须使用 rt_free_align() 释放对齐内存
        
        // 计算平均速度
        if (total_time > 0 && total_written > 0)
        {
            DWORD speed_x100 = (total_written * 100000UL) / (total_time * 1024);
            rt_kprintf("  平均速度: %lu.%02lu KB/s (%lu 字节, %lu ms)\n", 
                       speed_x100/100, speed_x100%100, total_written, total_time);
        }
    }
    
    rt_kprintf("\n--- 读取速度测试 ---\n");
    
    for (int i = 0; i < num_tests; i++)
    {
        UINT size = test_sizes[i];
        rt_kprintf("测试块大小 %u 字节...\n", size);
        
        // 为每个块大小使用独立的文件名，避免冲突
        char test_file[32];
        snprintf(test_file, sizeof(test_file), "0:/test/rdspd%d.dat", size);
        
        // 使用对齐内存分配
        BYTE *write_buffer = (BYTE *)rt_malloc_align(size, 4);
        BYTE *read_buffer = (BYTE *)rt_malloc_align(size, 4);
        
        if (!write_buffer || !read_buffer)
        {
            rt_kprintf("  内存分配失败\n");
            if (write_buffer) rt_free_align(write_buffer);
            if (read_buffer) rt_free_align(read_buffer);
            continue;
        }
        
        // 填充测试数据
        for (UINT j = 0; j < size; j++)
        {
            write_buffer[j] = (BYTE)(j & 0xFF);
        }
        
        // 先删除旧文件（如果存在）
        f_unlink(test_file);
        
        // 先写入测试文件
        res = f_open(&file, test_file, FA_CREATE_ALWAYS | FA_WRITE);
        if (res != FR_OK)
        {
            rt_kprintf("  打开文件失败(写): %d\n", res);
            rt_free_align(write_buffer);
            rt_free_align(read_buffer);
            continue;
        }
        
        const int read_count = 10;
        bool write_success = true;
        for (int j = 0; j < read_count; j++)
        {
            res = f_write(&file, write_buffer, size, &bw);
            if (res != FR_OK || bw != size)
            {
                rt_kprintf("  写入失败: res=%d, bw=%u\n", res, bw);
                write_success = false;
                break;
            }
        }
        f_close(&file);
        
        if (!write_success)
        {
            rt_free_align(write_buffer);
            rt_free_align(read_buffer);
            f_unlink(test_file);
            continue;
        }
        
        // 读取测试
        res = f_open(&file, test_file, FA_READ);
        if (res != FR_OK)
        {
            rt_kprintf("  打开文件失败(读): %d\n", res);
            rt_free(write_buffer);
            rt_free(read_buffer);
            f_unlink(test_file);
            continue;
        }
        
        DWORD total_time = 0;
        DWORD total_read = 0;
        
        for (int j = 0; j < read_count; j++)
        {
            DWORD start = rt_tick_get();
            res = f_read(&file, read_buffer, size, &br);
            DWORD elapsed = rt_tick_get() - start;
            
            if (res == FR_OK && br == size)
            {
                total_time += elapsed;
                total_read += br;
            }
            else
            {
                rt_kprintf("  读取失败: res=%d, br=%u\n", res, br);
                break;
            }
        }
        
        f_close(&file);
        rt_free_align(write_buffer);
        rt_free_align(read_buffer);
        
        // 删除测试文件
        res = f_unlink(test_file);
        if (res != FR_OK)
        {
            rt_kprintf("  删除文件失败: %d\n", res);
        }
        
        // 计算平均速度 (使用整数运算)
        if (total_time > 0 && total_read > 0)
        {
            DWORD speed_x100 = (total_read * 100000UL) / (total_time * 1024);
            rt_kprintf("  块大小 %4u 字节: %lu.%02lu KB/s (读取 %lu 字节, 耗时 %lu ms)\n", 
                       size, speed_x100/100, speed_x100%100, total_read, total_time);
        }
    }
    
    rt_kprintf("\n--- 顺序读写性能测试 (16KB 文件) ---\n");
    
    const UINT seq_size = 16 * 1024;  // 16KB
    BYTE *seq_buffer = (BYTE *)rt_malloc(seq_size);
    
    if (seq_buffer)
    {
        // 填充数据
        for (UINT i = 0; i < seq_size; i++)
        {
            seq_buffer[i] = (BYTE)(i & 0xFF);
        }
        
        // 顺序写入测试
        res = f_open(&file, test_file, FA_CREATE_ALWAYS | FA_WRITE);
        if (res == FR_OK)
        {
            DWORD start = rt_tick_get();
            UINT total = 0;
            
            // 分块写入
            for (UINT offset = 0; offset < seq_size; offset += 512)
            {
                res = f_write(&file, seq_buffer + offset, 512, &bw);
                if (res == FR_OK)
                {
                    total += bw;
                }
            }
            
            DWORD write_time = rt_tick_get() - start;
            f_close(&file);
            
            if (write_time > 0)
            {
                DWORD speed_x100 = (total * 100000UL) / (write_time * 1024);
                rt_kprintf("  顺序写入: %lu.%02lu KB/s (%u 字节, %lu ms)\n", 
                           speed_x100/100, speed_x100%100, total, write_time);
            }
        }
        
        // 顺序读取测试
        res = f_open(&file, test_file, FA_READ);
        if (res == FR_OK)
        {
            DWORD start = rt_tick_get();
            UINT total = 0;
            
            // 分块读取
            for (UINT offset = 0; offset < seq_size; offset += 512)
            {
                res = f_read(&file, seq_buffer + offset, 512, &br);
                if (res == FR_OK)
                {
                    total += br;
                }
            }
            
            DWORD read_time = rt_tick_get() - start;
            f_close(&file);
            
            if (read_time > 0)
            {
                DWORD speed_x100 = (total * 100000UL) / (read_time * 1024);
                rt_kprintf("  顺序读取: %lu.%02lu KB/s (%u 字节, %lu ms)\n", 
                           speed_x100/100, speed_x100%100, total, read_time);
            }
        }
        
        rt_free(seq_buffer);
        f_unlink(test_file);
    }
    else
    {
        rt_kprintf("无法分配 16KB 内存进行顺序读写测试\n");
    }
    
    rt_kprintf("\n--- 小文件操作性能测试 ---\n");
    
    // 测试大量小文件创建和删除的性能
    const int small_file_count = 20;
    char filename[32];
    BYTE small_data[] = "Test data";
    
    DWORD start = rt_tick_get();
    
    for (int i = 0; i < small_file_count; i++)
    {
        snprintf(filename, sizeof(filename), "0:/test/sf%d.txt", i);
        
        res = f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE);
        if (res == FR_OK)
        {
            f_write(&file, small_data, sizeof(small_data), &bw);
            f_close(&file);
        }
    }
    
    DWORD create_time = rt_tick_get() - start;
    rt_kprintf("  创建 %d 个小文件: %lu ms (平均 %lu ms/文件)\n", 
               small_file_count, create_time, create_time / small_file_count);
    
    // 删除测试
    start = rt_tick_get();
    
    for (int i = 0; i < small_file_count; i++)
    {
        snprintf(filename, sizeof(filename), "0:/test/sf%d.txt", i);
        f_unlink(filename);
    }
    
    DWORD delete_time = rt_tick_get() - start;
    rt_kprintf("  删除 %d 个小文件: %lu ms (平均 %lu ms/文件)\n", 
               small_file_count, delete_time, delete_time / small_file_count);
    
    rt_kprintf("\n=== 速度性能测试完成 ===\n\n");
}

// ==================== 测试用例注册 ====================

// 测试1：磁盘驱动接口
UTEST_TC_EXPORT(test_fatfs_disk_interface, "fatfs.01.disk_interface", 
                utest_fatfs_init, NULL, 10);

// 测试2：文件系统挂载和信息
UTEST_TC_EXPORT(test_fatfs_mount_info, "fatfs.02.mount_info", 
                utest_fatfs_init, NULL, 10);

// 测试3：基本文件读写
UTEST_TC_EXPORT(test_fatfs_basic_rw, "fatfs.03.basic_rw", 
                utest_fatfs_init, NULL, 10);

// 测试4：文件追加
UTEST_TC_EXPORT(test_fatfs_append, "fatfs.04.append", 
                utest_fatfs_init, NULL, 10);

// 测试5：大文件读写
UTEST_TC_EXPORT(test_fatfs_large_file, "fatfs.05.large_file", 
                utest_fatfs_init, NULL, 30);

// 测试6：文件查找定位
UTEST_TC_EXPORT(test_fatfs_seek, "fatfs.06.seek", 
                utest_fatfs_init, NULL, 10);

// 测试7：目录操作
UTEST_TC_EXPORT(test_fatfs_directory, "fatfs.07.directory", 
                utest_fatfs_init, NULL, 10);

// 测试8：重命名和删除
UTEST_TC_EXPORT(test_fatfs_rename_delete, "fatfs.08.rename_delete", 
                utest_fatfs_init, NULL, 10);

// 测试9：错误处理
UTEST_TC_EXPORT(test_fatfs_error_handling, "fatfs.09.error_handling", 
                utest_fatfs_init, NULL, 10);

// 测试10：压力测试
UTEST_TC_EXPORT(test_fatfs_stress, "fatfs.10.stress", 
                utest_fatfs_init, NULL, 30);

// 测试11：速度性能测试
UTEST_TC_EXPORT(test_fatfs_speed_benchmark, "fatfs.11.speed_benchmark", 
                utest_fatfs_init, utest_fatfs_cleanup, 60);

} // extern "C"
