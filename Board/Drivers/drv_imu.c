#include "drv_imu.h"

#include <finsh.h>
#include <rtthread.h>
#include <stdlib.h>

#define LOG_TAG "drv.imu"
#define LOG_LVL LOG_LVL_DBG
#include "ulog.h"

static struct _soft_i2c_bus *imu_bus;
static pin_t imu_int1_pin = -1;
static pin_t imu_int2_pin = -1;
static drv_imu_status_t imu_status;
static uint8_t imu_initialized;

#define DRV_IMU_TEST_SAMPLE_COUNT 5U
#define DRV_IMU_TEST_DELAY_MS     200U
#define DRV_IMU_MONITOR_DELAY_MS  1000U
#define DRV_IMU_MONITOR_MAX_DELAY 60000U

static void drv_imu_clear_status(void)
{
    drv_imu_status_t empty_status = {0};
    imu_status                    = empty_status;
    imu_status.int1_level         = -1;
    imu_status.int2_level         = -1;
}

static void drv_imu_clear_binding(void)
{
    imu_bus         = RT_NULL;
    imu_int1_pin    = -1;
    imu_int2_pin    = -1;
    imu_initialized = 0U;
}

int drv_imu_bind(struct _soft_i2c_bus *bus, pin_t int1_pin, pin_t int2_pin)
{
    drv_imu_clear_binding();
    drv_imu_clear_status();

    if (!bus) {
        imu_status.last_error = -1;
        return -1;
    }

    if (pin_init(int1_pin, PIN_MODE_INPUT, PIN_PULL_NONE) != 0) {
        imu_status.last_error = -2;
        return -1;
    }
    if (pin_init(int2_pin, PIN_MODE_INPUT, PIN_PULL_NONE) != 0) {
        imu_status.last_error = -3;
        return -1;
    }

    imu_bus      = bus;
    imu_int1_pin = int1_pin;
    imu_int2_pin = int2_pin;

    return 0;
}

int drv_imu_read_int_status(int8_t *int1_level, int8_t *int2_level)
{
    if (!imu_bus || imu_int1_pin < 0 || imu_int2_pin < 0) {
        return -1;
    }

    int8_t int1 = pin_read(imu_int1_pin);
    int8_t int2 = pin_read(imu_int2_pin);
    if (int1 < 0 || int2 < 0) {
        imu_status.last_error = -4;
        return -1;
    }

    imu_status.int1_level = int1;
    imu_status.int2_level = int2;
    if (int1_level) {
        *int1_level = int1;
    }
    if (int2_level) {
        *int2_level = int2;
    }

    return 0;
}

int drv_imu_init(void)
{
    if (!imu_bus) {
        return -1;
    }

    drv_imu_clear_status();
    imu_initialized = 0U;

    if (drv_lsm6dsm_probe(imu_bus, &imu_status.lsm6dsm_addr, &imu_status.lsm6dsm_id) != 0) {
        imu_status.last_error = -5;
        return -1;
    }
    imu_status.lsm6dsm_found = 1U;

    if (drv_lis3mdltr_probe(imu_bus, &imu_status.lis3mdl_addr, &imu_status.lis3mdl_id) != 0) {
        imu_status.last_error = -6;
        return -1;
    }
    imu_status.lis3mdl_found = 1U;

    if (drv_lsm6dsm_init_addr(imu_bus, imu_status.lsm6dsm_addr) != 0) {
        imu_status.last_error = -7;
        return -1;
    }

    if (drv_lis3mdltr_init_addr(imu_bus, imu_status.lis3mdl_addr) != 0) {
        imu_status.last_error = -8;
        return -1;
    }

    (void)drv_imu_read_int_status(&imu_status.int1_level, &imu_status.int2_level);
    imu_initialized       = 1U;
    imu_status.last_error = 0;

    LOG_I("IMU service ready: lsm6dsm addr=0x%02x id=0x%02x, lis3mdl addr=0x%02x id=0x%02x",
          imu_status.lsm6dsm_addr,
          imu_status.lsm6dsm_id,
          imu_status.lis3mdl_addr,
          imu_status.lis3mdl_id);

    return 0;
}

int drv_imu_read(drv_imu_sample_t *sample)
{
    if (!sample || !imu_bus || !imu_initialized) {
        return -1;
    }

    drv_imu_sample_t current_sample = {0};
    if (drv_lsm6dsm_read_accel_addr(imu_bus, imu_status.lsm6dsm_addr, &current_sample.accel) != 0) {
        imu_status.last_error = -9;
        return -1;
    }
    if (drv_lsm6dsm_read_gyro_addr(imu_bus, imu_status.lsm6dsm_addr, &current_sample.gyro) != 0) {
        imu_status.last_error = -10;
        return -1;
    }
    if (drv_lis3mdltr_read_mag_addr(imu_bus, imu_status.lis3mdl_addr, &current_sample.mag) != 0) {
        imu_status.last_error = -11;
        return -1;
    }

    (void)drv_lsm6dsm_read_temp_addr(imu_bus, imu_status.lsm6dsm_addr, &current_sample.lsm6dsm_temp);
    (void)drv_lis3mdltr_read_temp_addr(imu_bus, imu_status.lis3mdl_addr, &current_sample.lis3mdl_temp);
    (void)drv_imu_read_int_status(&imu_status.int1_level, &imu_status.int2_level);

    current_sample.valid_flags = DRV_IMU_FLAG_LSM6DSM_READY | DRV_IMU_FLAG_LIS3MDL_READY | DRV_IMU_FLAG_SAMPLE_VALID;
    imu_status.last_error      = 0;
    *sample                    = current_sample;

    return 0;
}

