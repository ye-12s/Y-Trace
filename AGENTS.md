# Repository Guidelines

## Project Structure & Module Organization

This repository is embedded firmware for an AT32F403A Cortex-M4 target using RT-Thread, Artery BSP files, and a CMake/ARM GCC build. Application code lives in `Application/`: `main.c` is the idle entry point, `app_init.c` registers RT-Thread init hooks, `sample/` contains feature demos, and `utils/` contains reusable helpers such as `minmea`. Board support and drivers live in `Board/`, with device drivers under `Board/Drivers/` and linker/startup/SVD files under `Board/misc/`. Third-party and vendor components are under `Middlewares/` and `libraries/`. Tests live in `Test/` when present.

## Build, Test, and Development Commands

Use the CMake presets from the repository root:

- `cmake --preset debug`: configures the Debug build in `build/cmake/`.
- `cmake --build --preset debug`: builds the firmware.
- `cmake --build --preset debug --target clean`: removes generated CMake build outputs.

The Debug CMake preset uses ARM GCC, C11, hard-float Cortex-M4 settings, and `Board/misc/AT32F403AxG_FLASH.ld`.

## Coding Style & Naming Conventions

Format C with `.clang-format`: Microsoft base style, 4-space indentation, no tabs, Linux braces, no column limit, and no include sorting. Keep C modules and drivers in lower snake case (`drv_uart.c`). Use `drv_` prefixes for board drivers and `*_if` for port adapters.

## Testing Guidelines

Tests use RT-Thread `utest` and `UTEST_TC_EXPORT`. Add new tests under `Test/` as C sources, name functions `test_<module>_<behavior>`, and export cases with dotted IDs. Some tests require target hardware or storage media, especially FatFS and SDIO tests. At minimum, run a CMake build before submitting firmware changes; run relevant `utest` suites on hardware when touching drivers or storage.

## Commit & Pull Request Guidelines

Recent history uses short, descriptive commit subjects, often in Chinese, for example `添加gps的基础解析功能`. Keep subjects concise and focused on the intent. For pull requests, include a short change summary, affected modules, build result, hardware/test evidence, and any known limitations. Link related issues when available, and include screenshots or serial logs when UI, display, or runtime behavior changes.

## Security & Configuration Tips

Do not commit private device paths, generated binaries, or local tool state. Keep target-specific configuration in CMake files, `.vscode/`, and board headers; document hardware assumptions when changing flash layout, upload settings, or peripheral pin mappings.
