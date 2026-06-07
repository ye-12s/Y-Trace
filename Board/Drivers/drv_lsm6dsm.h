#ifndef __DRV_LSM6DSM_H__
#define __DRV_LSM6DSM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "drv_soft_iic.h"

/* LSM6DSM I2C 地址 */
#define LSM6DSM_I2C_ADDR_LOW    0x6A  // SDO/SA0 连接到GND时
#define LSM6DSM_I2C_ADDR_HIGH   0x6B  // SDO/SA0 连接到VDD时
#define LSM6DSM_I2C_ADDR        LSM6DSM_I2C_ADDR_LOW  // 默认使用低地址

/* LSM6DSM 寄存器地址 */
#define LSM6DSM_FUNC_CFG_ACCESS     0x01  // 功能配置访问
#define LSM6DSM_FIFO_CTRL1          0x06  // FIFO控制寄存器1
#define LSM6DSM_FIFO_CTRL2          0x07  // FIFO控制寄存器2
#define LSM6DSM_FIFO_CTRL3          0x08  // FIFO控制寄存器3
#define LSM6DSM_FIFO_CTRL4          0x09  // FIFO控制寄存器4
#define LSM6DSM_FIFO_CTRL5          0x0A  // FIFO控制寄存器5
#define LSM6DSM_INT1_CTRL           0x0D  // INT1引脚控制
#define LSM6DSM_INT2_CTRL           0x0E  // INT2引脚控制
#define LSM6DSM_WHO_AM_I            0x0F  // 设备ID寄存器
#define LSM6DSM_CTRL1_XL            0x10  // 加速度计控制寄存器1
#define LSM6DSM_CTRL2_G             0x11  // 陀螺仪控制寄存器2
#define LSM6DSM_CTRL3_C             0x12  // 控制寄存器3
#define LSM6DSM_CTRL4_C             0x13  // 控制寄存器4
#define LSM6DSM_CTRL5_C             0x14  // 控制寄存器5
#define LSM6DSM_CTRL6_C             0x15  // 控制寄存器6
#define LSM6DSM_CTRL7_G             0x16  // 陀螺仪控制寄存器7
#define LSM6DSM_CTRL8_XL            0x17  // 加速度计控制寄存器8
#define LSM6DSM_CTRL9_XL            0x18  // 加速度计控制寄存器9
#define LSM6DSM_CTRL10_C            0x19  // 控制寄存器10
#define LSM6DSM_STATUS_REG          0x1E  // 状态寄存器
#define LSM6DSM_OUT_TEMP_L          0x20  // 温度输出低字节
#define LSM6DSM_OUT_TEMP_H          0x21  // 温度输出高字节
#define LSM6DSM_OUTX_L_G            0x22  // 陀螺仪X轴低字节
#define LSM6DSM_OUTX_H_G            0x23  // 陀螺仪X轴高字节
#define LSM6DSM_OUTY_L_G            0x24  // 陀螺仪Y轴低字节
#define LSM6DSM_OUTY_H_G            0x25  // 陀螺仪Y轴高字节
#define LSM6DSM_OUTZ_L_G            0x26  // 陀螺仪Z轴低字节
#define LSM6DSM_OUTZ_H_G            0x27  // 陀螺仪Z轴高字节
#define LSM6DSM_OUTX_L_XL           0x28  // 加速度计X轴低字节
#define LSM6DSM_OUTX_H_XL           0x29  // 加速度计X轴高字节
#define LSM6DSM_OUTY_L_XL           0x2A  // 加速度计Y轴低字节
#define LSM6DSM_OUTY_H_XL           0x2B  // 加速度计Y轴高字节
#define LSM6DSM_OUTZ_L_XL           0x2C  // 加速度计Z轴低字节
#define LSM6DSM_OUTZ_H_XL           0x2D  // 加速度计Z轴高字节

/* 设备ID值 */
#define LSM6DSM_ID                  0x6A

/* 加速度计输出数据率 (CTRL1_XL) */
#define LSM6DSM_XL_ODR_POWER_DOWN   0x00
#define LSM6DSM_XL_ODR_12_5HZ       0x10
#define LSM6DSM_XL_ODR_26HZ         0x20
#define LSM6DSM_XL_ODR_52HZ         0x30
#define LSM6DSM_XL_ODR_104HZ        0x40
#define LSM6DSM_XL_ODR_208HZ        0x50
#define LSM6DSM_XL_ODR_416HZ        0x60
#define LSM6DSM_XL_ODR_833HZ        0x70
#define LSM6DSM_XL_ODR_1666HZ       0x80
#define LSM6DSM_XL_ODR_3333HZ       0x90
#define LSM6DSM_XL_ODR_6666HZ       0xA0

/* 加速度计量程 (CTRL1_XL) */
#define LSM6DSM_XL_FS_2G            0x00
#define LSM6DSM_XL_FS_4G            0x08
#define LSM6DSM_XL_FS_8G            0x0C
#define LSM6DSM_XL_FS_16G           0x04

/* 陀螺仪输出数据率 (CTRL2_G) */
#define LSM6DSM_G_ODR_POWER_DOWN    0x00
#define LSM6DSM_G_ODR_12_5HZ        0x10
#define LSM6DSM_G_ODR_26HZ          0x20
#define LSM6DSM_G_ODR_52HZ          0x30
#define LSM6DSM_G_ODR_104HZ         0x40
#define LSM6DSM_G_ODR_208HZ         0x50
#define LSM6DSM_G_ODR_416HZ         0x60
#define LSM6DSM_G_ODR_833HZ         0x70
#define LSM6DSM_G_ODR_1666HZ        0x80

