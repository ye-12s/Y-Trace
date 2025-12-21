//
// Created by An on 2025/12/14.
//

#ifndef Y_TRACK_DRV_LCD_H
#define Y_TRACK_DRV_LCD_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int drv_lcd_init(void);
int drv_lcd_set_backlight(int level);
int drv_lcd_clear(void);
int drv_lcd_refresh(void);
int drv_lcd_point(size_t x, size_t y, uint16_t color);
int drv_lcd_full(size_t xStart, size_t yStart, size_t xEnd, size_t yEnd, uint16_t color);




#ifdef __cplusplus
}
#endif

#endif //Y_TRACK_DRV_LCD_H