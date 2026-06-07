#ifndef Y_TRACE_MAP_BENCHMARK_H
#define Y_TRACE_MAP_BENCHMARK_H

#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t fps_x10;
    uint32_t handler_load_pct;
    uint32_t flushes;
    uint32_t pixels;
    uint32_t avg_flush_ms;
    uint32_t max_flush_pixels;
    uint32_t max_handler_ms;
    uint32_t max_refresh_ms;
    uint32_t refreshes;
    uint32_t errors;
    uint32_t draw_buffer_bytes;
} app_map_benchmark_metrics_t;

void app_map_benchmark_create(lv_obj_t *parent, uint32_t draw_buffer_bytes);
void app_map_benchmark_pan(int32_t dx, int32_t dy);
void app_map_benchmark_update_metrics(const app_map_benchmark_metrics_t *metrics);
uint32_t app_map_benchmark_tile_bytes(void);
uint32_t app_map_benchmark_estimated_ram_bytes(uint32_t draw_buffer_bytes);
uint32_t app_map_benchmark_tile_flash_bytes(void);

#ifdef __cplusplus
}
#endif

#endif
