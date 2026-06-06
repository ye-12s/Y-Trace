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

typedef void (*drv_lcd_write_done_cb_t)(void *user_data, int result);

int drv_lcd_init(void);
size_t drv_lcd_get_width(void);
size_t drv_lcd_get_height(void);
int drv_lcd_set_backlight(int level);
int drv_lcd_clear(void);
int drv_lcd_refresh(void);
int drv_lcd_point(size_t x, size_t y, uint16_t color);
int drv_lcd_full(size_t xStart, size_t yStart, size_t xEnd, size_t yEnd, uint16_t color);
int drv_lcd_set_window(size_t xStart, size_t yStart, size_t xEnd, size_t yEnd);
int drv_lcd_write_bytes(const uint8_t *data, size_t len);
int drv_lcd_write_bytes_async(const uint8_t *data, size_t len, drv_lcd_write_done_cb_t done_cb, void *user_data);
int drv_lcd_write_pixels(const uint16_t *pixels, size_t count);

#ifdef __cplusplus
}
#endif

#endif // Y_TRACK_DRV_LCD_H
