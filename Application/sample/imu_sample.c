#include "rtthread.h"
#include <Drivers/drv_lsm6dsm.h>
#include <Drivers/drv_soft_iic.h>

#define LOG_TAG "imu.sample"
#define LOG_LVL LOG_LVL_DBG
#include "ulog.h"

// LSM6DSM IMU线程入口函数
static void imu_thread_entry(struct _soft_i2c_bus *i2c_bus)
{
    lsm6dsm_accel_data_t accel_data;
    lsm6dsm_gyro_data_t gyro_data;
    lsm6dsm_temp_data_t temp_data;
    int retry_count = 0;
    const int max_retries = 10;

    while (retry_count < max_retries)
    {
        if (drv_lsm6dsm_init(i2c_bus) == 0)
        {
            LOG_I("LSM6DSM initialized successfully!");
            // 打印诊断信息
            drv_lsm6dsm_diagnose(i2c_bus);
            break;
        }

        LOG_W("LSM6DSM initialization failed! (Attempt %d/%d)",
              retry_count + 1, max_retries);
        retry_count++;
        if (retry_count >= max_retries)
        {
            LOG_E("LSM6DSM initialization failed after %d attempts. "
                  "Please check hardware connections.",
                  max_retries);
            return; // 退出线程
        }
        rt_thread_mdelay(1000);
    }

    while (1)
    {
        // 读取加速度计数据
        if (drv_lsm6dsm_read_accel(i2c_bus, &accel_data) == 0)
        {
            LOG_D("Accel - X: %6d, Y: %6d, Z: %6d | ", accel_data.x,
                  accel_data.y, accel_data.z);
        }
        else
        {
            LOG_W("Failed to read accelerometer data | ");
        }

        // 读取陀螺仪数据
        if (drv_lsm6dsm_read_gyro(i2c_bus, &gyro_data) == 0)
        {
            LOG_D("Gyro - X: %6d, Y: %6d, Z: %6d | ", gyro_data.x,
                  gyro_data.y, gyro_data.z);
        }
        else
        {
            LOG_W("Failed to read gyroscope data | ");
        }

        // 读取温度数据
        if (drv_lsm6dsm_read_temp(i2c_bus, &temp_data) == 0)
        {
            // LSM6DSM温度计算: Temp(°C) = (temp_raw / 256) + 25
            float temperature = (temp_data.temp_raw / 256.0f) + 25.0f;
            LOG_D("Temp: %.2f°C", temperature);
        }
        else
        {
            LOG_W("Failed to read temperature");
        }
        rt_thread_mdelay(500);
    }
}


int imu_sample(struct _soft_i2c_bus *bus)
{
    if (bus == RT_NULL)
    {
        LOG_E("I2C bus pointer is NULL!\n");
        return RT_ERROR;
    }

    rt_thread_t imu_thread =
        rt_thread_create("imu", (void (*)(void *))imu_thread_entry, bus, 2048, 10, 20);

    if (imu_thread != RT_NULL)
    {
        rt_thread_startup(imu_thread);
        return RT_EOK;
    }
    else
    {
        LOG_E("Failed to create IMU thread!\n");
        return RT_ERROR;
    }
}