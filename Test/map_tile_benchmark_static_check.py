#!/usr/bin/env python3
"""Static verification for the LVGL map tile benchmark surface.

This check is intentionally host-only. It does not prove runtime FPS, but it
guards the benchmark contract that on-target measurement depends on: LVGL is
enabled in the debug preset, the map renderer consumes flash-resident tile
data, and the instrumentation reports FPS, flush, handler-load, resource, and error
counters.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
LVGL_PORT = REPO_ROOT / "Application" / "port" / "lvgl_port.c"
MAP_BENCHMARK = REPO_ROOT / "Application" / "map" / "map_benchmark.c"
MAP_TILES_HEADER = REPO_ROOT / "Application" / "map" / "ytrace_map_tiles.h"
MAP_TILES_SOURCE = REPO_ROOT / "Application" / "map" / "ytrace_map_tiles.c"
OPTIONS = REPO_ROOT / "cmake" / "options.cmake"
SOURCES = REPO_ROOT / "cmake" / "sources.cmake"
CMAKELISTS = REPO_ROOT / "CMakeLists.txt"
PRESETS = REPO_ROOT / "CMakePresets.json"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def require(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as exc:
        fail(f"cannot read {path.relative_to(REPO_ROOT)}: {exc}")


def defines(source: str) -> dict[str, int]:
    values: dict[str, int] = {}
    for name, value in re.findall(r"^#define\s+(LVGL_[A-Z0-9_]+)\s+([0-9]+)U?", source, re.MULTILINE):
        values[name] = int(value)
    return values


def function_body(source: str, name: str) -> str:
    match = re.search(rf"(?:static\s+)?[\w\s\*]+\s+{name}\([^)]*\)\s*\{{", source)
    require(match is not None, f"{name}() is missing")

    start = match.end()
    depth = 1
    index = start
    while index < len(source) and depth:
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
        index += 1

    require(depth == 0, f"{name}() body is not balanced")
    return source[start : index - 1]


def verify_debug_lvgl_enabled() -> None:
    options = read_text(OPTIONS)
    require(
        'option(Y_TRACE_ENABLE_LVGL "Build and start the LVGL UI path" ON)' in options,
        "Y_TRACE_ENABLE_LVGL must default ON for the benchmark firmware",
    )

    try:
        presets = json.loads(read_text(PRESETS))
    except json.JSONDecodeError as exc:
        fail(f"CMakePresets.json is not valid JSON: {exc}")

    debug = next((item for item in presets.get("configurePresets", []) if item.get("name") == "debug"), None)
    require(debug is not None, "debug configure preset is missing")
    require(
        debug.get("cacheVariables", {}).get("Y_TRACE_ENABLE_LVGL") == "ON",
        "debug preset must build the LVGL benchmark path",
    )


def verify_lvgl_port_contract() -> None:
    source = read_text(LVGL_PORT)
    constants = defines(source)

    require(constants.get("LVGL_HOR_RES") == 240, "benchmark must target 240 px LCD width")
    require(constants.get("LVGL_VER_RES") == 320, "benchmark must target 320 px LCD height")
    require(constants.get("LVGL_PERF_PERIOD_MS") == 500, "performance reporting interval must stay at 500 ms")
    require('#include "map/map_benchmark.h"' in source, "LVGL port must include the map benchmark interface")

    stats_match = re.search(r"typedef\s+struct\s*\{(?P<body>.*?)\}\s+lvgl_perf_stats_t;", source, re.S)
    require(stats_match is not None, "lvgl_perf_stats_t is missing")
    stats_body = stats_match.group("body")
    for field in ("frames", "flushes", "pixels", "max_flush_pixels", "flush_ms", "handler_ms", "errors"):
        require(re.search(rf"\buint32_t\s+{field}\s*;", stats_body), f"perf stats must include {field}")

    record_flush = function_body(source, "lvgl_record_flush")
    require("lvgl_perf_stats.flushes++" in record_flush, "flush counter is not incremented")
    require("lvgl_perf_stats.pixels += px_cnt" in record_flush, "pixel counter is not accumulated")
    require("lv_disp_flush_is_last(disp_drv)" in record_flush, "frame counter must use LVGL last-flush signal")
    require("lvgl_perf_stats.frames++" in record_flush, "frame counter is not incremented")

    flush_cb = function_body(source, "lvgl_flush_cb")
    for fragment in (
        "area->x2 < 0",
        "area->y2 < 0",
        "area->x1 >= LVGL_HOR_RES",
        "area->y1 >= LVGL_VER_RES",
        "clipped.x1 = 0",
        "clipped.y1 = 0",
        "clipped.x2 = LVGL_HOR_RES - 1",
        "clipped.y2 = LVGL_VER_RES - 1",
        "contiguous",
        "drv_lcd_write_bytes(",
        "drv_lcd_write_bytes_async",
        "lvgl_flush_done_cb",
        "lvgl_perf_stats.errors++",
    ):
        require(fragment in flush_cb, f"flush callback must preserve '{fragment}'")

    perf_cb = function_body(source, "lvgl_perf_timer_cb")
    for fragment in (
        "fps_x10",
        "frames * 10000U / elapsed_ms",
        ".fps_x10",
        ".handler_load_pct",
        ".flushes",
        ".pixels",
        ".avg_flush_ms",
        ".errors",
        ".draw_buffer_bytes",
        "app_map_benchmark_update_metrics(&metrics)",
        "handler_ms * 100U / elapsed_ms",
    ):
        require(fragment in perf_cb, f"perf timer must preserve '{fragment}'")

    create_screen = function_body(source, "lvgl_create_screen")
    for fragment in (
        "app_map_benchmark_create(scr, lvgl_draw_buffer_bytes)",
        "lv_timer_create(lvgl_perf_timer_cb, LVGL_PERF_PERIOD_MS",
    ):
        require(fragment in create_screen, f"benchmark screen must preserve '{fragment}'")


def verify_map_renderer_contract() -> None:
    source = read_text(MAP_BENCHMARK)
    header = read_text(MAP_TILES_HEADER)
    tile_source = read_text(MAP_TILES_SOURCE)

    require('#include "map/ytrace_map_tiles.h"' in source, "map renderer must consume the tile resource header")
    require("YTRACE_MAP_TILE_TOTAL_BYTES" in source, "resource reporting must use tile metadata")
    require("ytrace_map_tile_pixels(tile_index)" in source, "renderer must parse tile pixel indices from flash data")
    require("ytrace_map_tiles_palette" in source, "renderer must convert tile palette entries")
    require("MAP_BENCHMARK_ANIM_PERIOD_MS 16" in source, "map pan timer must preserve a 60 Hz target cadence")
    require("lv_obj_invalidate(map_state.map)" in source, "map pan timer must invalidate the map object")
    require("void app_map_benchmark_pan(int32_t dx, int32_t dy)" in source, "explicit pan API is missing")
    require("app_map_benchmark_pan(3, 1)" in source, "auto-pan timer must use the pan API")
    require("map_state.offset_x = map_wrap_offset" in source, "map must move horizontally")
    require("map_state.offset_y = map_wrap_offset" in source, "map must move vertically")
    require("rt_kprintf(\"map_bench:" in source, "benchmark must log serial resource/FPS samples")
    require("app_map_benchmark_estimated_ram_bytes" in source, "RAM estimate API is missing")
    require("app_map_benchmark_tile_flash_bytes" in source, "tile flash byte API is missing")
    require("map_draw_navigation_overlay" in source, "map must draw a navigation/map semantics overlay")
    require("map_draw_position_marker" in source, "map must draw an obvious current-position marker")
    require("map_draw_compass" in source, "map must draw an obvious heading/compass cue")
    require("map_draw_screen_line" in source, "map must draw road/route line primitives")
    require("lv_draw_line(" in source, "map must use LVGL line drawing for roads and route")
    require("map_draw_tile_rect" in source, "tile renderer must coalesce same-color source pixels into rectangles")
    require("map_visible_src_range" in source, "tile renderer must skip source pixels outside the visible map area")
    require("palette_colors" in source and "map_init_palette_cache" in source, "tile renderer must cache converted palette colors")

    draw_event = function_body(source, "map_draw_event_cb")
    require("map_draw_navigation_overlay(draw_ctx, &coords)" in draw_event, "map overlay must be drawn after tile background")
    require("map_draw_tile(draw_ctx, &coords" in draw_event, "tile renderer must receive the visible map clip area")

    tile_draw = function_body(source, "map_draw_tile")
    require("map_visible_src_range" in tile_draw, "tile draw must calculate visible source ranges before drawing")
    require("drawn_rows" in tile_draw, "tile draw must track merged source pixels")
    require("rect_x_end" in tile_draw and "rect_y_end" in tile_draw, "tile draw must merge two-dimensional same-color rectangles")
    require("map_draw_tile_rect(" in tile_draw, "tile draw must emit coalesced rectangles instead of per-pixel rectangles")

    tile_rect = function_body(source, "map_draw_tile_rect")
    require("map_state.palette_colors" in tile_rect, "tile rect drawing must use cached LVGL colors")

    compass = function_body(source, "map_draw_compass")
    for fragment in ("24, 292", "24, 300", "24, 278", "18, 288", "30, 288"):
        require(fragment in compass, f"compass/heading cue must stay in the lower-left corner: '{fragment}'")

    overlay = function_body(source, "map_draw_navigation_overlay")
    for fragment in (
        "0x4A90E2",
        "0xF7F5EA",
        "0xE53935",
        "map_draw_compass",
        "map_draw_position_marker",
        "map_draw_poi",
    ):
        require(fragment in overlay, f"navigation overlay must preserve visible map cue '{fragment}'")
    require("0x2F80ED" in source, "current-position marker must use a distinct GPS blue")

    for name, value in (
        ("YTRACE_MAP_TILE_WIDTH_PX", "16U"),
        ("YTRACE_MAP_TILE_HEIGHT_PX", "16U"),
        ("YTRACE_MAP_TILE_COUNT", "64U"),
        ("YTRACE_MAP_TILE_TOTAL_BYTES", "16416U"),
    ):
        require(f"#define {name}" in header and value in header, f"tile metadata must define {name} as {value}")

    require("#define MAP_BENCHMARK_GRID_W         8" in source, "map world must be 8 tiles wide")
    require("#define MAP_BENCHMARK_GRID_H         8" in source, "map world must be 8 tiles high")

    require("YTRACE_MAP_TILE_FORMAT_INDEX8_RGB565" in header, "tile format must be indexed RGB565")
    require("const uint8_t ytrace_map_tiles_pixels" in tile_source, "tile source must define pixel index data")
    require("const uint16_t ytrace_map_tiles_palette" in tile_source, "tile source must define RGB565 palette data")
    require("const ytrace_map_tile_resource_t ytrace_map_tiles" in tile_source, "tile source must expose metadata")
    require("const uint8_t *ytrace_map_tile_pixels" in tile_source, "tile accessor is missing")


def verify_build_integration() -> None:
    sources = read_text(SOURCES)
    cmakelists = read_text(CMAKELISTS)

    for path in ("Application/map/map_benchmark.c", "Application/map/ytrace_map_tiles.c", "Application/port/lvgl_port.c"):
        require(path in sources, f"{path} must be included when LVGL is enabled")

    for path in ("Application/map/map_benchmark.c", "Application/map/ytrace_map_tiles.c"):
        require(path in cmakelists, f"{path} must be built with the optimized LVGL source set")


def main() -> int:
    verify_debug_lvgl_enabled()
    verify_lvgl_port_contract()
    verify_map_renderer_contract()
    verify_build_integration()
    print("PASS: map tile benchmark static contract verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
