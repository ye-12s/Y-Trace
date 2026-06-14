#include "sample/sample.h"

#include <stdint.h>

#include "Drivers/drv_lcd.h"
#include "rtthread.h"

#define LCD_REFRESH_WIDTH       240U
#define LCD_REFRESH_HEIGHT      320U
#define LCD_REFRESH_BUF_BYTES   (112U * 1024U)
#define LCD_REFRESH_EXT_LINES   (LCD_REFRESH_BUF_BYTES / LCD_REFRESH_WIDTH / 2U)
#define LCD_REFRESH_TAIL_LINES  (LCD_REFRESH_HEIGHT - LCD_REFRESH_EXT_LINES)
#define LCD_REFRESH_LOCAL_LINES LCD_REFRESH_TAIL_LINES
#define LCD_REFRESH_STACK_SIZE  2048
#define LCD_REFRESH_PRIORITY    9
#define LCD_REFRESH_TIMESLICE   10
#define LCD_GRADIENT_STOP_COUNT 5U

static uint16_t lcd_refresh_local_buf[LCD_REFRESH_WIDTH * LCD_REFRESH_LOCAL_LINES];
static uint16_t lcd_refresh_palette[256];
static uint8_t lcd_refresh_x_grad[LCD_REFRESH_WIDTH];
static uint8_t lcd_refresh_y_grad[LCD_REFRESH_HEIGHT];
volatile uint32_t lcd_refresh_frame_count;
volatile uint32_t lcd_refresh_last_fps_x10;
volatile uint32_t lcd_refresh_last_frame_ms;
volatile uint32_t lcd_refresh_active_buf_lines;

typedef struct {
    uint8_t pos;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} lcd_gradient_stop_t;

static const lcd_gradient_stop_t lcd_gradient_stops[LCD_GRADIENT_STOP_COUNT] = {
    {0, 20, 34, 105},
    {58, 0, 205, 255},
    {118, 70, 255, 150},
    {185, 255, 190, 65},
    {255, 245, 65, 190},
};

static uint16_t lcd_rgb565_bus(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t color = (uint16_t)(((uint16_t)(r & 0xF8U) << 8) | ((uint16_t)(g & 0xFCU) << 3) | ((uint16_t)b >> 3));
    return (uint16_t)((color >> 8) | (color << 8));
}

static void lcd_refresh_palette_init(void)
{
    for (uint32_t x = 0; x < LCD_REFRESH_WIDTH; x++) {
        lcd_refresh_x_grad[x] = (uint8_t)((x * 156U) / (LCD_REFRESH_WIDTH - 1U));
    }

    for (uint32_t y = 0; y < LCD_REFRESH_HEIGHT; y++) {
        lcd_refresh_y_grad[y] = (uint8_t)((y * 100U) / (LCD_REFRESH_HEIGHT - 1U));
    }

    for (uint32_t i = 0; i < 256U; i++) {
        const lcd_gradient_stop_t *a = &lcd_gradient_stops[0];
        const lcd_gradient_stop_t *b = &lcd_gradient_stops[LCD_GRADIENT_STOP_COUNT - 1U];

        for (uint32_t s = 0; s < LCD_GRADIENT_STOP_COUNT - 1U; s++) {
            if (i >= lcd_gradient_stops[s].pos && i <= lcd_gradient_stops[s + 1U].pos) {
                a = &lcd_gradient_stops[s];
                b = &lcd_gradient_stops[s + 1U];
                break;
            }
        }

        uint32_t span = (uint32_t)b->pos - (uint32_t)a->pos;
        uint32_t t    = span == 0U ? 0U : ((i - (uint32_t)a->pos) * 255U) / span;
        uint8_t r     = (uint8_t)(((uint32_t)a->r * (255U - t) + (uint32_t)b->r * t) / 255U);
        uint8_t g     = (uint8_t)(((uint32_t)a->g * (255U - t) + (uint32_t)b->g * t) / 255U);
        uint8_t bl    = (uint8_t)(((uint32_t)a->b * (255U - t) + (uint32_t)b->b * t) / 255U);

        lcd_refresh_palette[i] = lcd_rgb565_bus(r, g, bl);
    }
}

