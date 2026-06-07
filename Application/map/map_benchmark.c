#include "map/map_benchmark.h"

#include <stddef.h>
#include <stdint.h>

#include "map/ytrace_map_tiles.h"
#include "rtthread.h"

#define MAP_BENCHMARK_HOR_RES        240
#define MAP_BENCHMARK_VER_RES        320
#define MAP_BENCHMARK_TILE_SIZE      80
#define MAP_BENCHMARK_TILE_SCALE     (MAP_BENCHMARK_TILE_SIZE / YTRACE_MAP_TILE_WIDTH_PX)
#define MAP_BENCHMARK_ANIM_PERIOD_MS 16
#define MAP_BENCHMARK_GRID_W         8
#define MAP_BENCHMARK_GRID_H         8
#define MAP_BENCHMARK_OVERLAY_W      232
#define MAP_BENCHMARK_LV_OBJ_EST     96U
#define MAP_BENCHMARK_LV_TIMER_EST   48U

typedef struct {
    lv_obj_t *map;
    lv_obj_t *fps_label;
    lv_obj_t *flush_label;
    lv_obj_t *memory_label;
    lv_obj_t *flash_label;
    uint32_t offset_x;
    uint32_t offset_y;
    uint32_t draw_buffer_bytes;
    lv_color_t palette_colors[YTRACE_MAP_TILE_PALETTE_COLOR_COUNT];
} map_benchmark_state_t;

static map_benchmark_state_t map_state;

static uint32_t map_tile_catalog_bytes(void)
{
    return (uint32_t)YTRACE_MAP_TILE_TOTAL_BYTES;
}

static lv_color_t map_rgb565(uint16_t color)
{
    return lv_color_make((uint8_t)(((color >> 11) & 0x1FU) << 3),
                         (uint8_t)(((color >> 5) & 0x3FU) << 2),
                         (uint8_t)((color & 0x1FU) << 3));
}

static void map_init_palette_cache(void)
{
    for (uint16_t index = 0; index < YTRACE_MAP_TILE_PALETTE_COLOR_COUNT; index++) {
        map_state.palette_colors[index] = map_rgb565(ytrace_map_tiles_palette[index]);
    }
}

static uint8_t map_visible_src_range(int32_t tile_origin, int32_t clip_start, int32_t clip_end, uint16_t source_len, uint16_t *src_start, uint16_t *src_end)
{
    int32_t start = 0;
    int32_t end   = (int32_t)source_len - 1;

    if (clip_start > tile_origin) {
        start = (clip_start - tile_origin) / MAP_BENCHMARK_TILE_SCALE;
    }

    if (clip_end < tile_origin + MAP_BENCHMARK_TILE_SIZE - 1) {
        end = (clip_end - tile_origin) / MAP_BENCHMARK_TILE_SCALE;
    }

    if (start < 0) {
        start = 0;
    }

    if (end >= (int32_t)source_len) {
        end = (int32_t)source_len - 1;
    }

    if (start > end || start >= (int32_t)source_len || end < 0) {
        return 0U;
    }

    *src_start = (uint16_t)start;
    *src_end   = (uint16_t)end;
    return 1U;
}

static uint16_t map_src_mask(uint16_t src_x_start, uint16_t src_x_end)
{
    uint16_t mask = 0U;
    for (uint16_t src_x = src_x_start; src_x <= src_x_end; src_x++) {
        mask |= (uint16_t)(1UL << src_x);
    }

    return mask;
}

static uint8_t map_src_rect_can_extend(const uint8_t *pixels,
                                       const uint16_t *drawn_rows,
                                       uint16_t src_y,
                                       uint16_t src_x_start,
                                       uint16_t src_x_end,
                                       uint8_t palette_index)
{
    uint16_t mask = map_src_mask(src_x_start, src_x_end);
    if ((drawn_rows[src_y] & mask) != 0U) {
        return 0U;
    }

    for (uint16_t src_x = src_x_start; src_x <= src_x_end; src_x++) {
        if (pixels[(src_y * YTRACE_MAP_TILE_WIDTH_PX) + src_x] != palette_index) {
            return 0U;
        }
    }

    return 1U;
}

static void map_mark_src_rect(uint16_t *drawn_rows, uint16_t src_y_start, uint16_t src_y_end, uint16_t src_x_start, uint16_t src_x_end)
{
    uint16_t mask = map_src_mask(src_x_start, src_x_end);
    for (uint16_t src_y = src_y_start; src_y <= src_y_end; src_y++) {
        drawn_rows[src_y] |= mask;
    }
}

