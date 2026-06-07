# Host C Logic Tests

These tests run on a PC before flashing firmware. They use the standalone Unity copy under `Test/unity/` and cover host-buildable C logic only; they do not require RT-Thread runtime, target hardware, serial access, flashing, or debug probes.

From the repository root:

```sh
cmake -S Test -B build/host-test
cmake --build build/host-test
ctest --test-dir build/host-test --output-on-failure
```

From this `Test/` directory:

```sh
cmake --preset host-test
cmake --build --preset host-test
ctest --preset host-test --output-on-failure
```

Add new host tests by explicitly listing only host-safe source files in `Test/CMakeLists.txt`. Do not import the firmware `Y_TRACE_SOURCES` list; it contains MCU, RT-Thread, startup, and board-driver code that is not suitable for PC tests.

The Unity files in `Test/unity/` are a dedicated copy downloaded for this test lane. Do not include Unity from `Middlewares/lvgl/tests/unity`; that copy belongs to the LVGL third-party tree.

The previous Python scripts in this directory were static source-contract checks. The first host-test lane intentionally replaces them with compiled C logic tests rather than preserving those static checks.
