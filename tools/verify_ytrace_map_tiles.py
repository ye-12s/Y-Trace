#!/usr/bin/env python3
"""Verify the static Y-Trace map tile resource metadata."""

from __future__ import annotations

import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
HEADER = ROOT / "Application" / "map" / "ytrace_map_tiles.h"
SOURCE = ROOT / "Application" / "map" / "ytrace_map_tiles.c"


def read_text(path: pathlib.Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        print(f"missing required file: {path.relative_to(ROOT)}", file=sys.stderr)
        raise SystemExit(1)


def find_define(text: str, name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+\(?([0-9]+)U?\)?$", text, re.MULTILINE)
    if not match:
        print(f"missing integer define: {name}", file=sys.stderr)
        raise SystemExit(1)
    return int(match.group(1))


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*", "", text)


def find_array_values(text: str, name: str) -> list[int]:
    clean = strip_comments(text)
    match = re.search(rf"{re.escape(name)}\s*\[[^\]]*\]\s*=\s*\{{(.*?)\}};", clean, re.DOTALL)
    if not match:
        print(f"missing array initializer: {name}", file=sys.stderr)
        raise SystemExit(1)

    values: list[int] = []
    for token in match.group(1).replace("\n", " ").split(","):
        token = token.strip()
        if not token:
            continue
        try:
            values.append(int(token, 0))
        except ValueError:
            print(f"non-literal array value in {name}: {token}", file=sys.stderr)
            raise SystemExit(1)
    return values


def tile_rect_count(tile: tuple[int, ...], width: int, height: int) -> int:
    drawn_rows = [0] * height
    rect_count = 0

    def mask(x_start: int, x_end: int) -> int:
        value = 0
        for x in range(x_start, x_end + 1):
            value |= 1 << x
        return value

    def can_extend(y: int, x_start: int, x_end: int, color: int) -> bool:
        row_mask = mask(x_start, x_end)
        if drawn_rows[y] & row_mask:
            return False
        return all(tile[(y * width) + x] == color for x in range(x_start, x_end + 1))

    for y in range(height):
        x = 0
        while x < width:
            if drawn_rows[y] & (1 << x):
                x += 1
                continue

            color = tile[(y * width) + x]
            x_end = x
            while x_end + 1 < width and can_extend(y, x, x_end + 1, color):
                x_end += 1

            y_end = y
            while y_end + 1 < height and can_extend(y_end + 1, x, x_end, color):
                y_end += 1

            row_mask = mask(x, x_end)
            for drawn_y in range(y, y_end + 1):
                drawn_rows[drawn_y] |= row_mask

            rect_count += 1
            x = x_end + 1

    return rect_count


def main() -> int:
    header = read_text(HEADER)
    source = read_text(SOURCE)

    width = find_define(header, "YTRACE_MAP_TILE_WIDTH_PX")
    height = find_define(header, "YTRACE_MAP_TILE_HEIGHT_PX")
    tile_count = find_define(header, "YTRACE_MAP_TILE_COUNT")
    palette_count = find_define(header, "YTRACE_MAP_TILE_PALETTE_COLOR_COUNT")
    bytes_per_pixel = find_define(header, "YTRACE_MAP_TILE_BYTES_PER_PIXEL")
    pixel_bytes = find_define(header, "YTRACE_MAP_TILE_PIXEL_BYTES")
    palette_bytes = find_define(header, "YTRACE_MAP_TILE_PALETTE_BYTES")
    total_bytes = find_define(header, "YTRACE_MAP_TILE_TOTAL_BYTES")

    pixels = find_array_values(source, "ytrace_map_tiles_pixels")
    palette = find_array_values(source, "ytrace_map_tiles_palette")

    expected_pixels = width * height * tile_count
    expected_palette = palette_count * 2
    expected_total = expected_pixels * bytes_per_pixel + expected_palette
    tile_area = width * height
    chunks = [tuple(pixels[i : i + tile_area]) for i in range(0, len(pixels), tile_area)] if tile_area else []
    rect_counts = [tile_rect_count(chunk, width, height) for chunk in chunks]
    avg_rects_x10 = (sum(rect_counts) * 10 + len(rect_counts) // 2) // len(rect_counts) if rect_counts else 0

    failures: list[str] = []
    if tile_count < 64:
        failures.append(f"tile count {tile_count} < 64")
    if pixel_bytes != expected_pixels:
        failures.append(f"pixel bytes {pixel_bytes} != {expected_pixels}")
    if palette_bytes != expected_palette:
        failures.append(f"palette bytes {palette_bytes} != {expected_palette}")
    if len(pixels) != expected_pixels:
        failures.append(f"pixel count {len(pixels)} != {expected_pixels}")
    if len(palette) != palette_count:
        failures.append(f"palette count {len(palette)} != {palette_count}")
    if total_bytes != expected_total:
        failures.append(f"total bytes {total_bytes} != {expected_total}")
    if pixels and max(pixels) >= palette_count:
        failures.append(f"pixel index {max(pixels)} exceeds palette count {palette_count}")
    if len(chunks) != tile_count:
        failures.append(f"tile chunks {len(chunks)} != {tile_count}")
    if len(set(chunks)) < tile_count:
        failures.append(f"unique tile chunks {len(set(chunks))} != {tile_count}")
    if rect_counts and max(rect_counts) > 20:
        failures.append(f"max tile rectangle count {max(rect_counts)} > 20")
    if rect_counts and avg_rects_x10 > 120:
        failures.append(f"average tile rectangle count {avg_rects_x10 / 10:.1f} > 12.0")

    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
        return 1

    print(
        f"verified {tile_count} tiles, {width}x{height}px, "
        f"{len(pixels)} pixel bytes + {len(palette) * 2} palette bytes = {total_bytes} bytes, "
        f"tile rectangles avg {avg_rects_x10 / 10:.1f} max {max(rect_counts)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