static void map_draw_tile_rect(lv_draw_ctx_t *draw_ctx,
                               const lv_area_t *tile_area,
                               lv_draw_rect_dsc_t *dsc,
                               uint16_t src_x_start,
                               uint16_t src_y_start,
                               uint16_t src_x_end,
                               uint16_t src_y_end,
                               uint8_t palette_index)
{
    dsc->bg_color = map_state.palette_colors[palette_index % YTRACE_MAP_TILE_PALETTE_COLOR_COUNT];

    lv_area_t rect_area = {
        .x1 = tile_area->x1 + (int32_t)(src_x_start * MAP_BENCHMARK_TILE_SCALE),
        .y1 = tile_area->y1 + (int32_t)(src_y_start * MAP_BENCHMARK_TILE_SCALE),
        .x2 = tile_area->x1 + (int32_t)(((src_x_end + 1U) * MAP_BENCHMARK_TILE_SCALE) - 1U),
        .y2 = tile_area->y1 + (int32_t)(((src_y_end + 1U) * MAP_BENCHMARK_TILE_SCALE) - 1U),
    };

    lv_draw_rect(draw_ctx, dsc, &rect_area);
}

static void map_draw_tile(lv_draw_ctx_t *draw_ctx, const lv_area_t *clip, int32_t tile_x, int32_t tile_y, uint16_t tile_index)
{
    lv_area_t area = {
        .x1 = tile_x,
        .y1 = tile_y,
        .x2 = tile_x + MAP_BENCHMARK_TILE_SIZE - 1,
        .y2 = tile_y + MAP_BENCHMARK_TILE_SIZE - 1,
    };

    const uint8_t *pixels = ytrace_map_tile_pixels(tile_index);
    if (pixels == RT_NULL) {
        return;
    }

    uint16_t src_x_start;
    uint16_t src_x_end;
    uint16_t src_y_start;
    uint16_t src_y_end;
    if (!map_visible_src_range(area.x1, clip->x1, clip->x2, YTRACE_MAP_TILE_WIDTH_PX, &src_x_start, &src_x_end) ||
        !map_visible_src_range(area.y1, clip->y1, clip->y2, YTRACE_MAP_TILE_HEIGHT_PX, &src_y_start, &src_y_end)) {
        return;
    }

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = 0;

    uint16_t drawn_rows[YTRACE_MAP_TILE_HEIGHT_PX] = {0U};

    for (uint16_t src_y = src_y_start; src_y <= src_y_end; src_y++) {
        uint16_t src_x = src_x_start;
        while (src_x <= src_x_end) {
            uint16_t src_bit = (uint16_t)(1UL << src_x);
            if ((drawn_rows[src_y] & src_bit) != 0U) {
                src_x++;
                continue;
            }

            uint8_t palette_index = pixels[(src_y * YTRACE_MAP_TILE_WIDTH_PX) + src_x];
            uint16_t rect_x_end   = src_x;
            while (rect_x_end < src_x_end &&
                   map_src_rect_can_extend(pixels, drawn_rows, src_y, src_x, (uint16_t)(rect_x_end + 1U), palette_index)) {
                rect_x_end++;
            }

            uint16_t rect_y_end = src_y;
            while (rect_y_end < src_y_end &&
                   map_src_rect_can_extend(pixels, drawn_rows, (uint16_t)(rect_y_end + 1U), src_x, rect_x_end, palette_index)) {
                rect_y_end++;
            }

            map_mark_src_rect(drawn_rows, src_y, rect_y_end, src_x, rect_x_end);
            map_draw_tile_rect(draw_ctx, &area, &dsc, src_x, src_y, rect_x_end, rect_y_end, palette_index);
            src_x = (uint16_t)(rect_x_end + 1U);
        }
    }

    lv_draw_rect_dsc_t border;
    lv_draw_rect_dsc_init(&border);
    border.bg_opa       = LV_OPA_TRANSP;
    border.border_width = 1;
    border.border_color = lv_color_hex(0x526A58);
    border.border_opa   = LV_OPA_30;
    border.radius       = 0;
    lv_draw_rect(draw_ctx, &border, &area);
}

static uint16_t map_get_tile_index(uint32_t x, uint32_t y)
{
    uint32_t wrapped_x = x % MAP_BENCHMARK_GRID_W;
    uint32_t wrapped_y = y % MAP_BENCHMARK_GRID_H;
    return (uint16_t)((wrapped_y * MAP_BENCHMARK_GRID_W + wrapped_x) % YTRACE_MAP_TILE_COUNT);
}

