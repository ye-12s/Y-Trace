# Repository Guidelines

## Project Structure & Module Organization

This repository is embedded firmware for an AT32F403A Cortex-M4 target using RT-Thread, Artery BSP files, and a CMake/ARM GCC build. Application code lives in `Application/`: `main.c` is the idle entry point, `app_init.c` registers RT-Thread init hooks, `sample/` contains feature demos, and `utils/` contains reusable helpers such as `minmea`. Board support and drivers live in `Board/`, with device drivers under `Board/Drivers/` and linker/startup/SVD files under `Board/misc/`. Third-party and vendor components are under `Middlewares/` and `libraries/`. Tests live in `Test/` when present.

## Build, Test, and Development Commands

Use the CMake presets from the repository root:

- `cmake --preset debug`: configures the Debug build in `build/cmake/`.
- `cmake --build --preset debug`: builds the firmware.
- `cmake --build --preset debug --target clean`: removes generated CMake build outputs.
- `jf flash ${workspaceFolder}/build/cmake/Y-Trace.hex --chip AT32F403AVGT7`: downloads the built firmware to the target. This is the repository's VS Code `download` task command and depends on a fresh Debug build.

The Debug CMake preset uses ARM GCC, C11, hard-float Cortex-M4 settings, and `Board/misc/AT32F403AxG_FLASH.ld`.
For command-line flashing from the repository root, replace `${workspaceFolder}` with the absolute repository path:
`jf flash /home/ans/workspace/Y-Trace/firmware/Y-Trace/build/cmake/Y-Trace.hex --chip AT32F403AVGT7`.
USB probe access may require elevated permissions; if the first flash attempt reports a probe/USB open error, rerun the same `jf flash` command with the required approval rather than switching tools.
For RTT log capture, default to the repository tool wrapper: `jf rtt --chip AT32F403AVGT7 --search-range 0x20000000:0x18000 --log <path>`. Use this before falling back to raw `JLinkRTTClient`.

## Coding Style & Naming Conventions

Format C with `.clang-format`: Microsoft base style, 4-space indentation, no tabs, Linux braces, no column limit, and no include sorting. Keep C modules and drivers in lower snake case (`drv_uart.c`). Use `drv_` prefixes for board drivers and `*_if` for port adapters.

## Testing Guidelines

Tests use RT-Thread `utest` and `UTEST_TC_EXPORT`. Add new tests under `Test/` as C sources, name functions `test_<module>_<behavior>`, and export cases with dotted IDs. Some tests require target hardware or storage media, especially FatFS and SDIO tests. At minimum, run a CMake build before submitting firmware changes; run relevant `utest` suites on hardware when touching drivers or storage.

## Commit & Pull Request Guidelines

Recent history uses short, descriptive commit subjects, often in Chinese, for example `添加gps的基础解析功能`. Keep subjects concise and focused on the intent. For pull requests, include a short change summary, affected modules, build result, hardware/test evidence, and any known limitations. Link related issues when available, and include screenshots or serial logs when UI, display, or runtime behavior changes.

## Security & Configuration Tips

Do not commit private device paths, generated binaries, or local tool state. Keep target-specific configuration in CMake files, `.vscode/`, and board headers; document hardware assumptions when changing flash layout, upload settings, or peripheral pin mappings.
