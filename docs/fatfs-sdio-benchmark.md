# FatFs SDIO SD-card Smoke and Throughput Benchmark

This firmware includes an RT-Thread MSH command for destructive SD-card FatFs
validation:

```text
fatfs_sd_bench [--format] [file_size]
```

`file_size` accepts bytes, `K`, or `M` suffixes. The default is `1M`; values are
clamped to `64K..16M`.

## Scope

- Storage target: SDIO SD card, FatFs drive `0:`.
- FatFs disk glue: `Application/port/diskio.c`.
- Command implementation: `Application/sample/fatfs_sd_bench.c`.
- Build gate: `Y_TRACE_ENABLE_FATFS_SD_BENCH` controls whether the destructive
  MSH command is compiled; it defaults on for bring-up and can be disabled for
  production-like firmware.
- LittleFS, internal MCU flash, and SPI/QSPI external flash are intentionally out
  of scope for this benchmark.

## Destructive behavior

The command formats the SD card only when `f_mount("0:")` fails and `--format`
(or `-f`) was supplied. It also creates or overwrites these test files:

- `0:/YT_BENCH.BIN`
- `0:/YT_BENCH.GPX`

The names intentionally use 8.3 format because `FF_USE_LFN` is disabled in the current FatFs configuration.

Do not run it on a card that contains data you need to preserve. Do not run it
concurrently with future GPX logging, map-tile serving, or any other module that
owns/mounts FatFs drive `0:`; this command is a bring-up benchmark that owns the
mount during its run.

## Build and flash

From the repository root:

```sh
cmake --preset debug
cmake --build --preset debug
jf flash /home/ans/workspace/Y-Trace/firmware/Y-Trace/build/cmake/Y-Trace.hex --chip AT32F403AVGT7
```

To omit the command from production-like builds, configure with
`-DY_TRACE_ENABLE_FATFS_SD_BENCH=OFF`. For lab runs without a serial input path,
`-DY_TRACE_FATFS_SD_BENCH_AUTORUN=ON` starts a destructive `fatfs_sd_bench --format 1M`
run about five seconds after boot and enlarges the RTT up-buffer for evidence
capture; keep it off for normal firmware.

Open the RT-Thread serial console after reset, then run:

```text
fatfs_sd_bench
```

If the card is blank or has an incompatible filesystem, rerun with explicit
format permission:

```text
fatfs_sd_bench --format
```

For a longer limit run, use a larger file:

```text
fatfs_sd_bench 8M
fatfs_sd_bench 16M
```

## What the command verifies

Functional smoke checks:

1. Mount FatFs drive `0:` or, with `--format`, format/remount if required.
2. Create a binary benchmark file.
3. Write deterministic byte patterns and `f_sync` them.
4. Reopen/read/verify the binary file byte-for-byte.
5. Read the binary file sequentially.
6. Read tile-like 4 KiB chunks from pseudo-random offsets.
7. Create and append a GPX-like file, syncing every 128 track points.
8. Reopen/stat the GPX-like file and verify expected size plus header/footer content.

## Provisional performance goals

The command prints `PASS` or `WARN` per metric. `WARN` means the smoke test can
still be functionally valid, but the result is below the first-pass target for
future map-tile and GPX workloads.

| Workload | First-pass target |
| --- | ---: |
| Sequential FatFs read | >= 500 KB/s |
| Tile-like 4 KiB FatFs reads | >= 200 KB/s |
| Sequential FatFs write | >= 200 KB/s |
| GPX append writes | >= 20 KB/s |

## Required serial evidence

Capture the full `fatfs_sd_bench:` output, especially:

- `mount`, optional `mkfs`, and `remount` result codes.
- Volume `fs_type`, cluster size, and free space when available.
- Per-phase `res=<FRESULT>` lines.
- Per-result `bytes`, `elapsed_ms`, `chunk`, `throughput`, threshold, and
  `PASS`/`WARN` classification.
- `gpx_verify` expected size and result code.
- Final summary line with `best_seq_read`, `best_seq_write`, `tile4k`, and
  `gpx_append` throughput.
- `limit_chunk` skip line explaining that 64 KiB chunk testing is skipped when
  the static benchmark buffer is smaller than 64 KiB.

A fully passing smoke run ends with:

```text
fatfs_sd_bench: PASS functional smoke complete; performance_warn=no
```

## Limits of local verification

The CMake build proves the command compiles and is exported into the firmware.
Actual SDIO/FatFs functionality and throughput require the AT32F403A target, SD
card, and serial console. If the final line reports `performance_warn=yes`, the
file I/O smoke test passed but at least one throughput metric missed the
provisional target.
## 2026-06-07 SDIO SD-card hardware result

