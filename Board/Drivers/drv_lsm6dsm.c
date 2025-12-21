#include "drv_lsm6dsm.h"
#include "drv_soft_iic.h"
#include <rtthread.h>

#define LOG_TAG "drv.lsm6dsm"
#define LOG_LVL LOG_LVL_DBG
#include "ulog.h"

/**
 * @brief 写LSM6DSM寄存器
 *
 * @param bus 软件I2C总线句柄
 * @param reg 寄存器地址
 * @param data 写入的数据
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_write_reg(struct _soft_i2c_bus *bus, uint8_t reg, uint8_t data)
{
    uint8_t buf[2];
    int ret;

    if (!bus)
    {
        return -1;
    }

    buf[0] = reg;
    buf[1] = data;

    ret = drv_soft_i2c_master_send(bus, LSM6DSM_I2C_ADDR, I2C_FLAG_WR, buf, 2);

    return (ret == 2) ? 0 : -1;
}

/**
 * @brief 读LSM6DSM寄存器
 *
 * @param bus 软件I2C总线句柄
 * @param reg 寄存器地址
 * @param data 读取的数据缓冲区
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_read_reg(struct _soft_i2c_bus *bus, uint8_t reg, uint8_t *data)
{
    int ret;

    if (!bus || !data)
    {
        return -1;
    }

    /* 先写寄存器地址 */
    ret = drv_soft_i2c_master_send(bus, LSM6DSM_I2C_ADDR, I2C_FLAG_WR, &reg, 1);
    if (ret != 1)
    {
        return -1;
    }

    /* 再读取数据 */
    ret = drv_soft_i2c_master_recv(bus, LSM6DSM_I2C_ADDR, I2C_FLAG_RD, data, 1);

    return (ret == 1) ? 0 : -1;
}

/**
 * @brief 读取LSM6DSM多个寄存器
 *
 * @param bus 软件I2C总线句柄
 * @param reg 起始寄存器地址
 * @param data 读取的数据缓冲区
 * @param len 读取的数据长度
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_read_regs(struct _soft_i2c_bus *bus, uint8_t reg, uint8_t *data, uint8_t len)
{
    int ret;

    if (!bus || !data || len == 0)
    {
        return -1;
    }

    /* 注意：CTRL3_C中已配置IF_INC=1（自动地址递增），不需要在地址上加0x80 */

    /* 先写寄存器地址 */
    ret = drv_soft_i2c_master_send(bus, LSM6DSM_I2C_ADDR, I2C_FLAG_WR, &reg, 1);
    if (ret != 1)
    {
        return -1;
    }

    /* 再读取多个数据 */
    ret = drv_soft_i2c_master_recv(bus, LSM6DSM_I2C_ADDR, I2C_FLAG_RD, data, len);

    return (ret == len) ? 0 : -1;
}

/**
 * @brief 读取设备ID
 *
 * @param bus 软件I2C总线句柄
 * @param id 设备ID缓冲区
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_read_id(struct _soft_i2c_bus *bus, uint8_t *id)
{
    return drv_lsm6dsm_read_reg(bus, LSM6DSM_WHO_AM_I, id);
}

/**
 * @brief 读取加速度计数据
 *
 * @param bus 软件I2C总线句柄
 * @param accel_data 加速度计数据结构指针
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_read_accel(struct _soft_i2c_bus *bus, lsm6dsm_accel_data_t *accel_data)
{
    uint8_t buf[6];
    int ret;

    if (!bus || !accel_data)
    {
        return -1;
    }

    /* 从OUTX_L_XL开始连续读取6个字节 */
    ret = drv_lsm6dsm_read_regs(bus, LSM6DSM_OUTX_L_XL, buf, 6);
    if (ret != 0)
    {
        return ret;
    }

    /* 组合高低字节 */
    accel_data->x = (int16_t)((buf[1] << 8) | buf[0]);
    accel_data->y = (int16_t)((buf[3] << 8) | buf[2]);
    accel_data->z = (int16_t)((buf[5] << 8) | buf[4]);

    return 0;
}

