#ifndef __DRV_LIS3MDLTR_H__
#define __DRV_LIS3MDLTR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "drv_soft_iic.h"

/* LIS3MDLTR I2C 地址 */
#define LIS3MDLTR_I2C_ADDR      0x1C

/* LIS3MDLTR 寄存器地址 */
#define LIS3MDLTR_WHO_AM_I      0x0F  // 设备ID寄存器
#define LIS3MDLTR_CTRL_REG1     0x20  // 控制寄存器1
#define LIS3MDLTR_CTRL_REG2     0x21  // 控制寄存器2
#define LIS3MDLTR_CTRL_REG3     0x22  // 控制寄存器3
#define LIS3MDLTR_CTRL_REG4     0x23  // 控制寄存器4
#define LIS3MDLTR_CTRL_REG5     0x24  // 控制寄存器5
#define LIS3MDLTR_STATUS_REG    0x27  // 状态寄存器
#define LIS3MDLTR_OUT_X_L       0x28  // X轴低字节
#define LIS3MDLTR_OUT_X_H       0x29  // X轴高字节
#define LIS3MDLTR_OUT_Y_L       0x2A  // Y轴低字节
#define LIS3MDLTR_OUT_Y_H       0x2B  // Y轴高字节
#define LIS3MDLTR_OUT_Z_L       0x2C  // Z轴低字节
#define LIS3MDLTR_OUT_Z_H       0x2D  // Z轴高字节
#define LIS3MDLTR_TEMP_OUT_L    0x2E  // 温度低字节
#define LIS3MDLTR_TEMP_OUT_H    0x2F  // 温度高字节

/* 设备ID值 */
#define LIS3MDLTR_ID            0x3D

/* 工作模式 */
#define LIS3MDLTR_MODE_CONTINUOUS   0x00  // 连续测量模式
#define LIS3MDLTR_MODE_SINGLE       0x01  // 单次测量模式
#define LIS3MDLTR_MODE_POWERDOWN    0x03  // 省电模式

/* 输出数据率 */
#define LIS3MDLTR_ODR_0_625HZ   0x00
#define LIS3MDLTR_ODR_1_25HZ    0x04
#define LIS3MDLTR_ODR_2_5HZ     0x08
#define LIS3MDLTR_ODR_5HZ       0x0C
#define LIS3MDLTR_ODR_10HZ      0x10
#define LIS3MDLTR_ODR_20HZ      0x14
#define LIS3MDLTR_ODR_40HZ      0x18
#define LIS3MDLTR_ODR_80HZ      0x1C

/* 量程 */
#define LIS3MDLTR_FS_4GAUSS     0x00
#define LIS3MDLTR_FS_8GAUSS     0x20
#define LIS3MDLTR_FS_12GAUSS    0x40
#define LIS3MDLTR_FS_16GAUSS    0x60

/* 磁力计数据结构 */
typedef struct
{
    int16_t x;  // X轴磁场强度
    int16_t y;  // Y轴磁场强度
    int16_t z;  // Z轴磁场强度
} lis3mdltr_mag_data_t;

/* 温度数据结构 */
typedef struct
{
    int16_t temp_raw;  // 温度原始值
} lis3mdltr_temp_data_t;

/**
 * @brief 初始化LIS3MDLTR设备
 * 
 * @param bus 软件I2C总线句柄
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_init(struct _soft_i2c_bus *bus);

/**
 * @brief 写LIS3MDLTR寄存器
 * 
 * @param bus 软件I2C总线句柄
 * @param reg 寄存器地址
 * @param data 写入的数据
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_write_reg(struct _soft_i2c_bus *bus, uint8_t reg, uint8_t data);

/**
 * @brief 读LIS3MDLTR寄存器
 * 
 * @param bus 软件I2C总线句柄
 * @param reg 寄存器地址
 * @param data 读取的数据缓冲区
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_read_reg(struct _soft_i2c_bus *bus, uint8_t reg, uint8_t *data);

/**
 * @brief 读取LIS3MDLTR多个寄存器
 * 
 * @param bus 软件I2C总线句柄
 * @param reg 起始寄存器地址
 * @param data 读取的数据缓冲区
 * @param len 读取的数据长度
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_read_regs(struct _soft_i2c_bus *bus, uint8_t reg, uint8_t *data, uint8_t len);

/**
 * @brief 读取设备ID
 * 
 * @param bus 软件I2C总线句柄
 * @param id 设备ID缓冲区
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_read_id(struct _soft_i2c_bus *bus, uint8_t *id);

/**
 * @brief 读取磁力计数据
 * 
 * @param bus 软件I2C总线句柄
 * @param mag_data 磁力计数据结构指针
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_read_mag(struct _soft_i2c_bus *bus, lis3mdltr_mag_data_t *mag_data);

/**
 * @brief 读取温度数据
 * 
 * @param bus 软件I2C总线句柄
 * @param temp_data 温度数据结构指针
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_read_temp(struct _soft_i2c_bus *bus, lis3mdltr_temp_data_t *temp_data);

/**
 * @brief 设置工作模式
 * 
 * @param bus 软件I2C总线句柄
 * @param mode 工作模式
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_set_mode(struct _soft_i2c_bus *bus, uint8_t mode);

/**
 * @brief 设置输出数据率
 * 
 * @param bus 软件I2C总线句柄
 * @param odr 输出数据率
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_set_odr(struct _soft_i2c_bus *bus, uint8_t odr);

/**
 * @brief 设置量程
 * 
 * @param bus 软件I2C总线句柄
 * @param fs 量程
 * @return int 0:成功, <0:失败
 */
int drv_lis3mdltr_set_fullscale(struct _soft_i2c_bus *bus, uint8_t fs);

#ifdef __cplusplus
}
#endif

#endif