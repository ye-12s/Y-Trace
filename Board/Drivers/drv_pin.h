//
// Created by An on 2025/12/14.
//

#ifndef Y_TRACK_DRV_PIN_H
#define Y_TRACK_DRV_PIN_H

#include "stdint.h"
#include "at32f403a_407.h"

typedef int32_t pin_t;

#define __AT32_PORT(port) GPIO##port##_BASE

#define GET_PIN(PORTx, PIN) \
((pin_t)((16 * (((uint32_t)__AT32_PORT(PORTx) - (uint32_t)GPIOA_BASE) / (0x0400UL))) + PIN))

#define GET_PORT(PIN) \
((gpio_type *)((((uint32_t)GPIOA_BASE) + (((PIN) >> 4) * 0x0400UL))))

#define GET_PIN_INDEX(PIN) (((PIN) & 0x0F))

typedef enum {
    PIN_MODE_OPP,    // Output Push Pull
    PIN_MODE_OOD,    // Output Open Drain
    PIN_MODE_INPUT,  // Input
    PIN_MODE_ANALOG, // Analog
    PIN_MODE_AF,     // Alternate Function
    PIN_MODE_AF_OD,  // Alternate Function Open Drain
} pin_mode_t;

typedef enum {
    PIN_PULL_NONE, // No pull-up or pull-down
    PIN_PULL_UP,   // Pull-up resistor enabled
    PIN_PULL_DOWN, // Pull-down resistor enabled
} pin_pull_t;

/**
 * @brief 初始化 GPIO引脚
 *
 * @param pin 引脚号
 * @param mode 引脚模式
 * @param pull 上拉/下拉配置
 * @return 0 成功，< 0 失败
 */
int pin_init(pin_t     pin,
            pin_mode_t mode,
            pin_pull_t pull);

/**
 * @brief 初始化 GPIO 引脚为复用模式
 *
 * @param pin 引脚号
 * @param pull 上拉/下拉配置
 * @param mode 引脚模式    (IO_MODE_AF 或 IO_MODE_AF_OD)
 * @param af_mode 复用模式 [0:15]
 * @return 0 成功，< 0 失败
 */
int pin_af_init(pin_t     pin,
               pin_pull_t pull,
               pin_mode_t mode,
               int8_t    af_mode);

/**
 * @brief 反初始化 GPIO 引脚
 *
 * @param pin 引脚号
 */
void pin_deinit(pin_t pin);
/**
 * @brief 写入 GPIO 引脚
 *
 * @param pin 引脚号
 * @param val 写入值 (0 或 1)
 * @return int 成功返回 0，失败返回 < 0
 */
int pin_write(pin_t   pin,
             uint8_t val);
/**
 * @brief 读取 GPIO 引脚
 *
 * @param pin 引脚号
 * @return int8_t 读取值 (0 或 1)，失败返回 < 0
 */
int8_t pin_read(pin_t pin);
/**
 * @brief 读取 GPIO 引脚输出状态
 *
 * @param pin 引脚号
 * @return int8_t 输出状态 (0 或 1)，失败返回 < 0
 */
int8_t pin_read_output(pin_t pin);
/**
 * @brief 切换 GPIO 引脚状态
 *
 * @param pin 引脚号
 * @return int8_t 切换后的状态 (0 或 1)，失败返回 < 0
 */
int8_t pin_toggle(pin_t pin);

#endif //Y_TRACK_DRV_PIN_H