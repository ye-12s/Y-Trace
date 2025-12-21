#include "drv_lis3mdltr.h"
#include "drv_soft_iic.h"
#include <rtthread.h>

/**
 * @brief 写LIS3MDLTR寄存器
 * 
 * @param bus 软件I2C总线句柄
 * @param reg 寄存器地址
 * @param data 写入的数据
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_write_reg(struct _soft_i2c_bus *bus, uint8_t reg, uint8_t data)
{
    uint8_t buf[2];
    int ret;
    
    if (!bus)
    {
        return -1;
    }
    
    buf[0] = reg;
    buf[1] = data;
    
    ret = drv_soft_i2c_master_send(bus, LIS3MDLTR_I2C_ADDR, I2C_FLAG_WR, buf, 2);
    
    return (ret == 2) ? 0 : -1;
}

/**
 * @brief 读LIS3MDLTR寄存器
 * 
 * @param bus 软件I2C总线句柄
 * @param reg 寄存器地址
 * @param data 读取的数据缓冲区
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_read_reg(struct _soft_i2c_bus *bus, uint8_t reg, uint8_t *data)
{
    int ret;
    
    if (!bus || !data)
    {
        return -1;
    }
    
    /* 先写寄存器地址 */
    ret = drv_soft_i2c_master_send(bus, LIS3MDLTR_I2C_ADDR, I2C_FLAG_WR, &reg, 1);
    if (ret != 1)
    {
        return -1;
    }
    
    /* 再读取数据 */
    ret = drv_soft_i2c_master_recv(bus, LIS3MDLTR_I2C_ADDR, I2C_FLAG_RD, data, 1);
    
    return (ret == 1) ? 0 : -1;
}

/**
 * @brief 读取LIS3MDLTR多个寄存器
 * 
 * @param bus 软件I2C总线句柄
 * @param reg 起始寄存器地址
 * @param data 读取的数据缓冲区
 * @param len 读取的数据长度
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_read_regs(struct _soft_i2c_bus *bus, uint8_t reg, uint8_t *data, uint8_t len)
{
    int ret;
    
    if (!bus || !data || len == 0)
    {
        return -1;
    }
    
    /* LIS3MDLTR支持自动地址递增，设置最高位为1 */
    reg |= 0x80;
    
    /* 先写寄存器地址 */
    ret = drv_soft_i2c_master_send(bus, LIS3MDLTR_I2C_ADDR, I2C_FLAG_WR, &reg, 1);
    if (ret != 1)
    {
        return -1;
    }
    
    /* 再读取多个数据 */
    ret = drv_soft_i2c_master_recv(bus, LIS3MDLTR_I2C_ADDR, I2C_FLAG_RD, data, len);
    
    return (ret == len) ? 0 : -1;
}

/**
 * @brief 读取设备ID
 * 
 * @param bus 软件I2C总线句柄
 * @param id 设备ID缓冲区
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_read_id(struct _soft_i2c_bus *bus, uint8_t *id)
{
    return drv_lis3mdltr_read_reg(bus, LIS3MDLTR_WHO_AM_I, id);
}

/**
 * @brief 读取磁力计数据
 * 
 * @param bus 软件I2C总线句柄
 * @param mag_data 磁力计数据结构指针
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_read_mag(struct _soft_i2c_bus *bus, lis3mdltr_mag_data_t *mag_data)
{
    uint8_t buf[6];
    int ret;
    
    if (!bus || !mag_data)
    {
        return -1;
    }
    
    /* 从OUT_X_L开始连续读取6个字节 */
    ret = drv_lis3mdltr_read_regs(bus, LIS3MDLTR_OUT_X_L, buf, 6);
    if (ret != 0)
    {
        return ret;
    }
    
    /* 组合高低字节 */
    mag_data->x = (int16_t)((buf[1] << 8) | buf[0]);
    mag_data->y = (int16_t)((buf[3] << 8) | buf[2]);
    mag_data->z = (int16_t)((buf[5] << 8) | buf[4]);
    
    return 0;
}

/**
 * @brief 读取温度数据
 * 
 * @param bus 软件I2C总线句柄
 * @param temp_data 温度数据结构指针
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_read_temp(struct _soft_i2c_bus *bus, lis3mdltr_temp_data_t *temp_data)
{
    uint8_t buf[2];
    int ret;
    
    if (!bus || !temp_data)
    {
        return -1;
    }
    
    /* 从TEMP_OUT_L开始连续读取2个字节 */
    ret = drv_lis3mdltr_read_regs(bus, LIS3MDLTR_TEMP_OUT_L, buf, 2);
    if (ret != 0)
    {
        return ret;
    }
    
    /* 组合高低字节 */
    temp_data->temp_raw = (int16_t)((buf[1] << 8) | buf[0]);
    
    return 0;
}