/* 陀螺仪量程 (CTRL2_G) */
#define LSM6DSM_G_FS_250DPS         0x00
#define LSM6DSM_G_FS_500DPS         0x04
#define LSM6DSM_G_FS_1000DPS        0x08
#define LSM6DSM_G_FS_2000DPS        0x0C

/* 加速度计数据结构 */
typedef struct
{
    int16_t x;  // X轴加速度
    int16_t y;  // Y轴加速度
    int16_t z;  // Z轴加速度
} lsm6dsm_accel_data_t;

/* 陀螺仪数据结构 */
typedef struct
{
    int16_t x;  // X轴角速度
    int16_t y;  // Y轴角速度
    int16_t z;  // Z轴角速度
} lsm6dsm_gyro_data_t;

/* 温度数据结构 */
typedef struct
{
    int16_t temp_raw;  // 温度原始值
} lsm6dsm_temp_data_t;

/**
 * @brief 初始化LSM6DSM设备
 * 
 * @param bus 软件I2C总线句柄
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_init(struct _soft_i2c_bus *bus);
int drv_lsm6dsm_probe(struct _soft_i2c_bus *bus, uint8_t *addr, uint8_t *id);
int drv_lsm6dsm_init_addr(struct _soft_i2c_bus *bus, uint8_t addr);

/**
 * @brief 写LSM6DSM寄存器
 * 
 * @param bus 软件I2C总线句柄
 * @param reg 寄存器地址
 * @param data 写入的数据
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_write_reg(struct _soft_i2c_bus *bus, uint8_t reg, uint8_t data);
int drv_lsm6dsm_write_reg_addr(struct _soft_i2c_bus *bus, uint8_t addr, uint8_t reg, uint8_t data);

/**
 * @brief 读LSM6DSM寄存器
 * 
 * @param bus 软件I2C总线句柄
 * @param reg 寄存器地址
 * @param data 读取的数据缓冲区
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_read_reg(struct _soft_i2c_bus *bus, uint8_t reg, uint8_t *data);
int drv_lsm6dsm_read_reg_addr(struct _soft_i2c_bus *bus, uint8_t addr, uint8_t reg, uint8_t *data);

/**
 * @brief 读取LSM6DSM多个寄存器
 * 
 * @param bus 软件I2C总线句柄
 * @param reg 起始寄存器地址
 * @param data 读取的数据缓冲区
 * @param len 读取的数据长度
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_read_regs(struct _soft_i2c_bus *bus, uint8_t reg, uint8_t *data, uint8_t len);
int drv_lsm6dsm_read_regs_addr(struct _soft_i2c_bus *bus, uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len);

/**
 * @brief 读取设备ID
 * 
 * @param bus 软件I2C总线句柄
 * @param id 设备ID缓冲区
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_read_id(struct _soft_i2c_bus *bus, uint8_t *id);
int drv_lsm6dsm_read_id_addr(struct _soft_i2c_bus *bus, uint8_t addr, uint8_t *id);

/**
 * @brief 读取加速度计数据
 * 
 * @param bus 软件I2C总线句柄
 * @param accel_data 加速度计数据结构指针
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_read_accel(struct _soft_i2c_bus *bus, lsm6dsm_accel_data_t *accel_data);
int drv_lsm6dsm_read_accel_addr(struct _soft_i2c_bus *bus, uint8_t addr, lsm6dsm_accel_data_t *accel_data);

/**
 * @brief 读取陀螺仪数据
 * 
 * @param bus 软件I2C总线句柄
 * @param gyro_data 陀螺仪数据结构指针
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_read_gyro(struct _soft_i2c_bus *bus, lsm6dsm_gyro_data_t *gyro_data);
int drv_lsm6dsm_read_gyro_addr(struct _soft_i2c_bus *bus, uint8_t addr, lsm6dsm_gyro_data_t *gyro_data);

/**
 * @brief 读取温度数据
 * 
 * @param bus 软件I2C总线句柄
 * @param temp_data 温度数据结构指针
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_read_temp(struct _soft_i2c_bus *bus, lsm6dsm_temp_data_t *temp_data);
int drv_lsm6dsm_read_temp_addr(struct _soft_i2c_bus *bus, uint8_t addr, lsm6dsm_temp_data_t *temp_data);

/**
 * @brief 设置加速度计输出数据率
 * 
 * @param bus 软件I2C总线句柄
 * @param odr 输出数据率
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_set_accel_odr(struct _soft_i2c_bus *bus, uint8_t odr);

/**
 * @brief 设置加速度计量程
 * 
 * @param bus 软件I2C总线句柄
 * @param fs 量程
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_set_accel_fullscale(struct _soft_i2c_bus *bus, uint8_t fs);

/**
 * @brief 设置陀螺仪输出数据率
 * 
 * @param bus 软件I2C总线句柄
 * @param odr 输出数据率
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_set_gyro_odr(struct _soft_i2c_bus *bus, uint8_t odr);

/**
 * @brief 设置陀螺仪量程
 * 
 * @param bus 软件I2C总线句柄
 * @param fs 量程
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_set_gyro_fullscale(struct _soft_i2c_bus *bus, uint8_t fs);

/**
 * @brief 诊断LSM6DSM配置（用于调试）
 * 
 * @param bus 软件I2C总线句柄
 * @return int 0:成功, <0:失败
 */
int drv_lsm6dsm_diagnose(struct _soft_i2c_bus *bus);

#ifdef __cplusplus
}
#endif

#endif