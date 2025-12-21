#ifndef __DRV_SOFT_IIC_H__
#define __DRV_SOFT_IIC_H__

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <stddef.h>
#include "drv_pin.h"
#include "rtthread.h"

struct _soft_i2c_bus
{
	pin_t    scl;     // SCL pin
	pin_t    sda;     // SDA pin
	int32_t  delay;   // Delay in microseconds
	uint32_t timeout; // Timeout in milliseconds
    rt_mutex_t lock;  // Mutex for thread safety
};
struct i2c_msg
{
	uint16_t addr;
	uint16_t flags;
	uint16_t len;
	uint8_t *buf;
};

/**
 * @brief I2C传输标志位定义
 *
 */
#define I2C_FLAG_WR          (0x0000u) // 写
#define I2C_FLAG_RD          (1u << 0) // 读
#define I2C_FLAG_ADDR_10BIT  (1u << 2) // 10位地址
#define I2C_FLAG_NO_START    (1u << 4) // 不发送起始信号
#define I2C_FLAG_IGNORE_NACK (1u << 5) // 忽略NACK
#define I2C_FLAG_NO_READ_ACK (1u << 6) // 不发送ACK
#define I2C_FLAG_NO_STOP     (1u << 7) // 不发送停止信号

/**
 * @brief 初始化软件I2C总线
 *
 * @param bus I2C总线句柄
 * @param scl SCL引脚
 * @param sda SDA引脚
 * @param speed I2C总线速度，Unit:KHz
 * @param timeout 超时时间，Unit:ms
 * @return int
 */
int drv_soft_i2c_init( struct _soft_i2c_bus *bus, pin_t scl, pin_t sda, int32_t speed, uint32_t timeout );
int drv_soft_i2c_deinit( struct _soft_i2c_bus *bus );

/**
 * @brief 执行I2C传输
 *
 * @param bus i2c总线句柄
 * @param msgs 消息数组
 * @param num 消息数量
 * @return int
 */
int drv_soft_i2c_transfer( struct _soft_i2c_bus *bus,
                           struct i2c_msg        msgs[],
                           uint32_t              num );
/**
 * @brief 以主机发送I2C数据
 *
 * @param bus i2c总线句柄
 * @param addr 设备地址
 * @param flags 传输标志
 * @param buf 数据缓冲区
 * @param count 数据长度
 *
 * @return int
 * @retval 1  成功，
 * @retval 0  NACK，
 * @retval <0  错误
 */
int drv_soft_i2c_master_send( struct _soft_i2c_bus *bus,
                              uint16_t              addr,
                              uint16_t              flags,
                              const uint8_t        *buf,
                              size_t                count );

/**
 * @brief 以主机接收I2C数据
 *
 * @param bus i2c总线句柄
 * @param addr 设备地址
 * @param flags 传输标志
 * @param buf 数据缓冲区
 * @param count 数据长度
 * @return int
 * @retval 1  成功，
 * @retval 0  NACK，
 * @retval <0  错误
 */
int drv_soft_i2c_master_recv( struct _soft_i2c_bus *bus,
                              uint16_t              addr,
                              uint16_t              flags,
                              uint8_t              *buf,
                              size_t                count );


#ifdef __cplusplus
}
#endif

#endif