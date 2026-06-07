#include "ff.h"
#include "rtthread.h"

#include <finsh.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FATFS_SD_BENCH_DRIVE              "0:"
#define FATFS_SD_BENCH_BIN_PATH           "0:/YT_BENCH.BIN"
#define FATFS_SD_BENCH_GPX_PATH           "0:/YT_BENCH.GPX"

#define FATFS_SD_BENCH_DEFAULT_FILE_SIZE  (1024UL * 1024UL)
#define FATFS_SD_BENCH_MIN_FILE_SIZE      (64UL * 1024UL)
#define FATFS_SD_BENCH_MAX_FILE_SIZE      (16UL * 1024UL * 1024UL)
#define FATFS_SD_BENCH_TILE_READ_TOTAL    (2UL * 1024UL * 1024UL)
#define FATFS_SD_BENCH_GPX_RECORDS        4096UL
#define FATFS_SD_BENCH_GPX_SYNC_EVERY     128UL

#define FATFS_SD_BENCH_SEQ_READ_MIN_KBPS  500UL
#define FATFS_SD_BENCH_TILE_READ_MIN_KBPS 200UL
#define FATFS_SD_BENCH_SEQ_WRITE_MIN_KBPS 200UL
#define FATFS_SD_BENCH_GPX_WRITE_MIN_KBPS 20UL

#define FATFS_SD_BENCH_STATIC_BUFFER_SIZE (16UL * 1024UL)
#define FATFS_SD_BENCH_MKFS_WORK_SIZE     4096U

static const char fatfs_sd_bench_gpx_header[] = "<?xml version=\"1.0\"?><gpx><trk><trkseg>\n";
static const char fatfs_sd_bench_gpx_footer[] = "</trkseg></trk></gpx>\n";

static FATFS fatfs_sd_bench_fs;
static uint8_t fatfs_sd_bench_static_buffer[FATFS_SD_BENCH_STATIC_BUFFER_SIZE];
static uint8_t fatfs_sd_bench_verify_buffer[FATFS_SD_BENCH_STATIC_BUFFER_SIZE];
static uint8_t fatfs_sd_bench_mkfs_work[FATFS_SD_BENCH_MKFS_WORK_SIZE];

static uint32_t fatfs_sd_bench_parse_size(const char *text, uint32_t default_size)
{
    if (text == RT_NULL) {
        return default_size;
    }

    char *end           = RT_NULL;
    unsigned long value = strtoul(text, &end, 0);
    if (value == 0UL) {
        return default_size;
    }

    if (end != RT_NULL) {
        if (*end == 'k' || *end == 'K') {
            value *= 1024UL;
        } else if (*end == 'm' || *end == 'M') {
            value *= 1024UL * 1024UL;
        }
    }

    if (value < FATFS_SD_BENCH_MIN_FILE_SIZE) {
        value = FATFS_SD_BENCH_MIN_FILE_SIZE;
    }
    if (value > FATFS_SD_BENCH_MAX_FILE_SIZE) {
        value = FATFS_SD_BENCH_MAX_FILE_SIZE;
    }

    return (uint32_t)value;
}

static void fatfs_sd_bench_fill_pattern(uint8_t *buffer, uint32_t length, uint32_t offset_seed)
{
    for (uint32_t i = 0; i < length; i++) {
        buffer[i] = (uint8_t)((offset_seed + i * 31UL + (i >> 3)) & 0xffU);
    }
}

static int fatfs_sd_bench_verify_pattern(const uint8_t *buffer, uint32_t length, uint32_t offset_seed)
{
    for (uint32_t i = 0; i < length; i++) {
        uint8_t expected = (uint8_t)((offset_seed + i * 31UL + (i >> 3)) & 0xffU);
        if (buffer[i] != expected) {
            rt_kprintf("fatfs_sd_bench: verify mismatch at +%lu expected=0x%02x actual=0x%02x\n",
                       (unsigned long)i,
                       expected,
                       buffer[i]);
            return -1;
        }
    }
    return 0;
}

