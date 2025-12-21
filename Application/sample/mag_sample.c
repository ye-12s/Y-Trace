#include <rtthread.h>
#include <Drivers/drv_lis3mdltr.h>
#include <Drivers/drv_soft_iic.h>
#include <math.h>

#define LOG_TAG "magnetometer.sample"
#define LOG_LVL LOG_LVL_DBG
#include "ulog.h"


// 磁力计线程入口函数
static void mag_thread_entry(struct _soft_i2c_bus *i2c_bus)
{
    lis3mdltr_mag_data_t mag_data;
    int retry_count = 0;
    const int max_retries = 10;  // 最多重试10次

    while (retry_count < max_retries)
    {
        if (drv_lis3mdltr_init(i2c_bus) == 0)
        {
            LOG_I("LIS3MDLTR initialized successfully!");
            LOG_I("Expected Earth magnetic field: 25-65 μT (normal range)");
            LOG_W("If readings are consistently high, check for nearby magnetic interference");
            LOG_I("Heading: 0°=N, 90°=E, 180°=S, 270°=W (device horizontal)");
            break;
        }

        retry_count++;
        if (retry_count >= max_retries)
        {
            LOG_E("LIS3MDLTR initialization failed after %d attempts. "
                  "Please check hardware connections.",
                  max_retries);
            return; // 退出线程
        }
        rt_thread_mdelay(1000);
    }

    while (1)
    {
        if (drv_lis3mdltr_read_mag(i2c_bus, &mag_data) == 0)
        {
            int64_t x_sq = (int64_t)mag_data.x * mag_data.x;
            int64_t y_sq = (int64_t)mag_data.y * mag_data.y;
            int64_t z_sq = (int64_t)mag_data.z * mag_data.z;
            int64_t mag_squared = x_sq + y_sq + z_sq;

            uint32_t mag_magnitude = 0;
            if (mag_squared > 0)
            {
                uint64_t x = mag_squared;
                uint64_t y = (x + 1) / 2;

                while (y < x)
                {
                    x = y;
                    y = (x + mag_squared / x) / 2;
                }
                mag_magnitude = (uint32_t)x;
            }

            // 转换为微特斯拉 (μT)
            // ±4 Gauss量程: 1 Gauss = 100 μT, 灵敏度约6842 LSB/Gauss
            uint32_t mag_uT = (mag_magnitude * 100) / 6842;

            // 计算航向角（假设设备水平放置）
            // heading = atan2(y, x) * 180 / π
            // 注意：这里假设X指向前，Y指向左
            float heading_rad = atan2f((float)mag_data.y, (float)mag_data.x);
            float heading_deg = heading_rad * 180.0f / 3.14159265f;

            // 归一化到 0-360 度
            if (heading_deg < 0)
            {
                heading_deg += 360.0f;
            }

            // 确定方向（8个方位）
            const char *direction;
            if (heading_deg >= 337.5f || heading_deg < 22.5f)
                direction = "N ";  // 北
            else if (heading_deg >= 22.5f && heading_deg < 67.5f)
                direction = "NE";  // 东北
            else if (heading_deg >= 67.5f && heading_deg < 112.5f)
                direction = "E ";  // 东
            else if (heading_deg >= 112.5f && heading_deg < 157.5f)
                direction = "SE";  // 东南
            else if (heading_deg >= 157.5f && heading_deg < 202.5f)
                direction = "S ";  // 南
            else if (heading_deg >= 202.5f && heading_deg < 247.5f)
                direction = "SW";  // 西南
            else if (heading_deg >= 247.5f && heading_deg < 292.5f)
                direction = "W ";  // 西
            else
                direction = "NW";  // 西北

            LOG_D("Mag[X:%5d Y:%5d Z:%5d] |B|=%d(%dμT) HDG:%3d° %s",
                  mag_data.x, mag_data.y, mag_data.z,
                  mag_magnitude, mag_uT, (int)heading_deg, direction);
        }
        rt_thread_mdelay(500);
    }
}

/**
 * @brief 磁力计示例程序
 *
 * @param bus 软件I2C总线指针
 * @return int 返回状态码
 */
int mag_sample(struct _soft_i2c_bus *bus)
{
    rt_thread_t tid;
    if (bus == RT_NULL)
    {
        LOG_E("I2C bus is NULL!");
        return RT_EINVAL;
    }

    tid = rt_thread_create(
              "mag", (void (*)(void *))mag_thread_entry, bus, 2048, 10, 20);
    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
        LOG_I("Magnetometer thread created successfully!");
        return RT_EOK;
    }
    else
    {
        LOG_E("Failed to create magnetometer thread!");
        return RT_ERROR;
    }
}