Captured with the repository default RTT command:

```sh
jf rtt --chip AT32F403AVGT7 --search-range 0x20000000:0x18000 --log /tmp/fatfs_sd_bench_final.log
```

Target/card notes from the log:

- Card detected as SDHC/SDXC.
- Reported capacity: 60906 MB.
- Bus mode before the follow-up 4-line fix: 1-bit (`[SDIO] Using 1-bit bus mode (card limitation)`).
  The hardware has DAT0..DAT3 wired, so this was treated as an SCR/bus-width
  negotiation issue rather than a board limitation.
- FatFs mount result: `res=0`.
- FatFs volume: `fs_type=3`, cluster size `csize=64`, free space `62358752 KB`.

Measured with autorun `fatfs_sd_bench --format 1M`:

| Workload | Chunk | Bytes | Time | Throughput | Result |
| --- | ---: | ---: | ---: | ---: | --- |
| Sequential write | 512 B | 1,048,576 | 2030 ms | 504 KB/s | PASS |
| Sequential read | 512 B | 1,048,576 | 661 ms | 1549 KB/s | PASS |
| Sequential write | 4 KiB | 1,048,576 | 1908 ms | 536 KB/s | PASS |
| Sequential read | 4 KiB | 1,048,576 | 656 ms | 1560 KB/s | PASS |
| Sequential write | 16 KiB | 1,048,576 | 1941 ms | 527 KB/s | PASS |
| Sequential read | 16 KiB | 1,048,576 | 659 ms | 1553 KB/s | PASS |
| Tile-like random read | 4 KiB | 2,097,152 | 1317 ms | 1555 KB/s | PASS |
| GPX append + sync every 128 records | stream | 257,124 | 1329 ms | 188 KB/s | PASS |

The run completed with:

```text
fatfs_sd_bench: PASS functional smoke complete; performance_warn=no
```

A pre-fix hardware run exposed an SDIO multi-block reliability issue: 512 B
write/read passed, but 4 KiB verify failed with `FR_DISK_ERR`. The current
FatFs disk glue therefore loops single-sector transfers for reliability; optimize
and re-validate the lower SDIO multi-block path separately before enabling it for
map-tile workloads.

## 2026-06-07 SDIO 4-bit follow-up result

The SD-card socket is wired with DAT0..DAT3. A follow-up fix first changed the
driver so 4-bit mode is attempted with ACMD6 even if SCR parsing is misleading.
The SCR parser was then fixed to decode the AT SDIO FIFO word order explicitly.
Evidence was captured with `jf rtt`; a cleaned copy of the raw log is committed as `docs/fatfs-sdio-benchmark-rtt-20260607.log`:

```sh
jf rtt --chip AT32F403AVGT7 --search-range 0x20000000:0x18000 --log /tmp/fatfs_sd_bench_scr_fix.log
```

Relevant log lines:

```text
[SDIO] SCR raw=43804502 00000000 parsed=02 45 80 43 00 00 00 00 order=1
[SDIO] SCR: structure=0 sd_spec=3.x+(code=2) bus_widths=0x5 supports 4-bit bus
[SDIO] 4-bit bus mode configured successfully
fatfs_sd_bench: PASS functional smoke complete; performance_warn=no
```

Measured with autorun `fatfs_sd_bench --format 1M` after 4-bit enablement:

| Workload | Chunk | Bytes | Time | Throughput | Result |
| --- | ---: | ---: | ---: | ---: | --- |
| Sequential write | 512 B | 1,048,576 | 1729 ms | 592 KB/s | PASS |
| Sequential read | 512 B | 1,048,576 | 424 ms | 2415 KB/s | PASS |
| Sequential write | 4 KiB | 1,048,576 | 1673 ms | 612 KB/s | PASS |
| Sequential read | 4 KiB | 1,048,576 | 419 ms | 2443 KB/s | PASS |
| Sequential write | 16 KiB | 1,048,576 | 1689 ms | 606 KB/s | PASS |
| Sequential read | 16 KiB | 1,048,576 | 422 ms | 2426 KB/s | PASS |
| Tile-like random read | 4 KiB | 2,097,152 | 844 ms | 2426 KB/s | PASS |
| GPX append + sync every 128 records | stream | 257,124 | 531 ms | 472 KB/s | PASS |

After the SCR parser fix, 4-bit mode improved the map-tile-like read path from
about `1555 KB/s` to `2426 KB/s`, and GPX append from about `188 KB/s` to
`472 KB/s` in this 1 MiB bring-up run.