static lv_coord_t map_screen_x(const lv_area_t *coords, int16_t x)
{
    return coords->x1 + x;
}

static lv_coord_t map_screen_y(const lv_area_t *coords, int16_t y)
{
    return coords->y1 + y;
}

static void map_draw_screen_line(lv_draw_ctx_t *draw_ctx,
                                 const lv_area_t *coords,
                                 int16_t x1,
                                 int16_t y1,
                                 int16_t x2,
                                 int16_t y2,
                                 lv_color_t color,
                                 uint8_t width,
                                 lv_opa_t opa)
{
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = color;
    dsc.width = width;
    dsc.opa   = opa;

    lv_point_t p1 = {
        .x = map_screen_x(coords, x1),
        .y = map_screen_y(coords, y1),
    };
    lv_point_t p2 = {
        .x = map_screen_x(coords, x2),
        .y = map_screen_y(coords, y2),
    };

    lv_draw_line(draw_ctx, &dsc, &p1, &p2);
}

static void map_draw_circle(lv_draw_ctx_t *draw_ctx,
                            const lv_area_t *coords,
                            int16_t cx,
                            int16_t cy,
                            int16_t radius,
                            lv_color_t fill,
                            lv_color_t border,
                            uint8_t border_width)
{
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color     = fill;
    dsc.bg_opa       = LV_OPA_COVER;
    dsc.radius       = LV_RADIUS_CIRCLE;
    dsc.border_color = border;
    dsc.border_width = border_width;
    dsc.border_opa   = LV_OPA_COVER;

    lv_area_t area = {
        .x1 = map_screen_x(coords, cx - radius),
        .y1 = map_screen_y(coords, cy - radius),
        .x2 = map_screen_x(coords, cx + radius),
        .y2 = map_screen_y(coords, cy + radius),
    };
    lv_draw_rect(draw_ctx, &dsc, &area);
}

static void map_draw_poi(lv_draw_ctx_t *draw_ctx, const lv_area_t *coords, int16_t cx, int16_t cy, lv_color_t color)
{
    map_draw_circle(draw_ctx, coords, cx, cy, 4, color, lv_color_hex(0xFFFFFF), 2);
}

static void map_draw_position_marker(lv_draw_ctx_t *draw_ctx, const lv_area_t *coords)
{
    map_draw_circle(draw_ctx, coords, 120, 174, 11, lv_color_hex(0x2F80ED), lv_color_hex(0xFFFFFF), 3);
    map_draw_screen_line(draw_ctx, coords, 120, 159, 120, 144, lv_color_hex(0x2F80ED), 5, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 120, 159, 112, 151, lv_color_hex(0xFFFFFF), 2, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 120, 159, 128, 151, lv_color_hex(0xFFFFFF), 2, LV_OPA_COVER);
}

static void map_draw_compass(lv_draw_ctx_t *draw_ctx, const lv_area_t *coords)
{
    map_draw_circle(draw_ctx, coords, 24, 292, 13, lv_color_hex(0x29445E), lv_color_hex(0xFFFFFF), 2);
    map_draw_screen_line(draw_ctx, coords, 24, 300, 24, 278, lv_color_hex(0xFFFFFF), 3, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 24, 278, 18, 288, lv_color_hex(0xE53935), 3, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 24, 278, 30, 288, lv_color_hex(0xE53935), 3, LV_OPA_COVER);
}