static uint32_t fatfs_sd_bench_elapsed_ms(rt_tick_t start, rt_tick_t end)
{
    rt_tick_t elapsed   = end - start;
    uint64_t elapsed_ms = ((uint64_t)elapsed * 1000ULL) / RT_TICK_PER_SECOND;
    if (elapsed_ms == 0ULL) {
        elapsed_ms = 1ULL;
    }
    return (uint32_t)elapsed_ms;
}

static uint32_t fatfs_sd_bench_kbps(uint32_t bytes, uint32_t elapsed_ms)
{
    if (elapsed_ms == 0U) {
        elapsed_ms = 1U;
    }
    return (uint32_t)(((uint64_t)bytes * 1000ULL) / ((uint64_t)elapsed_ms * 1024ULL));
}

static uint32_t fatfs_sd_bench_print_result(const char *name, uint32_t bytes, uint32_t elapsed_ms, uint32_t chunk_size, uint32_t threshold_kbps)
{
    uint32_t kbps = fatfs_sd_bench_kbps(bytes, elapsed_ms);
    rt_kprintf("fatfs_sd_bench: result %-12s bytes=%lu elapsed_ms=%lu chunk=%lu throughput=%lu KB/s threshold=%lu %s\n",
               name,
               (unsigned long)bytes,
               (unsigned long)elapsed_ms,
               (unsigned long)chunk_size,
               (unsigned long)kbps,
               (unsigned long)threshold_kbps,
               kbps >= threshold_kbps ? "PASS" : "WARN");
    return kbps;
}

static FRESULT fatfs_sd_bench_mount_or_format(int allow_format)
{
    FRESULT res = f_mount(&fatfs_sd_bench_fs, FATFS_SD_BENCH_DRIVE, 1);
    rt_kprintf("fatfs_sd_bench: mount res=%d\n", res);
    if (res == FR_OK) {
        return res;
    }

    if (!allow_format) {
        rt_kprintf("fatfs_sd_bench: mount failed; rerun with --format to format SD card (destructive)\n");
        return res;
    }

    MKFS_PARM opt = {
        .fmt     = FM_FAT | FM_FAT32 | FM_SFD,
        .n_fat   = 0,
        .align   = 0,
        .n_root  = 0,
        .au_size = 0,
    };

    rt_kprintf("fatfs_sd_bench: mount failed, --format supplied, formatting SD card (destructive)\n");
    res = f_mkfs(FATFS_SD_BENCH_DRIVE, &opt, fatfs_sd_bench_mkfs_work, sizeof(fatfs_sd_bench_mkfs_work));
    rt_kprintf("fatfs_sd_bench: mkfs res=%d\n", res);
    if (res != FR_OK) {
        return res;
    }

    f_mount(RT_NULL, FATFS_SD_BENCH_DRIVE, 0);
    res = f_mount(&fatfs_sd_bench_fs, FATFS_SD_BENCH_DRIVE, 1);
    rt_kprintf("fatfs_sd_bench: remount res=%d\n", res);
    return res;
}

static FRESULT fatfs_sd_bench_write_file(const char *path, uint32_t file_size, uint32_t chunk_size, uint32_t *elapsed_ms)
{
    FIL file;
    FRESULT res = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        return res;
    }

    uint32_t written_total = 0U;
    rt_tick_t start        = rt_tick_get();
    while (written_total < file_size) {
        uint32_t todo = file_size - written_total;
        if (todo > chunk_size) {
            todo = chunk_size;
        }

        fatfs_sd_bench_fill_pattern(fatfs_sd_bench_static_buffer, todo, written_total);

        UINT written = 0U;
        res          = f_write(&file, fatfs_sd_bench_static_buffer, todo, &written);
        if (res != FR_OK || written != todo) {
            rt_kprintf("fatfs_sd_bench: write failed res=%d written=%u expected=%lu\n", res, written, (unsigned long)todo);
            if (res == FR_OK) {
                res = FR_INT_ERR;
            }
            break;
        }
        written_total += written;
    }

    if (res == FR_OK) {
        res = f_sync(&file);
    }
    rt_tick_t end = rt_tick_get();
    *elapsed_ms   = fatfs_sd_bench_elapsed_ms(start, end);

    FRESULT close_res = f_close(&file);
    if (res == FR_OK) {
        res = close_res;
    }
    return res;
}

