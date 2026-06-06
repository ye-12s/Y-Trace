#include "port/lvgl_port.h"

#include <stddef.h>
#include <stdint.h>

#include "Drivers/drv_lcd.h"
#include "Drivers/drv_sram.h"
#include "board.h"
#include "lvgl.h"
#include "rtthread.h"

#define LVGL_HOR_RES              240
#define LVGL_VER_RES              320
#define LVGL_DRAW_BUF_TOTAL_BYTES (112U * 1024U)
#define LVGL_DRAW_BUF_LINES       (LVGL_DRAW_BUF_TOTAL_BYTES / 2U / LVGL_HOR_RES / sizeof(lv_color_t))
#define LVGL_FALLBACK_BUF_LINES   40
#define LVGL_THREAD_STACK         4096
#define LVGL_THREAD_PRIORITY      8
#define LVGL_THREAD_TIMESLICE     10
#define LVGL_TASK_PERIOD_MS       5
#define LVGL_ANIM_PERIOD_MS       16
#define LVGL_PERF_PERIOD_MS       500
#define LVGL_STRESS_TILE_COUNT    10
#define LVGL_STRESS_BAR_COUNT     4
#define LVGL_STRESS_TILE_SIZE     24

static lv_disp_draw_buf_t lvgl_draw_buf;
static lv_color_t lvgl_fallback_buf[LVGL_HOR_RES * LVGL_FALLBACK_BUF_LINES];

typedef struct {
    uint32_t frames;
    uint32_t flushes;
    uint32_t pixels;
    uint32_t max_flush_pixels;
    uint32_t flush_ms;
} lvgl_perf_stats_t;

typedef struct {
    lv_disp_drv_t *disp_drv;
    uint32_t start_tick;
    uint32_t px_cnt;
} lvgl_flush_state_t;

static lvgl_perf_stats_t lvgl_perf_stats;
static lvgl_flush_state_t lvgl_flush_state;
static lv_obj_t *fps_label;
static lv_obj_t *flush_label;
static lv_obj_t *scene_label;
static lv_obj_t *bars[LVGL_STRESS_BAR_COUNT];
static lv_obj_t *tiles[LVGL_STRESS_TILE_COUNT];

static const uint32_t tile_colors[LVGL_STRESS_TILE_COUNT] = {
    0xFFCC4D,
    0x2D9CDB,
    0x7CFF9B,
    0xFF6B8A,
    0xA78BFA,
    0xF59E0B,
    0x22D3EE,
    0x34D399,
    0xF472B6,
    0xCBD5E1,
};

static void lvgl_draw_buffer_init(void)
{
    if (drv_sram_224k_prepare() == RT_TRUE) {
        lv_color_t *buf1 = (lv_color_t *)RAM_EXT_START;
        lv_color_t *buf2 = buf1 + (LVGL_HOR_RES * LVGL_DRAW_BUF_LINES);

        lv_disp_draw_buf_init(&lvgl_draw_buf, buf1, buf2, LVGL_HOR_RES * LVGL_DRAW_BUF_LINES);
        rt_kprintf("lvgl: dual DMA draw buffers at 0x%08x/0x%08x, %u lines each\n",
                   (unsigned int)(uintptr_t)buf1,
                   (unsigned int)(uintptr_t)buf2,
                   (unsigned int)LVGL_DRAW_BUF_LINES);
        return;
    }

    lv_disp_draw_buf_init(&lvgl_draw_buf, lvgl_fallback_buf, RT_NULL, sizeof(lvgl_fallback_buf) / sizeof(lvgl_fallback_buf[0]));
    rt_kprintf("lvgl: fallback single draw buffer, %u lines\n", (unsigned int)LVGL_FALLBACK_BUF_LINES);
}

static void lvgl_record_flush(lv_disp_drv_t *disp_drv, uint32_t px_cnt, uint32_t start_tick)
{
    uint32_t elapsed_ms = rt_tick_get() - start_tick;

    lvgl_perf_stats.flushes++;
    lvgl_perf_stats.pixels += px_cnt;
    lvgl_perf_stats.flush_ms += elapsed_ms;
    if (px_cnt > lvgl_perf_stats.max_flush_pixels) {
        lvgl_perf_stats.max_flush_pixels = px_cnt;
    }
    if (lv_disp_flush_is_last(disp_drv)) {
        lvgl_perf_stats.frames++;
    }
}

static void lvgl_flush_done_cb(void *user_data, int result)
{
    lvgl_flush_state_t *state = (lvgl_flush_state_t *)user_data;

    if (result == 0) {
        lvgl_record_flush(state->disp_drv, state->px_cnt, state->start_tick);
    }

    lv_disp_flush_ready(state->disp_drv);
}