/**
 * @brief 读取陀螺仪数据
 *
 * @param bus 软件I2C总线句柄
 * @param gyro_data 陀螺仪数据结构指针
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_read_gyro(struct _soft_i2c_bus *bus, lsm6dsm_gyro_data_t *gyro_data)
{
    uint8_t buf[6];
    int ret;

    if (!bus || !gyro_data)
    {
        return -1;
    }

    /* 从OUTX_L_G开始连续读取6个字节 */
    ret = drv_lsm6dsm_read_regs(bus, LSM6DSM_OUTX_L_G, buf, 6);
    if (ret != 0)
    {
        return ret;
    }

    /* 组合高低字节 */
    gyro_data->x = (int16_t)((buf[1] << 8) | buf[0]);
    gyro_data->y = (int16_t)((buf[3] << 8) | buf[2]);
    gyro_data->z = (int16_t)((buf[5] << 8) | buf[4]);

    return 0;
}

/**
 * @brief 读取温度数据
 *
 * @param bus 软件I2C总线句柄
 * @param temp_data 温度数据结构指针
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_read_temp(struct _soft_i2c_bus *bus, lsm6dsm_temp_data_t *temp_data)
{
    uint8_t buf[2];
    int ret;

    if (!bus || !temp_data)
    {
        return -1;
    }

    /* 从OUT_TEMP_L开始连续读取2个字节 */
    ret = drv_lsm6dsm_read_regs(bus, LSM6DSM_OUT_TEMP_L, buf, 2);
    if (ret != 0)
    {
        return ret;
    }

    /* 组合高低字节 */
    temp_data->temp_raw = (int16_t)((buf[1] << 8) | buf[0]);

    return 0;
}

/**
 * @brief 设置加速度计输出数据率
 *
 * @param bus 软件I2C总线句柄
 * @param odr 输出数据率
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_set_accel_odr(struct _soft_i2c_bus *bus, uint8_t odr)
{
    uint8_t value;
    int ret;

    if (!bus)
    {
        return -1;
    }

    /* 读取CTRL1_XL当前值 */
    ret = drv_lsm6dsm_read_reg(bus, LSM6DSM_CTRL1_XL, &value);
    if (ret != 0)
    {
        return ret;
    }

    /* 清除ODR位并设置新值 */
    value &= ~0xF0;
    value |= (odr & 0xF0);

    /* 写回寄存器 */
    return drv_lsm6dsm_write_reg(bus, LSM6DSM_CTRL1_XL, value);
}

/**
 * @brief 设置加速度计量程
 *
 * @param bus 软件I2C总线句柄
 * @param fs 量程
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_set_accel_fullscale(struct _soft_i2c_bus *bus, uint8_t fs)
{
    uint8_t value;
    int ret;

    if (!bus)
    {
        return -1;
    }

    /* 读取CTRL1_XL当前值 */
    ret = drv_lsm6dsm_read_reg(bus, LSM6DSM_CTRL1_XL, &value);
    if (ret != 0)
    {
        return ret;
    }

    /* 清除FS位并设置新值 */
    value &= ~0x0C;
    value |= (fs & 0x0C);

    /* 写回寄存器 */
    return drv_lsm6dsm_write_reg(bus, LSM6DSM_CTRL1_XL, value);
}

/**
 * @brief 设置陀螺仪输出数据率
 *
 * @param bus 软件I2C总线句柄
 * @param odr 输出数据率
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_set_gyro_odr(struct _soft_i2c_bus *bus, uint8_t odr)
{
    uint8_t value;
    int ret;

    if (!bus)
    {
        return -1;
    }

    /* 读取CTRL2_G当前值 */
    ret = drv_lsm6dsm_read_reg(bus, LSM6DSM_CTRL2_G, &value);
    if (ret != 0)
    {
        return ret;
    }

    /* 清除ODR位并设置新值 */
    value &= ~0xF0;
    value |= (odr & 0xF0);

    /* 写回寄存器 */
    return drv_lsm6dsm_write_reg(bus, LSM6DSM_CTRL2_G, value);
}

/**
 * @brief 设置陀螺仪量程
 *
 * @param bus 软件I2C总线句柄
 * @param fs 量程
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_set_gyro_fullscale(struct _soft_i2c_bus *bus, uint8_t fs)
{
    uint8_t value;
    int ret;

    if (!bus)
    {
        return -1;
    }

    /* 读取CTRL2_G当前值 */
    ret = drv_lsm6dsm_read_reg(bus, LSM6DSM_CTRL2_G, &value);
    if (ret != 0)
    {
        return ret;
    }

    /* 清除FS位并设置新值 */
    value &= ~0x0C;
    value |= (fs & 0x0C);

    /* 写回寄存器 */
    return drv_lsm6dsm_write_reg(bus, LSM6DSM_CTRL2_G, value);
}

