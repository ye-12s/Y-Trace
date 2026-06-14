#include "port/lvgl_port.h"

#include <stddef.h>
#include <stdint.h>

#include "Drivers/drv_lcd.h"
#include "lvgl.h"
#include "map/map_benchmark.h"
#include "rtthread.h"

#define LVGL_HOR_RES              240
#define LVGL_VER_RES              320
#define LVGL_DRAW_BUF_TOTAL_BYTES (112U * 1024U)
#define LVGL_DRAW_BUF_LINES       (LVGL_DRAW_BUF_TOTAL_BYTES / 2U / LVGL_HOR_RES / sizeof(lv_color_t))
#define LVGL_FALLBACK_BUF_LINES   40
#define LVGL_THREAD_STACK         4096
#define LVGL_THREAD_PRIORITY      8
#define LVGL_THREAD_TIMESLICE     10
#define LVGL_TASK_MAX_SLEEP_MS    8
#define LVGL_TASK_MIN_SLEEP_MS    1
#define LVGL_PERF_PERIOD_MS       500

static lv_disp_draw_buf_t lvgl_draw_buf;
static lv_color_t lvgl_fallback_buf[LVGL_HOR_RES * LVGL_FALLBACK_BUF_LINES];
static uint32_t lvgl_draw_buffer_bytes;

typedef struct {
    uint32_t frames;
    uint32_t flushes;
    uint32_t pixels;
    uint32_t max_flush_pixels;
    uint32_t flush_ms;
    uint32_t handler_ms;
    uint32_t max_handler_ms;
    uint32_t errors;
    uint32_t refreshes;
    uint32_t max_refresh_ms;
} lvgl_perf_stats_t;

typedef struct {
    lv_disp_drv_t *disp_drv;
    uint32_t start_tick;
    uint32_t px_cnt;
} lvgl_flush_state_t;

static lvgl_perf_stats_t lvgl_perf_stats;
static lvgl_flush_state_t lvgl_flush_state;

static void lvgl_draw_buffer_init(void)
{
    lv_color_t *heap_buf = (lv_color_t *)rt_malloc(LVGL_DRAW_BUF_TOTAL_BYTES);
    if (heap_buf != RT_NULL) {
        lv_color_t *buf1 = heap_buf;
        lv_color_t *buf2 = buf1 + (LVGL_HOR_RES * LVGL_DRAW_BUF_LINES);

        lv_disp_draw_buf_init(&lvgl_draw_buf, buf1, buf2, LVGL_HOR_RES * LVGL_DRAW_BUF_LINES);
        lvgl_draw_buffer_bytes = LVGL_HOR_RES * LVGL_DRAW_BUF_LINES * sizeof(lv_color_t) * 2U;
        rt_kprintf("lvgl: dual DMA draw buffers at 0x%08x/0x%08x, %u lines each\n",
                   (unsigned int)(uintptr_t)buf1,
                   (unsigned int)(uintptr_t)buf2,
                   (unsigned int)LVGL_DRAW_BUF_LINES);
        return;
    }

    lv_disp_draw_buf_init(&lvgl_draw_buf, lvgl_fallback_buf, RT_NULL, sizeof(lvgl_fallback_buf) / sizeof(lvgl_fallback_buf[0]));
    lvgl_draw_buffer_bytes = sizeof(lvgl_fallback_buf);
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
    } else {
        lvgl_perf_stats.errors++;
    }

    lv_disp_flush_ready(state->disp_drv);
}