static void lvgl_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    if (area->x2 < 0 || area->y2 < 0 || area->x1 >= LVGL_HOR_RES || area->y1 >= LVGL_VER_RES) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    lv_area_t clipped = *area;
    if (clipped.x1 < 0) {
        clipped.x1 = 0;
    }
    if (clipped.y1 < 0) {
        clipped.y1 = 0;
    }
    if (clipped.x2 >= LVGL_HOR_RES) {
        clipped.x2 = LVGL_HOR_RES - 1;
    }
    if (clipped.y2 >= LVGL_VER_RES) {
        clipped.y2 = LVGL_VER_RES - 1;
    }

    size_t area_width           = (size_t)(area->x2 - area->x1 + 1);
    size_t width                = (size_t)(clipped.x2 - clipped.x1 + 1);
    size_t height               = (size_t)(clipped.y2 - clipped.y1 + 1);
    size_t px_cnt               = width * height;
    size_t offset               = (size_t)(clipped.y1 - area->y1) * area_width + (size_t)(clipped.x1 - area->x1);
    const lv_color_t *flush_buf = color_p + offset;

    uint32_t start_tick = rt_tick_get();
    if (drv_lcd_set_window((size_t)clipped.x1, (size_t)clipped.y1, (size_t)clipped.x2, (size_t)clipped.y2) == 0) {
        lvgl_flush_state.disp_drv   = disp_drv;
        lvgl_flush_state.start_tick = start_tick;
        lvgl_flush_state.px_cnt     = (uint32_t)px_cnt;

        int result = drv_lcd_write_bytes_async((const uint8_t *)flush_buf, px_cnt * sizeof(lv_color_t), lvgl_flush_done_cb, &lvgl_flush_state);
        if (result == 0) {
            return;
        }
    }

    lv_disp_flush_ready(disp_drv);
}

static void lvgl_anim_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    static uint32_t frame;
    frame++;

    for (int i = 0; i < LVGL_STRESS_BAR_COUNT; i++) {
        uint32_t phase = frame * (3U + (uint32_t)i) + (uint32_t)i * 23U;
        int value      = (int)(phase % 200U);
        if (value > 100) {
            value = 200 - value;
        }
        lv_bar_set_value(bars[i], value, LV_ANIM_OFF);
    }

    for (int i = 0; i < LVGL_STRESS_TILE_COUNT; i++) {
        int tile_size = LVGL_STRESS_TILE_SIZE + (i % 3) * 5;
        int span_x    = LVGL_HOR_RES + tile_size;
        int span_y    = 142 + tile_size;
        int x         = (int)((frame * (2U + (uint32_t)(i % 4)) + (uint32_t)i * 21U) % (uint32_t)span_x) - tile_size;
        int y         = (int)(((frame * (1U + (uint32_t)(i % 3)) + (uint32_t)i * 17U) % (uint32_t)span_y)) - tile_size;

        lv_obj_set_pos(tiles[i], x, y);
        lv_obj_set_style_bg_color(tiles[i], lv_color_hex(tile_colors[(i + (frame >> 4)) % LVGL_STRESS_TILE_COUNT]), LV_PART_MAIN);
    }
}

static void lvgl_perf_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    static lvgl_perf_stats_t last;
    static uint32_t last_tick;

    uint32_t now        = rt_tick_get();
    uint32_t elapsed_ms = now - last_tick;
    if (elapsed_ms == 0) {
        return;
    }

    uint32_t frames       = lvgl_perf_stats.frames - last.frames;
    uint32_t flushes      = lvgl_perf_stats.flushes - last.flushes;
    uint32_t pixels       = lvgl_perf_stats.pixels - last.pixels;
    uint32_t flush_ms     = lvgl_perf_stats.flush_ms - last.flush_ms;
    uint32_t fps_x10      = frames * 10000U / elapsed_ms;
    uint32_t avg_flush_ms = flushes == 0 ? 0 : flush_ms / flushes;

    char text[48];
    rt_snprintf(text, sizeof(text), "FPS %lu.%lu", (unsigned long)(fps_x10 / 10U), (unsigned long)(fps_x10 % 10U));
    lv_label_set_text(fps_label, text);

    rt_snprintf(text, sizeof(text), "flush %lu px %luK avg %lums",
                (unsigned long)flushes,
                (unsigned long)(pixels / 1000U),
                (unsigned long)avg_flush_ms);
    lv_label_set_text(flush_label, text);

    rt_snprintf(text, sizeof(text), "max area %lu px", (unsigned long)lvgl_perf_stats.max_flush_pixels);
    lv_label_set_text(scene_label, text);

    last      = lvgl_perf_stats;
    last_tick = now;
}