static FRESULT fatfs_sd_bench_verify_file(const char *path, uint32_t file_size, uint32_t chunk_size)
{
    FIL file;
    FRESULT res = f_open(&file, path, FA_READ);
    if (res != FR_OK) {
        return res;
    }

    uint32_t read_total = 0U;
    while (read_total < file_size) {
        uint32_t todo = file_size - read_total;
        if (todo > chunk_size) {
            todo = chunk_size;
        }

        UINT bytes_read = 0U;
        res             = f_read(&file, fatfs_sd_bench_verify_buffer, todo, &bytes_read);
        if (res != FR_OK || bytes_read != todo) {
            rt_kprintf("fatfs_sd_bench: verify read failed res=%d read=%u expected=%lu\n", res, bytes_read, (unsigned long)todo);
            if (res == FR_OK) {
                res = FR_INT_ERR;
            }
            break;
        }

        if (fatfs_sd_bench_verify_pattern(fatfs_sd_bench_verify_buffer, todo, read_total) != 0) {
            res = FR_INT_ERR;
            break;
        }
        read_total += bytes_read;
    }

    if (read_total != file_size && res == FR_OK) {
        res = FR_INT_ERR;
    }

    FRESULT close_res = f_close(&file);
    if (res == FR_OK) {
        res = close_res;
    }
    return res;
}

static FRESULT fatfs_sd_bench_sequential_read(const char *path, uint32_t file_size, uint32_t chunk_size, uint32_t *elapsed_ms)
{
    FIL file;
    FRESULT res = f_open(&file, path, FA_READ);
    if (res != FR_OK) {
        return res;
    }

    uint32_t read_total = 0U;
    rt_tick_t start     = rt_tick_get();
    while (read_total < file_size) {
        uint32_t todo = file_size - read_total;
        if (todo > chunk_size) {
            todo = chunk_size;
        }

        UINT bytes_read = 0U;
        res             = f_read(&file, fatfs_sd_bench_static_buffer, todo, &bytes_read);
        if (res != FR_OK || bytes_read == 0U) {
            break;
        }
        read_total += bytes_read;
    }
    rt_tick_t end = rt_tick_get();
    *elapsed_ms   = fatfs_sd_bench_elapsed_ms(start, end);

    if (read_total != file_size && res == FR_OK) {
        res = FR_INT_ERR;
    }

    FRESULT close_res = f_close(&file);
    if (res == FR_OK) {
        res = close_res;
    }
    return res;
}

static FRESULT fatfs_sd_bench_tile_read(const char *path, uint32_t file_size, uint32_t chunk_size, uint32_t *bytes_read_out, uint32_t *elapsed_ms)
{
    FIL file;
    FRESULT res = f_open(&file, path, FA_READ);
    if (res != FR_OK) {
        return res;
    }

    uint32_t total = 0U;
    uint32_t reads = FATFS_SD_BENCH_TILE_READ_TOTAL / chunk_size;
    if (reads == 0U) {
        reads = 1U;
    }

    uint32_t max_offset = file_size > chunk_size ? file_size - chunk_size : 0U;
    rt_tick_t start     = rt_tick_get();
    for (uint32_t i = 0U; i < reads; i++) {
        uint32_t offset = 0U;
        if (max_offset > 0U) {
            offset = (uint32_t)(((uint64_t)i * 2654435761ULL) % (uint64_t)(max_offset + 1U));
            offset &= ~(uint32_t)0x1ffU;
            if (offset > max_offset) {
                offset = max_offset;
            }
        }

        res = f_lseek(&file, offset);
        if (res != FR_OK) {
            break;
        }

        UINT bytes_read = 0U;
        res             = f_read(&file, fatfs_sd_bench_static_buffer, chunk_size, &bytes_read);
        if (res != FR_OK || bytes_read != chunk_size) {
            if (res == FR_OK) {
                res = FR_INT_ERR;
            }
            break;
        }
        total += bytes_read;
    }
    rt_tick_t end   = rt_tick_get();
    *elapsed_ms     = fatfs_sd_bench_elapsed_ms(start, end);
    *bytes_read_out = total;

    FRESULT close_res = f_close(&file);
    if (res == FR_OK) {
        res = close_res;
    }
    return res;
}

