#ifndef YTRACE_MAP_TILES_H
#define YTRACE_MAP_TILES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YTRACE_MAP_TILE_WIDTH_PX            16U
#define YTRACE_MAP_TILE_HEIGHT_PX           16U
#define YTRACE_MAP_TILE_COUNT               64U
#define YTRACE_MAP_TILE_PALETTE_COLOR_COUNT 16U
#define YTRACE_MAP_TILE_BYTES_PER_PIXEL     1U
#define YTRACE_MAP_TILE_PIXEL_BYTES         16384U
#define YTRACE_MAP_TILE_PALETTE_BYTES       32U
#define YTRACE_MAP_TILE_TOTAL_BYTES         16416U

typedef enum {
    YTRACE_MAP_TILE_FORMAT_INDEX8_RGB565 = 1,
} ytrace_map_tile_format_t;

typedef struct {
    uint16_t width_px;
    uint16_t height_px;
    uint16_t tile_count;
    uint16_t palette_color_count;
    uint8_t bytes_per_pixel;
    ytrace_map_tile_format_t format;
    size_t pixel_bytes;
    size_t palette_bytes;
    size_t total_bytes;
    const uint8_t *pixels;
    const uint16_t *palette_rgb565;
} ytrace_map_tile_resource_t;

extern const uint16_t ytrace_map_tiles_palette[YTRACE_MAP_TILE_PALETTE_COLOR_COUNT];
extern const uint8_t ytrace_map_tiles_pixels[YTRACE_MAP_TILE_PIXEL_BYTES];
extern const ytrace_map_tile_resource_t ytrace_map_tiles;

const uint8_t *ytrace_map_tile_pixels(uint16_t tile_index);

#ifdef __cplusplus
}
#endif

#endif