static void lvgl_create_stress_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F172A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "LVGL ST7789 stress");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF5F7FA), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    fps_label = lv_label_create(scr);
    lv_label_set_text(fps_label, "FPS --");
    lv_obj_set_style_text_color(fps_label, lv_color_hex(0x7CFF9B), LV_PART_MAIN);
    lv_obj_align(fps_label, LV_ALIGN_TOP_LEFT, 12, 46);

    flush_label = lv_label_create(scr);
    lv_label_set_text(flush_label, "flush --");
    lv_obj_set_style_text_color(flush_label, lv_color_hex(0xA8C7FA), LV_PART_MAIN);
    lv_obj_align(flush_label, LV_ALIGN_TOP_LEFT, 12, 68);

    scene_label = lv_label_create(scr);
    lv_label_set_text(scene_label, "10 tiles / 4 bars");
    lv_obj_set_style_text_color(scene_label, lv_color_hex(0xFCD34D), LV_PART_MAIN);
    lv_obj_align(scene_label, LV_ALIGN_TOP_LEFT, 12, 90);

    for (int i = 0; i < LVGL_STRESS_BAR_COUNT; i++) {
        bars[i] = lv_bar_create(scr);
        lv_obj_set_size(bars[i], 102, 9);
        lv_obj_set_pos(bars[i], 126, 48 + i * 17);
        lv_bar_set_range(bars[i], 0, 100);
        lv_bar_set_value(bars[i], i * 18, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bars[i], lv_color_hex(0x263244), LV_PART_MAIN);
        lv_obj_set_style_bg_color(bars[i], lv_color_hex(tile_colors[i + 1]), LV_PART_INDICATOR);
        lv_obj_set_style_radius(bars[i], 0, LV_PART_MAIN);
        lv_obj_set_style_radius(bars[i], 0, LV_PART_INDICATOR);
        lv_obj_set_style_border_width(bars[i], 0, LV_PART_MAIN);
    }

    lv_obj_t *scene = lv_obj_create(scr);
    lv_obj_remove_style_all(scene);
    lv_obj_set_size(scene, LVGL_HOR_RES, 142);
    lv_obj_set_style_bg_color(scene, lv_color_hex(0x172033), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scene, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(scene, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(scene, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_pos(scene, 0, 132);
    lv_obj_clear_flag(scene, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < LVGL_STRESS_TILE_COUNT; i++) {
        int tile_size = LVGL_STRESS_TILE_SIZE + (i % 3) * 5;
        tiles[i]      = lv_obj_create(scene);
        lv_obj_remove_style_all(tiles[i]);
        lv_obj_set_size(tiles[i], tile_size, tile_size);
        lv_obj_set_style_bg_color(tiles[i], lv_color_hex(tile_colors[i]), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(tiles[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(tiles[i], 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(tiles[i], 0, LV_PART_MAIN);
        lv_obj_set_pos(tiles[i], i * 18, 18 + (i % 4) * 22);
    }

    lv_obj_t *footer = lv_label_create(scr);
    lv_label_set_text(footer, "dirty-area SPI DMA flush");
    lv_obj_set_style_text_color(footer, lv_color_hex(0xD0D6E0), LV_PART_MAIN);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -6);

    lv_timer_create(lvgl_anim_timer_cb, LVGL_ANIM_PERIOD_MS, RT_NULL);
    lv_timer_create(lvgl_perf_timer_cb, LVGL_PERF_PERIOD_MS, RT_NULL);
}

static void lvgl_thread_entry(void *parameter)
{
    (void)parameter;

    while (1) {
        (void)lv_timer_handler();
        rt_thread_mdelay(LVGL_TASK_PERIOD_MS);
    }
}

int app_lvgl_port_init(void)
{
    lv_init();

    lvgl_draw_buffer_init();

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res      = LVGL_HOR_RES;
    disp_drv.ver_res      = LVGL_VER_RES;
    disp_drv.draw_buf     = &lvgl_draw_buf;
    disp_drv.flush_cb     = lvgl_flush_cb;
    disp_drv.full_refresh = 1;
    disp_drv.antialiasing = 0;
    lv_disp_drv_register(&disp_drv);

    lvgl_create_stress_screen();

    rt_thread_t thread = rt_thread_create("lvgl", lvgl_thread_entry, RT_NULL, LVGL_THREAD_STACK, LVGL_THREAD_PRIORITY, LVGL_THREAD_TIMESLICE);
    if (thread == RT_NULL) {
        return -RT_ENOMEM;
    }

    return rt_thread_startup(thread);
}
