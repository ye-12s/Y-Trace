#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_source_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(file, path);

    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_END));
    const long size = ftell(file);
    TEST_ASSERT_GREATER_THAN_INT32(0, size);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_SET));

    char *buffer = (char *)malloc((size_t)size + 1U);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_EQUAL_size_t((size_t)size, fread(buffer, 1U, (size_t)size, file));
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

static void assert_contains(const char *source, const char *needle)
{
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(source, needle), needle);
}

void setUp(void)
{
}

void tearDown(void)
{
}

void test_lsm6dsm_exposes_backward_compatible_address_aware_helpers(void)
{
    char *header = read_source_file(Y_TRACE_LSM6DSM_HEADER_PATH);
    char *source = read_source_file(Y_TRACE_LSM6DSM_SOURCE_PATH);

    assert_contains(header, "#define LSM6DSM_I2C_ADDR_LOW");
    assert_contains(header, "0x6A");
    assert_contains(header, "#define LSM6DSM_I2C_ADDR_HIGH");
    assert_contains(header, "0x6B");
    assert_contains(header, "int drv_lsm6dsm_probe(struct _soft_i2c_bus *bus, uint8_t *addr, uint8_t *id);");
    assert_contains(header, "int drv_lsm6dsm_init_addr(struct _soft_i2c_bus *bus, uint8_t addr);");
    assert_contains(header, "int drv_lsm6dsm_read_reg_addr(struct _soft_i2c_bus *bus, uint8_t addr, uint8_t reg, uint8_t *data);");
    assert_contains(header, "int drv_lsm6dsm_read_accel_addr(struct _soft_i2c_bus *bus, uint8_t addr, lsm6dsm_accel_data_t *accel_data);");
    assert_contains(header, "int drv_lsm6dsm_read_gyro_addr(struct _soft_i2c_bus *bus, uint8_t addr, lsm6dsm_gyro_data_t *gyro_data);");
    assert_contains(header, "int drv_lsm6dsm_read_temp_addr(struct _soft_i2c_bus *bus, uint8_t addr, lsm6dsm_temp_data_t *temp_data);");

    assert_contains(source, "static const uint8_t lsm6dsm_addr_candidates[]");
    assert_contains(source, "drv_lsm6dsm_init_addr(bus, LSM6DSM_I2C_ADDR)");
    assert_contains(source, "LSM6DSM_XL_ODR_104HZ | LSM6DSM_XL_FS_2G");
    assert_contains(source, "LSM6DSM_G_ODR_104HZ | LSM6DSM_G_FS_250DPS");

    free(header);
    free(source);
}

void test_lis3mdl_exposes_backward_compatible_address_aware_helpers(void)
{
    char *header = read_source_file(Y_TRACE_LIS3MDL_HEADER_PATH);
    char *source = read_source_file(Y_TRACE_LIS3MDL_SOURCE_PATH);

    assert_contains(header, "#define LIS3MDLTR_I2C_ADDR_LOW");
    assert_contains(header, "0x1C");
    assert_contains(header, "#define LIS3MDLTR_I2C_ADDR_HIGH");
    assert_contains(header, "0x1E");
    assert_contains(header, "#define LIS3MDLTR_I2C_ADDR      LIS3MDLTR_I2C_ADDR_LOW");
    assert_contains(header, "#define LIS3MDLTR_CTRL_REG5_BDU 0x40");
    assert_contains(header, "int drv_lis3mdltr_probe(struct _soft_i2c_bus *bus, uint8_t *addr, uint8_t *id);");
    assert_contains(header, "int drv_lis3mdltr_init_addr(struct _soft_i2c_bus *bus, uint8_t addr);");
    assert_contains(header, "int drv_lis3mdltr_read_reg_addr(struct _soft_i2c_bus *bus, uint8_t addr, uint8_t reg, uint8_t *data);");
    assert_contains(header, "int drv_lis3mdltr_read_mag_addr(struct _soft_i2c_bus *bus, uint8_t addr, lis3mdltr_mag_data_t *mag_data);");
    assert_contains(header, "int drv_lis3mdltr_read_temp_addr(struct _soft_i2c_bus *bus, uint8_t addr, lis3mdltr_temp_data_t *temp_data);");

    assert_contains(source, "static const uint8_t lis3mdltr_addr_candidates[]");
    assert_contains(source, "drv_lis3mdltr_init_addr(bus, LIS3MDLTR_I2C_ADDR)");
    assert_contains(source, "LIS3MDLTR_FS_4GAUSS");
    assert_contains(source, "LIS3MDLTR_ODR_10HZ");
    assert_contains(source, "LIS3MDLTR_CTRL_REG5_BDU");

    free(header);
    free(source);
}

