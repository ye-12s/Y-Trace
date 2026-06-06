# CMake Migration Notes

This project was migrated from the EIDE Debug target to a CMake/ARM GCC build. The old EIDE files were used as the build-parity source of truth before removal.

## Build Commands

```sh
cmake --preset debug
cmake --build --preset debug
```

Expected outputs are generated under `build/cmake/`:

- `Y-Trace.elf`
- `Y-Trace.map`
- `Y-Trace.bin`
- `Y-Trace.hex`
- `compile_commands.json`

## EIDE To CMake Parity

| EIDE Debug input | Old source | CMake representation |
| --- | --- | --- |
| Source roots | `.eide/eide.yml:6-11` lists `libraries`, `Board`, `Application`, `Middlewares`, `Test` | `cmake/sources.cmake` explicitly lists firmware C and ASM files from those roots |
| Output directory | `.eide/eide.yml:17` used `build` | `CMakePresets.json` uses `build/cmake` |
| Defines | `.eide/eide.yml:23-27` | `cmake/options.cmake` sets `_DEBUG`, `USE_STDPERIPH_DRIVER`, `AT_START_F403A_V1`, `AT32F403AVGT7` |
| Include paths | `.eide/eide.yml:28-40` | `cmake/options.cmake` keeps those include paths and adds `Board/Drivers` for cross-driver headers |
| Exclusions | `.eide/eide.yml:42-66` | `cmake/sources.cmake` omits CMSIS-DSP, non-Cortex-M4 RT-Thread ports, non-GCC startup alternatives, `Board/misc/startup_at32f403a_407.s`, `divsi3.S`, LittleFS, and FatFS documents |
| C standard | `.eide/eide.yml:122-124` | `CMakeLists.txt` sets C11 |
| Function/data sections | `.eide/eide.yml:129-130` | `CMakeLists.txt` sets `-ffunction-sections` and `-fdata-sections` |
| Debug optimization and symbols | `.eide/eide.yml:131-136` | `CMakeLists.txt` sets `-Og` and `-g` |
| Warning level | `.eide/eide.yml:132` | `CMakeLists.txt` sets `-Wall` for C |
| CPU and ABI | `.eide/eide.yml:112-115,133-135` | `cmake/options.cmake` sets `-mthumb`, `-mcpu=cortex-m4`, `-mfpu=fpv4-sp-d16`, `-mfloat-abi=hard` |
| Newlib nano | `.eide/eide.yml:137` | `CMakeLists.txt` links with `--specs=nano.specs` |
| Link library flags | `.eide/eide.yml:138-142` | `CMakeLists.txt` links with `-lm` and `--gc-sections` |
| Linker script | `.eide/eide.yml:143`; `Board/misc/AT32F403AxG_FLASH.ld:21-35` | `cmake/options.cmake` uses `Board/misc/AT32F403AxG_FLASH.ld` |
| Startup object | EIDE excludes MDK/IAR startup and `Board/misc/startup_at32f403a_407.s` | `cmake/sources.cmake` uses `libraries/cmsis/cm4/device_support/startup/gcc/startup_at32f403a_407.s` |
| ASM flags | `.eide/eide.yml:119-120` | `CMakeLists.txt` sets `-Wa,-mimplicit-it=always` for ASM |
| Firmware artifacts | EIDE GCC output format was ELF | CMake emits ELF plus map, bin, and hex artifacts |

Two CMake-only compatibility details are present because this migration builds on a case-sensitive Linux filesystem with GCC 14:

- Some source files include driver headers as `drivers/...`; CMake copies `Board/Drivers/*.h` into `build/cmake/include/drivers/` so the source remains unchanged.
- `Application/utils/minmea/minmea.c` uses `timegm`; GCC 14 treats the implicit declaration more strictly than the older EIDE GCC configuration, so CMake downgrades that single source's implicit-declaration diagnostic without modifying vendor utility code.
- `Board/syscall.c` provides RT-Thread newlib allocator hooks. CMake renames those hooks at compile time and `cmake/newlib_heap_bridge.c` wraps current GCC 14 newlib reentrant allocator references back to the renamed project hooks without editing board or middleware source.

## Upload And Debug Metadata Preserved From EIDE

These settings were recorded before deleting `.eide/`. They are not implemented by this CMake migration.

| Workflow | EIDE setting |
| --- | --- |
| Default uploader | OpenOCD |
| OpenOCD base address | `0x08000000` |
| OpenOCD interface | `atlink` |
| OpenOCD target | `at32f403axG` |
| J-Link speed | `8000` |
| J-Link protocol type | `1` |
| VS Code debugger | `cortex-debug` with OpenOCD |
| Debug entry | `main` |
| Debug config files | `interface/atlink.cfg`, `target/at32f403axG.cfg` |
| Toolchain prefix | `arm-none-eabi` |

Flash/debug replacement is out of scope for this migration. Add CMake/OpenOCD or J-Link tasks as a follow-up if hardware programming needs to be restored from CMake.

## Deletion Gate Evidence

- Source parity: CMake sources were selected from EIDE source roots and exclusions.
- Flag parity: CMake options preserve Debug target defines, include paths, standards, CPU/FPU/ABI flags, and link flags.
- Linker parity: CMake uses the existing AT32F403AxG linker script and GCC startup assembly.
- Artifact parity: CMake emits ELF, map, bin, and hex artifacts when the ARM GCC toolchain is available.
- Stale-reference boundary: active config files should not contain old EIDE commands or absolute EIDE toolchain paths.
- Flash/debug boundary: upload/debug metadata is recorded above and remains follow-up work.

## Rollback Guidance

If hardware flashing/debugging depends on removed EIDE metadata, restore the old IDE files from git history and use the preserved settings above as the reference for a CMake/OpenOCD or J-Link replacement.