static FRESULT fatfs_sd_bench_gpx_append(uint32_t *bytes_written_out, uint32_t *elapsed_ms)
{
    FIL file;
    FRESULT res = f_open(&file, FATFS_SD_BENCH_GPX_PATH, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        return res;
    }

    UINT written    = 0U;
    uint32_t total  = 0U;
    rt_tick_t start = rt_tick_get();

    res = f_write(&file, fatfs_sd_bench_gpx_header, sizeof(fatfs_sd_bench_gpx_header) - 1U, &written);
    if (res == FR_OK && written != sizeof(fatfs_sd_bench_gpx_header) - 1U) {
        res = FR_INT_ERR;
    }
    if (res == FR_OK) {
        total += written;
    }

    for (uint32_t i = 0U; i < FATFS_SD_BENCH_GPX_RECORDS && res == FR_OK; i++) {
        char line[96];
        int line_len = rt_snprintf(line,
                                   sizeof(line),
                                   "<trkpt lat=\"%ld.%06ld\" lon=\"%ld.%06ld\"><ele>%lu</ele></trkpt>\n",
                                   31L,
                                   (long)((i * 37UL) % 1000000UL),
                                   121L,
                                   (long)((i * 53UL) % 1000000UL),
                                   (unsigned long)(i % 500UL));
        if (line_len <= 0 || (rt_size_t)line_len >= sizeof(line)) {
            res = FR_INT_ERR;
            break;
        }

        written = 0U;
        res     = f_write(&file, line, (UINT)line_len, &written);
        if (res != FR_OK || written != (UINT)line_len) {
            if (res == FR_OK) {
                res = FR_INT_ERR;
            }
            break;
        }
        total += written;

        if ((i + 1U) % FATFS_SD_BENCH_GPX_SYNC_EVERY == 0U) {
            res = f_sync(&file);
        }
    }

    if (res == FR_OK) {
        written = 0U;
        res     = f_write(&file, fatfs_sd_bench_gpx_footer, sizeof(fatfs_sd_bench_gpx_footer) - 1U, &written);
        if (res == FR_OK && written != sizeof(fatfs_sd_bench_gpx_footer) - 1U) {
            res = FR_INT_ERR;
        }
        if (res == FR_OK) {
            total += written;
        }
    }
    if (res == FR_OK) {
        res = f_sync(&file);
    }

    rt_tick_t end      = rt_tick_get();
    *elapsed_ms        = fatfs_sd_bench_elapsed_ms(start, end);
    *bytes_written_out = total;

    FRESULT close_res = f_close(&file);
    if (res == FR_OK) {
        res = close_res;
    }
    return res;
}