int drv_imu_get_status(drv_imu_status_t *status)
{
    if (!status) {
        return -1;
    }

    (void)drv_imu_read_int_status(&imu_status.int1_level, &imu_status.int2_level);
    *status = imu_status;

    return 0;
}

static void drv_imu_test_print_status(const char *tag, const drv_imu_status_t *status)
{
    if (!tag) {
        tag = "imu_monitor";
    }
    if (!status) {
        return;
    }

    rt_kprintf("%s: status lsm6dsm_found=%u addr=0x%02x id=0x%02x "
               "lis3mdl_found=%u addr=0x%02x id=0x%02x int1=%d int2=%d last_error=%d\n",
               tag,
               status->lsm6dsm_found,
               status->lsm6dsm_addr,
               status->lsm6dsm_id,
               status->lis3mdl_found,
               status->lis3mdl_addr,
               status->lis3mdl_id,
               status->int1_level,
               status->int2_level,
               status->last_error);
}

static uint32_t drv_imu_parse_u32_arg(int argc, char **argv, int index, uint32_t default_value, uint32_t min_value, uint32_t max_value)
{
    if (argc <= index || !argv || !argv[index]) {
        return default_value;
    }

    char *end           = RT_NULL;
    unsigned long value = strtoul(argv[index], &end, 0);
    if (end == argv[index] || (end && *end != '\0')) {
        return default_value;
    }
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }

    return (uint32_t)value;
}

static int drv_imu_monitor_run(const char *tag, uint32_t sample_count, uint32_t delay_ms, uint8_t print_pass)
{
    if (!tag) {
        tag = "imu_monitor";
    }

    rt_kprintf("%s: start samples=%u delay_ms=%u%s\n", tag, sample_count, delay_ms, sample_count == 0U ? " forever" : "");

    int ret = drv_imu_init();
    drv_imu_status_t status;
    if (drv_imu_get_status(&status) == 0) {
        drv_imu_test_print_status(tag, &status);
    }

    if (ret != 0) {
        rt_kprintf("%s: init FAIL ret=%d\n", tag, ret);
        return ret;
    }

    for (uint32_t i = 0U; sample_count == 0U || i < sample_count; ++i) {
        drv_imu_sample_t sample;
        ret = drv_imu_read(&sample);
        if (drv_imu_get_status(&status) == 0) {
            drv_imu_test_print_status(tag, &status);
        }

        if (ret != 0) {
            rt_kprintf("%s: sample %u FAIL ret=%d\n", tag, i, ret);
            return ret;
        }

        rt_kprintf("%s: sample %u "
                   "lsm6dsm=0x%02x/0x%02x lis3mdl=0x%02x/0x%02x "
                   "accel=%d,%d,%d gyro=%d,%d,%d mag=%d,%d,%d temp=%d,%d int=%d,%d flags=0x%02x err=%d\n",
                   tag,
                   i,
                   status.lsm6dsm_addr,
                   status.lsm6dsm_id,
                   status.lis3mdl_addr,
                   status.lis3mdl_id,
                   sample.accel.x,
                   sample.accel.y,
                   sample.accel.z,
                   sample.gyro.x,
                   sample.gyro.y,
                   sample.gyro.z,
                   sample.mag.x,
                   sample.mag.y,
                   sample.mag.z,
                   sample.lsm6dsm_temp.temp_raw,
                   sample.lis3mdl_temp.temp_raw,
                   status.int1_level,
                   status.int2_level,
                   sample.valid_flags,
                   status.last_error);

        rt_thread_mdelay(delay_ms);
    }

    if (print_pass) {
        rt_kprintf("%s: PASS samples=%u\n", tag, sample_count);
    }
    return 0;
}

static int imu_test(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    return drv_imu_monitor_run("imu_test", DRV_IMU_TEST_SAMPLE_COUNT, DRV_IMU_TEST_DELAY_MS, 1U);
}
MSH_CMD_EXPORT(imu_test, read LSM6DSM and LIS3MDL samples through the unified IMU service);

static int imu_monitor(int argc, char **argv)
{
    uint32_t delay_ms     = drv_imu_parse_u32_arg(argc, argv, 1, DRV_IMU_MONITOR_DELAY_MS, 10U, DRV_IMU_MONITOR_MAX_DELAY);
    uint32_t sample_count = drv_imu_parse_u32_arg(argc, argv, 2, 0U, 0U, 0xFFFFFFFFU);

    return drv_imu_monitor_run("imu_monitor", sample_count, delay_ms, 0U);
}
MSH_CMD_EXPORT(imu_monitor, continuously print IMU samples; usage imu_monitor[delay_ms][count] with count 0 forever);

#ifdef Y_TRACE_IMU_TEST_AUTORUN
static int drv_imu_test_autorun(void)
{
    return imu_test(0, RT_NULL);
}
INIT_APP_EXPORT(drv_imu_test_autorun);
#endif

#ifdef Y_TRACE_IMU_MONITOR_AUTORUN
static int drv_imu_monitor_autorun(void)
{
    return drv_imu_monitor_run("imu_monitor", 0U, DRV_IMU_MONITOR_DELAY_MS, 0U);
}
INIT_APP_EXPORT(drv_imu_monitor_autorun);
#endif
