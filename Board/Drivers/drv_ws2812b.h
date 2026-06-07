//
// Created by An on 2026/06/07.
//

#ifndef Y_TRACE_DRV_WS2812B_H
#define Y_TRACE_DRV_WS2812B_H

#include "drv_pin.h"
#include <rtthread.h>
#include <stdint.h>

#define WS2812B_PIN GET_PIN(A, 15)
#define WS2812B_MAX_PIXELS 8U

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} ws2812b_rgb_t;

int drv_ws2812b_init(void);
int drv_ws2812b_write_rgb(const ws2812b_rgb_t *pixels, rt_size_t count);
int drv_ws2812b_set_rgb(uint8_t red, uint8_t green, uint8_t blue);
void drv_ws2812b_clear(void);

#endif // Y_TRACE_DRV_WS2812B_H
