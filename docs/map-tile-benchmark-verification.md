# Map Tile Benchmark Verification

This firmware exposes an LVGL bicycle-computer map benchmark. The map renderer
reads a 64-tile indexed RGB565 catalog from firmware flash, scales the tiles to
the 240x320 display, pans continuously across an 8x8 tile world, and reports
display/resource metrics. The tile layer is intentionally backed by flash data,
while the foreground draws navigation-map cues: water, road hierarchy, a red
route, POIs, a centered current-position marker, and a compass/heading cue. Host
checks can only verify that the benchmark path, map semantics, and
instrumentation are present; FPS and resource claims must be measured on the
AT32F403A target with the ST7789 LCD path active.

## Host Static Check

Run this from the repository root:

```sh
python3 Test/map_tile_benchmark_static_check.py
```

The check passes when:

- `Y_TRACE_ENABLE_LVGL` defaults on and the `debug` preset enables it.
- The benchmark keeps the 240x320 target geometry.
- `Application/map/ytrace_map_tiles.c` provides 64 unique 16x16 indexed RGB565
  tiles: `16384 B` of indices plus a `32 B` RGB565 palette.
- `Application/map/map_benchmark.c` consumes the tile catalog, pans both axes,
  uses an 8x8 tile world, and invalidates the map object on a 16 ms cadence.
- The map draw path overlays visible navigation semantics: blue water, light
  roads, a red route, POIs, a blue current-position marker, and a compass cue.
- The performance timer still reports every 500 ms.
- FPS, flush, handler load, tile bytes, draw-buffer bytes, RAM estimate, tile
  flash bytes, and error counters remain wired into the LVGL benchmark labels
  and `map_bench:` serial logs.
- LCD flushes are clipped to panel bounds and counted only when LVGL marks the
  final flush for a frame.

## Build Check

Run:

```sh
cmake --preset debug
cmake --build --preset debug
```

Acceptance criteria:

- Configuration completes with the ARM GCC toolchain.
- Firmware build completes without errors.
- The build emits `build/cmake/Y-Trace.elf`, `Y-Trace.hex`, `Y-Trace.bin`, and
  `Y-Trace.map`.
- The final size report is captured with the benchmark result so FPS is tied to
  the measured firmware image.

## On-Target Measurement Checklist

1. Flash the exact debug build under test:

   ```sh
   jf flash /home/ans/workspace/Y-Trace/firmware/Y-Trace/build/cmake/Y-Trace.hex --chip AT32F403AVGT7
   ```

2. Reset the board and open the RT-Thread serial console.
3. Confirm boot logs show the LVGL path, draw-buffer selection, and no startup
   allocation failure.
4. Confirm the display reads as a map at a glance: moving tile background,
   distinct roads, blue water, red route, POI dots, and the current-position
   marker near the route, plus a compass cue.
5. Let the benchmark run for at least 30 seconds after the scene first appears.
6. Record at least 20 consecutive samples from the on-screen labels or matching
   `map_bench:` serial diagnostics:
   - `fps=<x.y>`
   - `handler_load=<percent>%`
   - `flushes=<count>`
   - `pixels=<count>`
   - `max_flush=<pixels>`
   - `avg_flush_ms=<ms>`
   - `tile_bytes=<bytes>`
   - `draw_buf=<bytes>`
   - `est_ram=<bytes>`
   - `tile_flash=<bytes>`
   - `errors=<count>`
7. Record memory/resource context:
   - Firmware size report from `cmake --build --preset debug`.
   - Whether SRAM preparation selected dual external DMA buffers or the fallback
     draw buffer.
   - Tile catalog verifier output from `python3 tools/verify_ytrace_map_tiles.py`.
   - Expected tile flash resource: `16416 B`.
   - RT-Thread heap/free-memory output if available in the shell.
   - Any LCD DMA, SPI, or LVGL error logs.

## On-Target Acceptance Criteria

- Warmed median FPS over the 30 second run is at least the target defined by the
  feature request.
- Minimum FPS across the recorded samples is no lower than 90% of that target.
- `errors` remains `0` for the full run.
- Handler-load label remains below the feature request's CPU budget.
- No visible tearing, stuck tiles, blank frames, or unexpected reset occurs.
- Draw-buffer mode and firmware size are reported with the result.
- Any target-specific variance is documented: probe power, LCD module, clock
  configuration, compiler preset, and whether external SRAM was active.