void test_unified_imu_service_exposes_external_module_polling_contract(void)
{
    char *header = read_source_file(Y_TRACE_IMU_HEADER_PATH);
    char *source = read_source_file(Y_TRACE_IMU_SOURCE_PATH);

    assert_contains(header, "typedef struct drv_imu_sample");
    assert_contains(header, "lsm6dsm_accel_data_t accel");
    assert_contains(header, "lsm6dsm_gyro_data_t gyro");
    assert_contains(header, "lis3mdltr_mag_data_t mag");
    assert_contains(header, "typedef struct drv_imu_status");
    assert_contains(header, "uint8_t lsm6dsm_addr");
    assert_contains(header, "uint8_t lis3mdl_addr");
    assert_contains(header, "int8_t int1_level");
    assert_contains(header, "int8_t int2_level");
    assert_contains(header, "int drv_imu_bind(struct _soft_i2c_bus *bus, pin_t int1_pin, pin_t int2_pin);");
    assert_contains(header, "int drv_imu_init(void);");
    assert_contains(header, "int drv_imu_read(drv_imu_sample_t *sample);");
    assert_contains(header, "int drv_imu_get_status(drv_imu_status_t *status);");
    assert_contains(header, "int drv_imu_read_int_status(int8_t *int1_level, int8_t *int2_level);");

    assert_contains(source, "drv_lsm6dsm_probe(imu_bus");
    assert_contains(source, "drv_lis3mdltr_probe(imu_bus");
    assert_contains(source, "drv_imu_clear_status();");
    assert_contains(source, "imu_initialized = 0U;");
    assert_contains(source, "drv_lsm6dsm_init_addr(imu_bus, imu_status.lsm6dsm_addr)");
    assert_contains(source, "drv_lis3mdltr_init_addr(imu_bus, imu_status.lis3mdl_addr)");
    assert_contains(source, "drv_lsm6dsm_read_accel_addr(imu_bus, imu_status.lsm6dsm_addr");
    assert_contains(source, "drv_lsm6dsm_read_gyro_addr(imu_bus, imu_status.lsm6dsm_addr");
    assert_contains(source, "drv_lis3mdltr_read_mag_addr(imu_bus, imu_status.lis3mdl_addr");
    assert_contains(source, "pin_init(int1_pin, PIN_MODE_INPUT, PIN_PULL_NONE)");
    assert_contains(source, "imu_bus");
    assert_contains(source, "RT_NULL;");
    assert_contains(source, "imu_status.last_error = -2;");
    assert_contains(source, "imu_status.last_error = -3;");
    assert_contains(source, "pin_read(imu_int1_pin)");
    assert_contains(source, "pin_read(imu_int2_pin)");
    TEST_ASSERT_NULL_MESSAGE(strstr(source, "rt_device_register"), "first pass must not register an RT-Thread device");
    TEST_ASSERT_NULL_MESSAGE(strstr(source, "FIFO"), "first pass must not implement FIFO/background streaming");

    free(header);
    free(source);
}