static FRESULT fatfs_sd_bench_verify_gpx(uint32_t expected_size)
{
    FILINFO info;
    FRESULT res = f_stat(FATFS_SD_BENCH_GPX_PATH, &info);
    if (res != FR_OK) {
        return res;
    }
    if (info.fsize != expected_size) {
        rt_kprintf("fatfs_sd_bench: GPX size mismatch expected=%lu actual=%lu\n",
                   (unsigned long)expected_size,
                   (unsigned long)info.fsize);
        return FR_INT_ERR;
    }

    FIL file;
    res = f_open(&file, FATFS_SD_BENCH_GPX_PATH, FA_READ);
    if (res != FR_OK) {
        return res;
    }

    char buffer[sizeof(fatfs_sd_bench_gpx_header)];
    UINT bytes_read = 0U;
    res             = f_read(&file, buffer, sizeof(fatfs_sd_bench_gpx_header) - 1U, &bytes_read);
    if (res == FR_OK && (bytes_read != sizeof(fatfs_sd_bench_gpx_header) - 1U ||
                         memcmp(buffer, fatfs_sd_bench_gpx_header, sizeof(fatfs_sd_bench_gpx_header) - 1U) != 0)) {
        rt_kprintf("fatfs_sd_bench: GPX header verify failed read=%u\n", bytes_read);
        res = FR_INT_ERR;
    }

    if (res == FR_OK) {
        res = f_lseek(&file, expected_size - (sizeof(fatfs_sd_bench_gpx_footer) - 1U));
    }
    if (res == FR_OK) {
        bytes_read = 0U;
        res        = f_read(&file, buffer, sizeof(fatfs_sd_bench_gpx_footer) - 1U, &bytes_read);
        if (res == FR_OK && (bytes_read != sizeof(fatfs_sd_bench_gpx_footer) - 1U ||
                             memcmp(buffer, fatfs_sd_bench_gpx_footer, sizeof(fatfs_sd_bench_gpx_footer) - 1U) != 0)) {
            rt_kprintf("fatfs_sd_bench: GPX footer verify failed read=%u\n", bytes_read);
            res = FR_INT_ERR;
        }
    }

    FRESULT close_res = f_close(&file);
    if (res == FR_OK) {
        res = close_res;
    }
    return res;
}

static void fatfs_sd_bench_print_usage(void)
{
    rt_kprintf("fatfs_sd_bench [--format] [file_size]\n");
    rt_kprintf("  destructive SD-card FatFs smoke + limit benchmark. --format allows formatting if mount fails.\n");
    rt_kprintf("  file_size accepts bytes, K, or M. default=1M max=16M\n");
}

