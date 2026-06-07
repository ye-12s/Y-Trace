#!/usr/bin/env python3
"""Generate render-optimized map tiles for the LVGL benchmark."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "Application" / "map" / "ytrace_map_tiles.c"

WIDTH = 16
HEIGHT = 16
TILE_COUNT = 64

PALETTE = [
    0xFFFF,
    0xF7BE,
    0xC618,
    0x8410,
    0x7E0F,
    0x5D6B,
    0x2C49,
    0x39E7,
    0xFFE0,
    0xFD20,
    0xBDF7,
    0x94B2,
    0x4A69,
    0x001F,
    0xF800,
    0x0000,
]


def fill_rect(tile: list[int], x1: int, y1: int, x2: int, y2: int, color: int) -> None:
    x1 = max(0, min(WIDTH - 1, x1))
    x2 = max(0, min(WIDTH - 1, x2))
    y1 = max(0, min(HEIGHT - 1, y1))
    y2 = max(0, min(HEIGHT - 1, y2))
    for y in range(y1, y2 + 1):
        for x in range(x1, x2 + 1):
            tile[(y * WIDTH) + x] = color


def make_tile(index: int) -> tuple[int, ...]:
    tx = index % 8
    ty = index // 8
    base_colors = [8, 1, 10, 5]
    tile = [base_colors[(tx + ty) % len(base_colors)]] * (WIDTH * HEIGHT)

    district = (index * 5 + ty) % 6
    if district == 0:
        fill_rect(tile, 0, 0, 6, 5, 5)
    elif district == 1:
        fill_rect(tile, 9, 10, 15, 15, 1)
    elif district == 2:
        fill_rect(tile, 0, 11, 6, 15, 10)
    elif district == 3:
        fill_rect(tile, 10, 0, 15, 6, 8)
    elif district == 4:
        fill_rect(tile, 2, 2, 13, 5, 6)
    else:
        fill_rect(tile, 3, 9, 12, 14, 11)

    water = (index + tx * 2) % 5
    if water == 0:
        fill_rect(tile, 0, 3 + (ty % 3), WIDTH - 1, 5 + (ty % 3), 4)
    elif water == 1:
        fill_rect(tile, 2 + (tx % 4), 0, 4 + (tx % 4), HEIGHT - 1, 4)
    elif water == 2:
        fill_rect(tile, 8, 0, 15, 4 + (ty % 4), 13)
    elif water == 3:
        fill_rect(tile, 0, 11 - (tx % 3), 7, 15, 4)

    marker_color = [6, 7, 9, 11][ty % 4]
    marker_w = 2 + (index % 4)
    marker_h = 2 + ((index // 4) % 3)
    marker_x = (index * 3) % (WIDTH - marker_w)
    marker_y = (index * 5) % (HEIGHT - marker_h)
    fill_rect(tile, marker_x, marker_y, marker_x + marker_w - 1, marker_y + marker_h - 1, marker_color)

    return tuple(tile)


def emit_array(values: list[int], indent: str = "    ") -> str:
    lines: list[str] = []
    for index in range(0, len(values), 16):
        row = values[index : index + 16]
        lines.append(indent + ", ".join(str(value) for value in row) + ",")
    return "\n".join(lines)


def main() -> int:
    tiles = [make_tile(index) for index in range(TILE_COUNT)]
    if len(set(tiles)) != TILE_COUNT:
        raise SystemExit("generated tiles are not unique")

    pixels = [value for tile in tiles for value in tile]
    tile_blocks = []
    for index, tile in enumerate(tiles):
        tile_blocks.append(f"    /* tile {index:02d} */\n{emit_array(list(tile))}")

    source = f'''#include "map/ytrace_map_tiles.h"

_Static_assert(YTRACE_MAP_TILE_PIXEL_BYTES == (YTRACE_MAP_TILE_WIDTH_PX * YTRACE_MAP_TILE_HEIGHT_PX * YTRACE_MAP_TILE_COUNT),
               "map tile pixel byte metadata is inconsistent");
_Static_assert(YTRACE_MAP_TILE_PALETTE_BYTES == (YTRACE_MAP_TILE_PALETTE_COLOR_COUNT * sizeof(uint16_t)),
               "map tile palette byte metadata is inconsistent");
_Static_assert(YTRACE_MAP_TILE_TOTAL_BYTES == (YTRACE_MAP_TILE_PIXEL_BYTES + YTRACE_MAP_TILE_PALETTE_BYTES),
               "map tile total byte metadata is inconsistent");

const uint16_t ytrace_map_tiles_palette[YTRACE_MAP_TILE_PALETTE_COLOR_COUNT] = {{
{emit_array(PALETTE)}
}};

const uint8_t ytrace_map_tiles_pixels[YTRACE_MAP_TILE_PIXEL_BYTES] = {{
{chr(10).join(tile_blocks)}
}};

const ytrace_map_tile_resource_t ytrace_map_tiles = {{
    .width_px            = YTRACE_MAP_TILE_WIDTH_PX,
    .height_px           = YTRACE_MAP_TILE_HEIGHT_PX,
    .tile_count          = YTRACE_MAP_TILE_COUNT,
    .palette_color_count = YTRACE_MAP_TILE_PALETTE_COLOR_COUNT,
    .bytes_per_pixel     = YTRACE_MAP_TILE_BYTES_PER_PIXEL,
    .format              = YTRACE_MAP_TILE_FORMAT_INDEX8_RGB565,
    .pixel_bytes         = YTRACE_MAP_TILE_PIXEL_BYTES,
    .palette_bytes       = YTRACE_MAP_TILE_PALETTE_BYTES,
    .total_bytes         = YTRACE_MAP_TILE_TOTAL_BYTES,
    .pixels              = ytrace_map_tiles_pixels,
    .palette_rgb565      = ytrace_map_tiles_palette,
}};

const uint8_t *ytrace_map_tile_pixels(uint16_t tile_index)
{{
    if (tile_index >= YTRACE_MAP_TILE_COUNT) {{
        return 0;
    }}

    return &ytrace_map_tiles_pixels[tile_index * YTRACE_MAP_TILE_WIDTH_PX * YTRACE_MAP_TILE_HEIGHT_PX];
}}
'''
    OUTPUT.write_text(source, encoding="utf-8")
    print(f"wrote {OUTPUT.relative_to(ROOT)} with {len(pixels)} pixel indices")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