static void map_draw_navigation_overlay(lv_draw_ctx_t *draw_ctx, const lv_area_t *coords)
{
    map_draw_screen_line(draw_ctx, coords, -12, 70, 252, 116, lv_color_hex(0x4A90E2), 16, LV_OPA_70);
    map_draw_screen_line(draw_ctx, coords, 10, 242, 80, 190, lv_color_hex(0xC8C3B2), 9, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 80, 190, 158, 150, lv_color_hex(0xC8C3B2), 9, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 158, 150, 250, 96, lv_color_hex(0xC8C3B2), 9, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 10, 242, 80, 190, lv_color_hex(0xF7F5EA), 5, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 80, 190, 158, 150, lv_color_hex(0xF7F5EA), 5, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 158, 150, 250, 96, lv_color_hex(0xF7F5EA), 5, LV_OPA_COVER);

    map_draw_screen_line(draw_ctx, coords, 36, 124, 96, 160, lv_color_hex(0xB9B3A1), 6, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 96, 160, 180, 236, lv_color_hex(0xB9B3A1), 6, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 36, 124, 96, 160, lv_color_hex(0xFFF7D8), 3, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 96, 160, 180, 236, lv_color_hex(0xFFF7D8), 3, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 168, 24, 154, 92, lv_color_hex(0xD0CCBC), 5, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 154, 92, 120, 174, lv_color_hex(0xFFFFFF), 3, LV_OPA_COVER);

    map_draw_screen_line(draw_ctx, coords, 8, 240, 80, 190, lv_color_hex(0xFFFFFF), 12, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 80, 190, 120, 174, lv_color_hex(0xFFFFFF), 12, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 120, 174, 158, 150, lv_color_hex(0xFFFFFF), 12, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 158, 150, 238, 108, lv_color_hex(0xFFFFFF), 12, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 8, 240, 80, 190, lv_color_hex(0xE53935), 7, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 80, 190, 120, 174, lv_color_hex(0xE53935), 7, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 120, 174, 158, 150, lv_color_hex(0xE53935), 7, LV_OPA_COVER);
    map_draw_screen_line(draw_ctx, coords, 158, 150, 238, 108, lv_color_hex(0xE53935), 7, LV_OPA_COVER);

    map_draw_poi(draw_ctx, coords, 60, 210, lv_color_hex(0x2E7D32));
    map_draw_poi(draw_ctx, coords, 188, 132, lv_color_hex(0xF2C94C));
    map_draw_poi(draw_ctx, coords, 176, 228, lv_color_hex(0x9B51E0));
    map_draw_position_marker(draw_ctx, coords);
    map_draw_compass(draw_ctx, coords);
}

static uint32_t map_wrap_offset(int64_t value, uint32_t limit)
{
    int64_t wrapped = value % (int64_t)limit;
    if (wrapped < 0) {
        wrapped += limit;
    }

    return (uint32_t)wrapped;
}

static void map_draw_event_cb(lv_event_t *event)
{
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(event);
    lv_obj_t *obj           = lv_event_get_target(event);
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    uint32_t sx   = map_state.offset_x % MAP_BENCHMARK_TILE_SIZE;
    uint32_t sy   = map_state.offset_y % MAP_BENCHMARK_TILE_SIZE;
    uint32_t col0 = (map_state.offset_x / MAP_BENCHMARK_TILE_SIZE) % MAP_BENCHMARK_GRID_W;
    uint32_t row0 = (map_state.offset_y / MAP_BENCHMARK_TILE_SIZE) % MAP_BENCHMARK_GRID_H;

    for (int32_t y = -(int32_t)sy, row = 0; y < MAP_BENCHMARK_VER_RES; y += MAP_BENCHMARK_TILE_SIZE, row++) {
        for (int32_t x = -(int32_t)sx, col = 0; x < MAP_BENCHMARK_HOR_RES; x += MAP_BENCHMARK_TILE_SIZE, col++) {
            uint16_t tile_index = map_get_tile_index(col0 + (uint32_t)col, row0 + (uint32_t)row);
            map_draw_tile(draw_ctx, &coords, coords.x1 + x, coords.y1 + y, tile_index);
        }
    }

    map_draw_navigation_overlay(draw_ctx, &coords);
}

static void map_anim_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    app_map_benchmark_pan(3, 1);
}