static int fatfs_sd_bench(int argc, char **argv)
{
    int allow_format   = 0;
    uint32_t file_size = FATFS_SD_BENCH_DEFAULT_FILE_SIZE;

    for (int arg = 1; arg < argc; arg++) {
        if (strcmp(argv[arg], "-h") == 0 || strcmp(argv[arg], "--help") == 0) {
            fatfs_sd_bench_print_usage();
            return 0;
        }
        if (strcmp(argv[arg], "-f") == 0 || strcmp(argv[arg], "--format") == 0) {
            allow_format = 1;
            continue;
        }
        file_size = fatfs_sd_bench_parse_size(argv[arg], file_size);
    }

    rt_kprintf("fatfs_sd_bench: start target=SDIO_SD drive=%s file=%s file_size=%lu destructive=yes\n",
               FATFS_SD_BENCH_DRIVE,
               FATFS_SD_BENCH_BIN_PATH,
               (unsigned long)file_size);
    rt_kprintf("fatfs_sd_bench: format_on_mount_failure=%s\n", allow_format ? "yes" : "no");
    rt_kprintf("fatfs_sd_bench: thresholds seq_read>=%luKB/s tile4k>=%luKB/s seq_write>=%luKB/s gpx_append>=%luKB/s\n",
               (unsigned long)FATFS_SD_BENCH_SEQ_READ_MIN_KBPS,
               (unsigned long)FATFS_SD_BENCH_TILE_READ_MIN_KBPS,
               (unsigned long)FATFS_SD_BENCH_SEQ_WRITE_MIN_KBPS,
               (unsigned long)FATFS_SD_BENCH_GPX_WRITE_MIN_KBPS);

    FRESULT res = fatfs_sd_bench_mount_or_format(allow_format);
    if (res != FR_OK) {
        rt_kprintf("fatfs_sd_bench: FAIL phase=mount_or_format res=%d\n", res);
        return -1;
    }

    DWORD free_clusters = 0U;
    FATFS *free_fs      = RT_NULL;
    res                 = f_getfree(FATFS_SD_BENCH_DRIVE, &free_clusters, &free_fs);
    if (res == FR_OK && free_fs != RT_NULL) {
        uint64_t free_bytes = (uint64_t)free_clusters * (uint64_t)free_fs->csize * 512ULL;
        rt_kprintf("fatfs_sd_bench: volume fs_type=%u csize=%u free=%lu KB\n",
                   free_fs->fs_type,
                   free_fs->csize,
                   (unsigned long)(free_bytes / 1024ULL));
    } else {
        rt_kprintf("fatfs_sd_bench: getfree res=%d\n", res);
    }

    const uint32_t chunks[]   = {512U, 4096U, 16U * 1024U};
    uint32_t best_read        = 0U;
    uint32_t best_write       = 0U;
    uint32_t performance_warn = 0U;

    for (rt_size_t i = 0U; i < sizeof(chunks) / sizeof(chunks[0]); i++) {
        uint32_t chunk      = chunks[i];
        uint32_t elapsed_ms = 0U;

        res = fatfs_sd_bench_write_file(FATFS_SD_BENCH_BIN_PATH, file_size, chunk, &elapsed_ms);
        rt_kprintf("fatfs_sd_bench: phase=seq_write chunk=%lu res=%d\n", (unsigned long)chunk, res);
        if (res != FR_OK) {
            rt_kprintf("fatfs_sd_bench: FAIL phase=seq_write res=%d\n", res);
            return -1;
        }
        uint32_t write_kbps = fatfs_sd_bench_print_result("seq_write", file_size, elapsed_ms, chunk, FATFS_SD_BENCH_SEQ_WRITE_MIN_KBPS);
        if (write_kbps < FATFS_SD_BENCH_SEQ_WRITE_MIN_KBPS) {
            performance_warn = 1U;
        }
        if (write_kbps > best_write) {
            best_write = write_kbps;
        }

        res = fatfs_sd_bench_verify_file(FATFS_SD_BENCH_BIN_PATH, file_size, chunk);
        rt_kprintf("fatfs_sd_bench: phase=verify chunk=%lu res=%d\n", (unsigned long)chunk, res);
        if (res != FR_OK) {
            rt_kprintf("fatfs_sd_bench: FAIL phase=verify res=%d\n", res);
            return -1;
        }

        res = fatfs_sd_bench_sequential_read(FATFS_SD_BENCH_BIN_PATH, file_size, chunk, &elapsed_ms);
        rt_kprintf("fatfs_sd_bench: phase=seq_read chunk=%lu res=%d\n", (unsigned long)chunk, res);
        if (res != FR_OK) {
            rt_kprintf("fatfs_sd_bench: FAIL phase=seq_read res=%d\n", res);
            return -1;
        }
        uint32_t read_kbps = fatfs_sd_bench_print_result("seq_read", file_size, elapsed_ms, chunk, FATFS_SD_BENCH_SEQ_READ_MIN_KBPS);
        if (read_kbps < FATFS_SD_BENCH_SEQ_READ_MIN_KBPS) {
            performance_warn = 1U;
        }
        if (read_kbps > best_read) {
            best_read = read_kbps;
        }
    }

    uint32_t tile_bytes      = 0U;
    uint32_t tile_elapsed_ms = 0U;
    res                      = fatfs_sd_bench_tile_read(FATFS_SD_BENCH_BIN_PATH, file_size, 4096U, &tile_bytes, &tile_elapsed_ms);
    rt_kprintf("fatfs_sd_bench: phase=tile_read chunk=4096 res=%d\n", res);
    if (res != FR_OK) {
        rt_kprintf("fatfs_sd_bench: FAIL phase=tile_read res=%d\n", res);
        return -1;
    }
    uint32_t tile_kbps = fatfs_sd_bench_print_result("tile_read", tile_bytes, tile_elapsed_ms, 4096U, FATFS_SD_BENCH_TILE_READ_MIN_KBPS);
    if (tile_kbps < FATFS_SD_BENCH_TILE_READ_MIN_KBPS) {
        performance_warn = 1U;
    }

    rt_kprintf("fatfs_sd_bench: result limit_chunk  bytes=0 elapsed_ms=0 chunk=65536 throughput=0 KB/s threshold=0 SKIP static_buffer=%lu reason=RAM_budget\n",
               (unsigned long)FATFS_SD_BENCH_STATIC_BUFFER_SIZE);

    uint32_t gpx_bytes      = 0U;
    uint32_t gpx_elapsed_ms = 0U;
    res                     = fatfs_sd_bench_gpx_append(&gpx_bytes, &gpx_elapsed_ms);
    rt_kprintf("fatfs_sd_bench: phase=gpx_append records=%lu sync_every=%lu res=%d\n",
               (unsigned long)FATFS_SD_BENCH_GPX_RECORDS,
               (unsigned long)FATFS_SD_BENCH_GPX_SYNC_EVERY,
               res);
    if (res != FR_OK) {
        rt_kprintf("fatfs_sd_bench: FAIL phase=gpx_append res=%d\n", res);
        return -1;
    }
    uint32_t gpx_kbps = fatfs_sd_bench_print_result("gpx_append", gpx_bytes, gpx_elapsed_ms, 0U, FATFS_SD_BENCH_GPX_WRITE_MIN_KBPS);
    if (gpx_kbps < FATFS_SD_BENCH_GPX_WRITE_MIN_KBPS) {
        performance_warn = 1U;
    }

    res = fatfs_sd_bench_verify_gpx(gpx_bytes);
    rt_kprintf("fatfs_sd_bench: phase=gpx_verify expected_size=%lu res=%d\n", (unsigned long)gpx_bytes, res);
    if (res != FR_OK) {
        rt_kprintf("fatfs_sd_bench: FAIL phase=gpx_verify res=%d\n", res);
        return -1;
    }

    rt_kprintf("fatfs_sd_bench: summary best_seq_read=%lu KB/s best_seq_write=%lu KB/s tile4k=%lu KB/s gpx_append=%lu KB/s\n",
               (unsigned long)best_read,
               (unsigned long)best_write,
               (unsigned long)tile_kbps,
               (unsigned long)gpx_kbps);
    rt_kprintf("fatfs_sd_bench: PASS functional smoke complete; performance_warn=%s\n", performance_warn ? "yes" : "no");
    return 0;
}
MSH_CMD_EXPORT(fatfs_sd_bench, destructive SD card FatFs smoke and throughput benchmark);

#ifdef Y_TRACE_FATFS_SD_BENCH_AUTORUN
static void fatfs_sd_bench_autorun_entry(void *parameter)
{
    (void)parameter;

    static char arg0[] = "fatfs_sd_bench";
    static char arg1[] = "--format";
    static char arg2[] = "1M";
    char *argv[]       = {arg0, arg1, arg2};

    rt_thread_mdelay(5000);
    rt_kprintf("fatfs_sd_bench: autorun begin after boot delay\n");
    (void)fatfs_sd_bench((int)(sizeof(argv) / sizeof(argv[0])), argv);
    rt_kprintf("fatfs_sd_bench: autorun end\n");
}

static int fatfs_sd_bench_autorun_init(void)
{
    rt_thread_t thread = rt_thread_create("fsbench", fatfs_sd_bench_autorun_entry, RT_NULL, 4096, RT_THREAD_PRIORITY_MAX - 4, 20);
    if (thread == RT_NULL) {
        rt_kprintf("fatfs_sd_bench: autorun thread create failed\n");
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}
INIT_APP_EXPORT(fatfs_sd_bench_autorun_init);
#endif