void test_unified_imu_service_provides_explicit_runtime_smoke_test(void)
{
    char *source        = read_source_file(Y_TRACE_IMU_SOURCE_PATH);
    char *cmake_options = read_source_file(Y_TRACE_CMAKE_OPTIONS_PATH);

    assert_contains(source, "#include <finsh.h>");
    assert_contains(source, "static int imu_test(int argc, char **argv)");
    assert_contains(source, "drv_imu_init()");
    assert_contains(source, "drv_imu_read(&sample)");
    assert_contains(source, "rt_thread_mdelay");
    assert_contains(source, ": PASS samples=%u");
    assert_contains(source, "MSH_CMD_EXPORT(imu_test");
    assert_contains(source, "static int imu_monitor(int argc, char **argv)");
    assert_contains(source, "drv_imu_parse_u32_arg(argc, argv");
    assert_contains(source, "sample_count == 0U");
    assert_contains(source, ": sample %u");
    assert_contains(source, "MSH_CMD_EXPORT(imu_monitor");
    assert_contains(source, "Y_TRACE_IMU_TEST_AUTORUN");
    assert_contains(source, "INIT_APP_EXPORT(drv_imu_test_autorun)");
    assert_contains(source, "Y_TRACE_IMU_MONITOR_AUTORUN");
    assert_contains(source, "INIT_APP_EXPORT(drv_imu_monitor_autorun)");
    assert_contains(source, "drv_imu_monitor_run(\"imu_monitor\", 0U, DRV_IMU_MONITOR_DELAY_MS, 0U)");

    assert_contains(cmake_options, "option(Y_TRACE_IMU_TEST_AUTORUN");
    assert_contains(cmake_options, "Y_TRACE_IMU_TEST_AUTORUN");
    assert_contains(cmake_options, "option(Y_TRACE_IMU_MONITOR_AUTORUN");
    assert_contains(cmake_options, "Y_TRACE_IMU_MONITOR_AUTORUN");

    free(source);
    free(cmake_options);
}

void test_unified_imu_service_is_built_and_bound_to_board_sensor_bus(void)
{
    char *cmake_source = read_source_file(Y_TRACE_CMAKE_SOURCES_PATH);
    char *app_source   = read_source_file(Y_TRACE_APP_INIT_PATH);

    assert_contains(cmake_source, "Board/Drivers/drv_imu.c");
    assert_contains(app_source, "#include \"Drivers/drv_imu.h\"");
    assert_contains(app_source, "static const pin_t PIN_SENSOR_IMU_INT1 = GET_PIN(C, 7);");
    assert_contains(app_source, "static const pin_t PIN_SENSOR_IMU_INT2 = GET_PIN(C, 8);");
    assert_contains(app_source, "drv_imu_bind(&sensor_i2c_bus, PIN_SENSOR_IMU_INT1, PIN_SENSOR_IMU_INT2)");
    assert_contains(app_source, "LOG_E(\"IMU service bind failed");
    assert_contains(app_source, "(void)drv_ws2812b_init();");
    assert_contains(app_source, "INIT_COMPONENT_EXPORT(lcd_init);");
    assert_contains(app_source, "app_lvgl_port_init();");
    assert_contains(app_source, "app_ws2812b_liuli_start() != RT_EOK");
    TEST_ASSERT_NULL_MESSAGE(strstr(app_source, "drv_imu_init();"),
                             "binding should not automatically probe sensors at boot; external modules own init timing");

    free(cmake_source);
    free(app_source);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lsm6dsm_exposes_backward_compatible_address_aware_helpers);
    RUN_TEST(test_lis3mdl_exposes_backward_compatible_address_aware_helpers);
    RUN_TEST(test_unified_imu_service_exposes_external_module_polling_contract);
    RUN_TEST(test_unified_imu_service_provides_explicit_runtime_smoke_test);
    RUN_TEST(test_unified_imu_service_is_built_and_bound_to_board_sensor_bus);
    return UNITY_END();
}