static lv_obj_t *map_create_overlay_label(lv_obj_t *parent, int16_t y, const char *text, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(label, MAP_BENCHMARK_OVERLAY_W);
    lv_obj_set_pos(label, 4, y);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_bg_color(label, lv_color_hex(0x29445E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_pad_left(label, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_right(label, 3, LV_PART_MAIN);
    return label;
}

void app_map_benchmark_create(lv_obj_t *parent, uint32_t draw_buffer_bytes)
{
    map_state.draw_buffer_bytes = draw_buffer_bytes;
    map_state.offset_x          = 0;
    map_state.offset_y          = 0;
    map_init_palette_cache();

    lv_obj_set_style_bg_color(parent, lv_color_hex(0xAFCDA1), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);

    map_state.map = lv_obj_create(parent);
    lv_obj_remove_style_all(map_state.map);
    lv_obj_set_size(map_state.map, MAP_BENCHMARK_HOR_RES, MAP_BENCHMARK_VER_RES);
    lv_obj_set_pos(map_state.map, 0, 0);
    lv_obj_clear_flag(map_state.map, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(map_state.map, map_draw_event_cb, LV_EVENT_DRAW_MAIN, RT_NULL);

    map_state.fps_label    = map_create_overlay_label(parent, 6, "Map tile bench", lv_color_hex(0xFFFFFF));
    map_state.flush_label  = map_create_overlay_label(parent, 24, "FPS -- Load --%", lv_color_hex(0xE7FCD8));
    map_state.memory_label = map_create_overlay_label(parent, 42, "F -- Px --K Mx --K", lv_color_hex(0xD7E9FF));
    map_state.flash_label  = map_create_overlay_label(parent, 60, "TileF --B Avg -- Err --", lv_color_hex(0xFFF4C2));

    lv_timer_create(map_anim_timer_cb, MAP_BENCHMARK_ANIM_PERIOD_MS, RT_NULL);
}

void app_map_benchmark_pan(int32_t dx, int32_t dy)
{
    uint32_t world_w = MAP_BENCHMARK_GRID_W * MAP_BENCHMARK_TILE_SIZE;
    uint32_t world_h = MAP_BENCHMARK_GRID_H * MAP_BENCHMARK_TILE_SIZE;

    map_state.offset_x = map_wrap_offset((int64_t)map_state.offset_x + dx, world_w);
    map_state.offset_y = map_wrap_offset((int64_t)map_state.offset_y + dy, world_h);

    if (map_state.map != RT_NULL) {
        lv_obj_invalidate(map_state.map);
    }
}

void app_map_benchmark_update_metrics(const app_map_benchmark_metrics_t *metrics)
{
    if (metrics == RT_NULL || map_state.fps_label == RT_NULL) {
        return;
    }

    uint32_t tile_bytes  = app_map_benchmark_tile_bytes();
    uint32_t ram_bytes   = app_map_benchmark_estimated_ram_bytes(metrics->draw_buffer_bytes);
    uint32_t flash_bytes = app_map_benchmark_tile_flash_bytes();

    char text[64];
    rt_snprintf(text,
                sizeof(text),
                "FPS %lu.%lu Load %lu%% Hmax %lums",
                (unsigned long)(metrics->fps_x10 / 10U),
                (unsigned long)(metrics->fps_x10 % 10U),
                (unsigned long)metrics->handler_load_pct,
                (unsigned long)metrics->max_handler_ms);
    lv_label_set_text(map_state.fps_label, text);

    rt_snprintf(text,
                sizeof(text),
                "Fl %lu Px %luK Mx %luK Rf %lu",
                (unsigned long)metrics->flushes,
                (unsigned long)(metrics->pixels / 1000U),
                (unsigned long)(metrics->max_flush_pixels / 1000U),
                (unsigned long)metrics->refreshes);
    lv_label_set_text(map_state.flush_label, text);

    rt_snprintf(text,
                sizeof(text),
                "Tile %luB Buf %luK RAM %luK",
                (unsigned long)tile_bytes,
                (unsigned long)(metrics->draw_buffer_bytes / 1024U),
                (unsigned long)(ram_bytes / 1024U));
    lv_label_set_text(map_state.memory_label, text);

    rt_snprintf(text,
                sizeof(text),
                "TileF %luB Avg %lums Err %lu",
                (unsigned long)flash_bytes,
                (unsigned long)metrics->avg_flush_ms,
                (unsigned long)metrics->errors);
    lv_label_set_text(map_state.flash_label, text);

    rt_kprintf("map_bench: fps=%lu.%lu handler_load=%lu%% flushes=%lu pixels=%lu max_flush=%lu avg_flush_ms=%lu hmax_ms=%lu refresh_max_ms=%lu tile_bytes=%lu draw_buf=%lu est_ram=%lu tile_flash=%lu errors=%lu\n",
               (unsigned long)(metrics->fps_x10 / 10U),
               (unsigned long)(metrics->fps_x10 % 10U),
               (unsigned long)metrics->handler_load_pct,
               (unsigned long)metrics->flushes,
               (unsigned long)metrics->pixels,
               (unsigned long)metrics->max_flush_pixels,
               (unsigned long)metrics->avg_flush_ms,
               (unsigned long)metrics->max_handler_ms,
               (unsigned long)metrics->max_refresh_ms,
               (unsigned long)tile_bytes,
               (unsigned long)metrics->draw_buffer_bytes,
               (unsigned long)ram_bytes,
               (unsigned long)flash_bytes,
               (unsigned long)metrics->errors);
}

uint32_t app_map_benchmark_tile_bytes(void)
{
    return map_tile_catalog_bytes();
}

uint32_t app_map_benchmark_estimated_ram_bytes(uint32_t draw_buffer_bytes)
{
    return draw_buffer_bytes + (uint32_t)sizeof(map_state) + (5U * MAP_BENCHMARK_LV_OBJ_EST) + MAP_BENCHMARK_LV_TIMER_EST;
}

uint32_t app_map_benchmark_tile_flash_bytes(void)
{
    return map_tile_catalog_bytes();
}