/**
 * @brief 设置工作模式
 * 
 * @param bus 软件I2C总线句柄
 * @param mode 工作模式
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_set_mode(struct _soft_i2c_bus *bus, uint8_t mode)
{
    uint8_t value;
    int ret;
    
    if (!bus)
    {
        return -1;
    }
    
    /* 读取CTRL_REG3当前值 */
    ret = drv_lis3mdltr_read_reg(bus, LIS3MDLTR_CTRL_REG3, &value);
    if (ret != 0)
    {
        return ret;
    }
    
    /* 清除模式位并设置新模式 */
    value &= ~0x03;
    value |= (mode & 0x03);
    
    /* 写回寄存器 */
    return drv_lis3mdltr_write_reg(bus, LIS3MDLTR_CTRL_REG3, value);
}

/**
 * @brief 设置输出数据率
 * 
 * @param bus 软件I2C总线句柄
 * @param odr 输出数据率
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_set_odr(struct _soft_i2c_bus *bus, uint8_t odr)
{
    uint8_t value;
    int ret;
    
    if (!bus)
    {
        return -1;
    }
    
    /* 读取CTRL_REG1当前值 */
    ret = drv_lis3mdltr_read_reg(bus, LIS3MDLTR_CTRL_REG1, &value);
    if (ret != 0)
    {
        return ret;
    }
    
    /* 清除ODR位并设置新值 */
    value &= ~0x1C;
    value |= (odr & 0x1C);
    
    /* 写回寄存器 */
    return drv_lis3mdltr_write_reg(bus, LIS3MDLTR_CTRL_REG1, value);
}

/**
 * @brief 设置量程
 * 
 * @param bus 软件I2C总线句柄
 * @param fs 量程
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_set_fullscale(struct _soft_i2c_bus *bus, uint8_t fs)
{
    uint8_t value;
    int ret;
    
    if (!bus)
    {
        return -1;
    }
    
    /* 读取CTRL_REG2当前值 */
    ret = drv_lis3mdltr_read_reg(bus, LIS3MDLTR_CTRL_REG2, &value);
    if (ret != 0)
    {
        return ret;
    }
    
    /* 清除FS位并设置新值 */
    value &= ~0x60;
    value |= (fs & 0x60);
    
    /* 写回寄存器 */
    return drv_lis3mdltr_write_reg(bus, LIS3MDLTR_CTRL_REG2, value);
}

/**
 * @brief 初始化LIS3MDLTR设备
 * 
 * @param bus 软件I2C总线句柄
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_init(struct _soft_i2c_bus *bus)
{
    uint8_t id = 0;
    int ret;
    
    if (!bus)
    {
        return -1;
    }
    
    /* 读取并验证设备ID */
    ret = drv_lis3mdltr_read_id(bus, &id);
    if (ret != 0)
    {
        rt_kprintf("LIS3MDLTR: Failed to read device ID\n");
        return -1;
    }
    
    if (id != LIS3MDLTR_ID)
    {
        rt_kprintf("LIS3MDLTR: Invalid device ID: 0x%02X (expected 0x%02X)\n", id, LIS3MDLTR_ID);
        return -1;
    }
    
    rt_kprintf("LIS3MDLTR: Device ID verified: 0x%02X\n", id);
    
    /* 配置CTRL_REG1: 使能温度传感器，设置输出数据率为10Hz，高性能模式 */
    ret = drv_lis3mdltr_write_reg(bus, LIS3MDLTR_CTRL_REG1, 0x90 | LIS3MDLTR_ODR_10HZ);
    if (ret != 0)
    {
        rt_kprintf("LIS3MDLTR: Failed to configure CTRL_REG1\n");
        return -1;
    }
    
    /* 配置CTRL_REG2: 设置量程为±4 Gauss */
    ret = drv_lis3mdltr_write_reg(bus, LIS3MDLTR_CTRL_REG2, LIS3MDLTR_FS_4GAUSS);
    if (ret != 0)
    {
        rt_kprintf("LIS3MDLTR: Failed to configure CTRL_REG2\n");
        return -1;
    }
    
    /* 配置CTRL_REG3: 设置为连续测量模式 */
    ret = drv_lis3mdltr_write_reg(bus, LIS3MDLTR_CTRL_REG3, LIS3MDLTR_MODE_CONTINUOUS);
    if (ret != 0)
    {
        rt_kprintf("LIS3MDLTR: Failed to configure CTRL_REG3\n");
        return -1;
    }
    
    /* 配置CTRL_REG4: Z轴高性能模式 */
    ret = drv_lis3mdltr_write_reg(bus, LIS3MDLTR_CTRL_REG4, 0x08);
    if (ret != 0)
    {
        rt_kprintf("LIS3MDLTR: Failed to configure CTRL_REG4\n");
        return -1;
    }
    
    /* 配置CTRL_REG5: 连续更新模式（BDU=0） */
    ret = drv_lis3mdltr_write_reg(bus, LIS3MDLTR_CTRL_REG5, 0x00);
    if (ret != 0)
    {
        rt_kprintf("LIS3MDLTR: Failed to configure CTRL_REG5\n");
        return -1;
    }
    
    rt_kprintf("LIS3MDLTR: Initialization successful\n");
    
    return 0;
}