/**
 * @brief 初始化LSM6DSM设备
 *
 * @param bus 软件I2C总线句柄
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_init(struct _soft_i2c_bus *bus)
{
    uint8_t id = 0;
    int ret;

    if (!bus)
    {
        return -1;
    }

    /* 读取并验证设备ID */
    ret = drv_lsm6dsm_read_id(bus, &id);
    if (ret != 0)
    {
        LOG_E("LSM6DSM: Failed to read device ID");
        return -1;
    }

    if (id != LSM6DSM_ID)
    {
        LOG_E("LSM6DSM: Invalid device ID: 0x%02X (expected 0x%02X)", id, LSM6DSM_ID);
        return -1;
    }

    LOG_I("LSM6DSM: Device ID verified: 0x%02X", id);
    /* 软复位 */
    ret = drv_lsm6dsm_write_reg(bus, LSM6DSM_CTRL3_C, 0x01);
    if (ret != 0)
    {
        LOG_E("LSM6DSM: Failed to perform soft reset");
        return -1;
    }

    /* 等待复位完成 */
    rt_thread_mdelay(10);

    /* 配置CTRL3_C: 使能BDU(块数据更新)和自动地址递增 */
    ret = drv_lsm6dsm_write_reg(bus, LSM6DSM_CTRL3_C, 0x44);
    if (ret != 0)
    {
        LOG_E("LSM6DSM: Failed to configure CTRL3_C");
        return -1;
    }

    /* 配置CTRL1_XL: 加速度计ODR=104Hz, FS=±2g */
    ret = drv_lsm6dsm_write_reg(bus, LSM6DSM_CTRL1_XL, LSM6DSM_XL_ODR_104HZ | LSM6DSM_XL_FS_2G);
    if (ret != 0)
    {
        LOG_E("LSM6DSM: Failed to configure CTRL1_XL");
        return -1;
    }

    /* 配置CTRL2_G: 陀螺仪ODR=104Hz, FS=250dps */
    ret = drv_lsm6dsm_write_reg(bus, LSM6DSM_CTRL2_G, LSM6DSM_G_ODR_104HZ | LSM6DSM_G_FS_250DPS);
    if (ret != 0)
    {
        LOG_E("LSM6DSM: Failed to configure CTRL2_G");
        return -1;
    }

    /* 配置CTRL4_C: 禁用I2C接口 */
    ret = drv_lsm6dsm_write_reg(bus, LSM6DSM_CTRL4_C, 0x00);
    if (ret != 0)
    {
        LOG_E("LSM6DSM: Failed to configure CTRL4_C");
        return -1;
    }

    /* 配置CTRL5_C: 加速度计和陀螺仪自测禁用，舍入禁用 */
    ret = drv_lsm6dsm_write_reg(bus, LSM6DSM_CTRL5_C, 0x00);
    if (ret != 0)
    {
        LOG_E("LSM6DSM: Failed to configure CTRL5_C");
        return -1;
    }

    /* 配置CTRL6_C: 陀螺仪高性能模式 */
    ret = drv_lsm6dsm_write_reg(bus, LSM6DSM_CTRL6_C, 0x00);
    if (ret != 0)
    {
        LOG_E("LSM6DSM: Failed to configure CTRL6_C");
        return -1;
    }

    /* 配置CTRL7_G: 陀螺仪高性能模式，高通滤波器禁用 */
    ret = drv_lsm6dsm_write_reg(bus, LSM6DSM_CTRL7_G, 0x00);
    if (ret != 0)
    {
        LOG_E("LSM6DSM: Failed to configure CTRL7_G");
        return -1;
    }

    /* 配置CTRL8_XL: 加速度计低通滤波器 */
    ret = drv_lsm6dsm_write_reg(bus, LSM6DSM_CTRL8_XL, 0x00);
    if (ret != 0)
    {
        LOG_E("LSM6DSM: Failed to configure CTRL8_XL");
        return -1;
    }

    /* 配置CTRL9_XL: 使能加速度计X/Y/Z轴 (bit 3-5 = 0表示使能) */
    ret = drv_lsm6dsm_write_reg(bus, LSM6DSM_CTRL9_XL, 0x00);
    if (ret != 0)
    {
        LOG_E("LSM6DSM: Failed to configure CTRL9_XL");
        return -1;
    }

    /* 配置CTRL10_C: 使能时间戳和功能 */
    ret = drv_lsm6dsm_write_reg(bus, LSM6DSM_CTRL10_C, 0x00);
    if (ret != 0)
    {
        LOG_E("LSM6DSM: Failed to configure CTRL10_C");
        return -1;
    }

    /* 等待传感器稳定 */
    rt_thread_mdelay(100);

    LOG_I("LSM6DSM: Initialization successful");

    return 0;
}