static void lvgl_monitor_cb(lv_disp_drv_t *disp_drv, uint32_t time, uint32_t px)
{
    (void)disp_drv;
    (void)px;

    lvgl_perf_stats.refreshes++;
    if (time > lvgl_perf_stats.max_refresh_ms) {
        lvgl_perf_stats.max_refresh_ms = time;
    }
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
    rt_bool_t contiguous        = width == area_width;

    uint32_t start_tick = rt_tick_get();
    if (drv_lcd_set_window((size_t)clipped.x1, (size_t)clipped.y1, (size_t)clipped.x2, (size_t)clipped.y2) == 0) {
        lvgl_flush_state.disp_drv   = disp_drv;
        lvgl_flush_state.start_tick = start_tick;
        lvgl_flush_state.px_cnt     = (uint32_t)px_cnt;

        if (contiguous) {
            int result = drv_lcd_write_bytes_async((const uint8_t *)flush_buf, px_cnt * sizeof(lv_color_t), lvgl_flush_done_cb, &lvgl_flush_state);
            if (result == 0) {
                return;
            }
        } else {
            int result = 0;
            for (size_t row = 0; row < height; row++) {
                const lv_color_t *row_buf = flush_buf + (row * area_width);
                result                    = drv_lcd_write_bytes((const uint8_t *)row_buf, width * sizeof(lv_color_t));
                if (result != 0) {
                    break;
                }
            }
            if (result == 0) {
                lvgl_record_flush(disp_drv, (uint32_t)px_cnt, start_tick);
                lv_disp_flush_ready(disp_drv);
                return;
            }
        }

        lvgl_perf_stats.errors++;
    } else {
        lvgl_perf_stats.errors++;
    }

    lv_disp_flush_ready(disp_drv);
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

    uint32_t frames           = lvgl_perf_stats.frames - last.frames;
    uint32_t flushes          = lvgl_perf_stats.flushes - last.flushes;
    uint32_t pixels           = lvgl_perf_stats.pixels - last.pixels;
    uint32_t flush_ms         = lvgl_perf_stats.flush_ms - last.flush_ms;
    uint32_t handler_ms       = lvgl_perf_stats.handler_ms - last.handler_ms;
    uint32_t errors           = lvgl_perf_stats.errors - last.errors;
    uint32_t fps_x10          = frames * 10000U / elapsed_ms;
    uint32_t avg_flush_ms     = flushes == 0 ? 0 : flush_ms / flushes;
    uint32_t handler_load_pct = handler_ms * 100U / elapsed_ms;

    app_map_benchmark_metrics_t metrics = {
        .fps_x10           = fps_x10,
        .handler_load_pct  = handler_load_pct,
        .flushes           = flushes,
        .pixels            = pixels,
        .avg_flush_ms      = avg_flush_ms,
        .max_flush_pixels  = lvgl_perf_stats.max_flush_pixels,
        .max_handler_ms    = lvgl_perf_stats.max_handler_ms,
        .max_refresh_ms    = lvgl_perf_stats.max_refresh_ms,
        .refreshes         = lvgl_perf_stats.refreshes - last.refreshes,
        .errors            = errors,
        .draw_buffer_bytes = lvgl_draw_buffer_bytes,
    };
    app_map_benchmark_update_metrics(&metrics);

    last      = lvgl_perf_stats;
    last_tick = now;
}

static void lvgl_create_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    app_map_benchmark_create(scr, lvgl_draw_buffer_bytes);
    lv_timer_create(lvgl_perf_timer_cb, LVGL_PERF_PERIOD_MS, RT_NULL);
}

static void lvgl_thread_entry(void *parameter)
{
    (void)parameter;

    while (1) {
        uint32_t start_tick = rt_tick_get();
        uint32_t wait_ms    = lv_timer_handler();
        uint32_t elapsed_ms = rt_tick_get() - start_tick;

        lvgl_perf_stats.handler_ms += elapsed_ms;
        if (elapsed_ms > lvgl_perf_stats.max_handler_ms) {
            lvgl_perf_stats.max_handler_ms = elapsed_ms;
        }

        if (wait_ms == LV_NO_TIMER_READY || wait_ms > LVGL_TASK_MAX_SLEEP_MS) {
            wait_ms = LVGL_TASK_MAX_SLEEP_MS;
        } else if (wait_ms < LVGL_TASK_MIN_SLEEP_MS) {
            wait_ms = LVGL_TASK_MIN_SLEEP_MS;
        }
        rt_thread_mdelay(wait_ms);
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
    disp_drv.monitor_cb   = lvgl_monitor_cb;
    disp_drv.antialiasing = 0;
    lv_disp_drv_register(&disp_drv);

    lvgl_create_screen();

    rt_thread_t thread = rt_thread_create("lvgl", lvgl_thread_entry, RT_NULL, LVGL_THREAD_STACK, LVGL_THREAD_PRIORITY, LVGL_THREAD_TIMESLICE);
    if (thread == RT_NULL) {
        return -RT_ENOMEM;
    }

    return rt_thread_startup(thread);
}