static void lcd_refresh_fill_dynamic(uint16_t *buf, size_t y0, size_t lines, uint32_t frame)
{
    uint8_t frame_phase = (uint8_t)(frame * 3U);

    for (size_t y = 0; y < lines; y++) {
        size_t abs_y  = y0 + y;
        uint8_t phase = (uint8_t)(frame_phase + lcd_refresh_y_grad[abs_y]);
        for (size_t x = 0; x < LCD_REFRESH_WIDTH; x++) {
            uint8_t idx                    = (uint8_t)(phase + lcd_refresh_x_grad[x]);
            buf[y * LCD_REFRESH_WIDTH + x] = lcd_refresh_palette[idx];
        }
    }
}

static void lcd_refresh_flush_chunk(uint16_t *buf, size_t lines)
{
    (void)drv_lcd_write_bytes((const uint8_t *)buf, LCD_REFRESH_WIDTH * lines * 2U);
}

static void lcd_refresh_begin_fullscreen(void)
{
    (void)drv_lcd_set_window(0, 0, LCD_REFRESH_WIDTH - 1U, LCD_REFRESH_HEIGHT - 1U);
}

static void lcd_refresh_thread_entry(void *parameter)
{
    (void)parameter;

    uint16_t *draw_buf = (uint16_t *)rt_malloc(LCD_REFRESH_BUF_BYTES);
    size_t buf_lines   = LCD_REFRESH_EXT_LINES;
    rt_bool_t use_heap = RT_TRUE;

    if (draw_buf == RT_NULL) {
        draw_buf  = lcd_refresh_local_buf;
        buf_lines = LCD_REFRESH_LOCAL_LINES;
        use_heap  = RT_FALSE;
    }

    rt_kprintf("lcd_refresh: raw SPI DMA test, buffer=%u lines at 0x%08x\n",
               (unsigned int)buf_lines,
               (unsigned int)(uintptr_t)draw_buf);
    lcd_refresh_active_buf_lines = (uint32_t)buf_lines;
    lcd_refresh_palette_init();

    uint32_t frame      = 0;
    uint32_t last_tick  = rt_tick_get();
    uint32_t last_frame = 0;

    while (1) {
        uint32_t frame_start = rt_tick_get();

        if (use_heap) {
            lcd_refresh_fill_dynamic(draw_buf, 0, LCD_REFRESH_EXT_LINES, frame);
            lcd_refresh_fill_dynamic(lcd_refresh_local_buf, LCD_REFRESH_EXT_LINES, LCD_REFRESH_TAIL_LINES, frame);
            lcd_refresh_begin_fullscreen();
            lcd_refresh_flush_chunk(draw_buf, LCD_REFRESH_EXT_LINES);
            lcd_refresh_flush_chunk(lcd_refresh_local_buf, LCD_REFRESH_TAIL_LINES);
        } else {
            lcd_refresh_begin_fullscreen();
            for (size_t y = 0; y < LCD_REFRESH_HEIGHT; y += buf_lines) {
                size_t lines = LCD_REFRESH_HEIGHT - y;
                if (lines > buf_lines) {
                    lines = buf_lines;
                }

                lcd_refresh_fill_dynamic(draw_buf, y, lines, frame);
                lcd_refresh_flush_chunk(draw_buf, lines);
            }
        }

        frame++;
        lcd_refresh_frame_count   = frame;
        lcd_refresh_last_frame_ms = rt_tick_get() - frame_start;

        uint32_t now = rt_tick_get();
        if ((now - last_tick) >= 1000U) {
            uint32_t frames          = frame - last_frame;
            uint32_t elapsed         = now - last_tick;
            lcd_refresh_last_fps_x10 = frames * 10000U / elapsed;
            rt_kprintf("lcd_refresh: fps=%lu.%lu frame=%lu buf_lines=%u\n",
                       (unsigned long)(frames * 1000U / elapsed),
                       (unsigned long)((frames * 10000U / elapsed) % 10U),
                       (unsigned long)frame,
                       (unsigned int)buf_lines);
            last_tick  = now;
            last_frame = frame;
        }
    }
}

int lcd_refresh_sample_init(void)
{
    rt_thread_t thread = rt_thread_create("lcdraw", lcd_refresh_thread_entry, RT_NULL, LCD_REFRESH_STACK_SIZE, LCD_REFRESH_PRIORITY, LCD_REFRESH_TIMESLICE);
    if (thread == RT_NULL) {
        return -RT_ENOMEM;
    }

    return rt_thread_startup(thread);
}