/**
 * @brief 诊断LSM6DSM配置（用于调试）
 *
 * @param bus 软件I2C总线句柄
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_diagnose(struct _soft_i2c_bus *bus)
{
    uint8_t ctrl_regs[10];
    int ret;

    if (!bus)
    {
        return -1;
    }

    LOG_I("===== LSM6DSM Configuration Dump =====");

    /* 读取所有控制寄存器 */
    ret = drv_lsm6dsm_read_reg(bus, LSM6DSM_CTRL1_XL, &ctrl_regs[0]);
    ret |= drv_lsm6dsm_read_reg(bus, LSM6DSM_CTRL2_G, &ctrl_regs[1]);
    ret |= drv_lsm6dsm_read_reg(bus, LSM6DSM_CTRL3_C, &ctrl_regs[2]);
    ret |= drv_lsm6dsm_read_reg(bus, LSM6DSM_CTRL4_C, &ctrl_regs[3]);
    ret |= drv_lsm6dsm_read_reg(bus, LSM6DSM_CTRL5_C, &ctrl_regs[4]);
    ret |= drv_lsm6dsm_read_reg(bus, LSM6DSM_CTRL6_C, &ctrl_regs[5]);
    ret |= drv_lsm6dsm_read_reg(bus, LSM6DSM_CTRL7_G, &ctrl_regs[6]);
    ret |= drv_lsm6dsm_read_reg(bus, LSM6DSM_CTRL8_XL, &ctrl_regs[7]);
    ret |= drv_lsm6dsm_read_reg(bus, LSM6DSM_CTRL9_XL, &ctrl_regs[8]);
    ret |= drv_lsm6dsm_read_reg(bus, LSM6DSM_CTRL10_C, &ctrl_regs[9]);

    if (ret != 0)
    {
        LOG_E("Failed to read control registers");
        return -1;
    }

    LOG_I("CTRL1_XL (0x10): 0x%02X [Accel ODR=%d, FS=%d]",
          ctrl_regs[0], (ctrl_regs[0] >> 4), (ctrl_regs[0] >> 2) & 0x03);
    LOG_I("CTRL2_G  (0x11): 0x%02X [Gyro ODR=%d, FS=%d]",
          ctrl_regs[1], (ctrl_regs[1] >> 4), (ctrl_regs[1] >> 2) & 0x03);
    LOG_I("CTRL3_C  (0x12): 0x%02X [BDU=%d, IF_INC=%d]",
          ctrl_regs[2], (ctrl_regs[2] >> 6) & 0x01, (ctrl_regs[2] >> 2) & 0x01);
    LOG_I("CTRL4_C  (0x13): 0x%02X", ctrl_regs[3]);
    LOG_I("CTRL5_C  (0x14): 0x%02X", ctrl_regs[4]);
    LOG_I("CTRL6_C  (0x15): 0x%02X", ctrl_regs[5]);
    LOG_I("CTRL7_G  (0x16): 0x%02X", ctrl_regs[6]);
    LOG_I("CTRL8_XL (0x17): 0x%02X", ctrl_regs[7]);
    LOG_I("CTRL9_XL (0x18): 0x%02X [XL_EN=%d]",
          ctrl_regs[8], (~ctrl_regs[8] >> 3) & 0x07);
    LOG_I("CTRL10_C (0x19): 0x%02X", ctrl_regs[9]);
    LOG_I("======================================");

    return 0;
}

