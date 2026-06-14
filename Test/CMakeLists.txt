cmake_minimum_required(VERSION 3.20)

project(YTraceHostTests LANGUAGES C)

enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(Y_TRACE_APP_DIR ${PROJECT_SOURCE_DIR}/../Application)
set(Y_TRACE_UNITY_DIR ${PROJECT_SOURCE_DIR}/unity/src)

add_library(unity STATIC
    unity/src/unity.c
)
target_include_directories(unity PUBLIC ${Y_TRACE_UNITY_DIR})
target_compile_options(unity PRIVATE -Wall -Wextra)

add_executable(test_minmea
    test_minmea.c
    ../Application/utils/minmea/minmea.c
)
target_include_directories(test_minmea PRIVATE ${Y_TRACE_APP_DIR})
target_compile_definitions(test_minmea PRIVATE _GNU_SOURCE)
target_compile_options(test_minmea PRIVATE -Wall -Wextra)
target_link_libraries(test_minmea PRIVATE unity)
add_test(NAME test_minmea COMMAND test_minmea)

add_executable(test_ytrace_map_tiles
    test_ytrace_map_tiles.c
    ../Application/map/ytrace_map_tiles.c
)
target_include_directories(test_ytrace_map_tiles PRIVATE ${Y_TRACE_APP_DIR})
target_compile_options(test_ytrace_map_tiles PRIVATE -Wall -Wextra)
target_link_libraries(test_ytrace_map_tiles PRIVATE unity)
add_test(NAME test_ytrace_map_tiles COMMAND test_ytrace_map_tiles)

add_executable(test_vfs_core
    test_vfs_core.c
    ../Application/storage/vfs.c
    ../Application/storage/vfs_cache.c
)
target_include_directories(test_vfs_core PRIVATE ${Y_TRACE_APP_DIR})
target_compile_definitions(test_vfs_core PRIVATE
    Y_TRACE_VFS_FATFS_SOURCE_PATH="${PROJECT_SOURCE_DIR}/../Application/storage/vfs_fatfs.c"
    Y_TRACE_VFS_SELFTEST_SOURCE_PATH="${PROJECT_SOURCE_DIR}/../Application/storage/vfs_selftest.c"
    Y_TRACE_CMAKE_OPTIONS_PATH="${PROJECT_SOURCE_DIR}/../cmake/options.cmake"
    Y_TRACE_CMAKE_SOURCES_PATH="${PROJECT_SOURCE_DIR}/../cmake/sources.cmake")
target_compile_options(test_vfs_core PRIVATE -Wall -Wextra)
target_link_libraries(test_vfs_core PRIVATE unity)
add_test(NAME test_vfs_core COMMAND test_vfs_core)

add_executable(test_vfs_cache
    test_vfs_cache.c
    ../Application/storage/vfs.c
    ../Application/storage/vfs_cache.c
)
target_include_directories(test_vfs_cache PRIVATE ${Y_TRACE_APP_DIR})
target_compile_options(test_vfs_cache PRIVATE -Wall -Wextra)
target_link_libraries(test_vfs_cache PRIVATE unity)
add_test(NAME test_vfs_cache COMMAND test_vfs_cache)

add_executable(test_ws2812b_driver_contract
    test_ws2812b_driver_contract.c
)
target_compile_definitions(test_ws2812b_driver_contract PRIVATE
    Y_TRACE_WS2812B_DRIVER_PATH="${PROJECT_SOURCE_DIR}/../Board/Drivers/drv_ws2812b.c"
    Y_TRACE_WS2812B_HEADER_PATH="${PROJECT_SOURCE_DIR}/../Board/Drivers/drv_ws2812b.h"
    Y_TRACE_APP_INIT_PATH="${PROJECT_SOURCE_DIR}/../Application/app_init.c")
target_compile_options(test_ws2812b_driver_contract PRIVATE -Wall -Wextra)
target_link_libraries(test_ws2812b_driver_contract PRIVATE unity)
add_test(NAME test_ws2812b_driver_contract COMMAND test_ws2812b_driver_contract)

add_executable(test_imu_driver_contract
    test_imu_driver_contract.c
)
target_compile_definitions(test_imu_driver_contract PRIVATE
    Y_TRACE_LSM6DSM_HEADER_PATH="${PROJECT_SOURCE_DIR}/../Board/Drivers/drv_lsm6dsm.h"
    Y_TRACE_LSM6DSM_SOURCE_PATH="${PROJECT_SOURCE_DIR}/../Board/Drivers/drv_lsm6dsm.c"
    Y_TRACE_LIS3MDL_HEADER_PATH="${PROJECT_SOURCE_DIR}/../Board/Drivers/drv_lis3mdltr.h"
    Y_TRACE_LIS3MDL_SOURCE_PATH="${PROJECT_SOURCE_DIR}/../Board/Drivers/drv_lis3mdltr.c"
    Y_TRACE_IMU_HEADER_PATH="${PROJECT_SOURCE_DIR}/../Board/Drivers/drv_imu.h"
    Y_TRACE_IMU_SOURCE_PATH="${PROJECT_SOURCE_DIR}/../Board/Drivers/drv_imu.c"
    Y_TRACE_APP_INIT_PATH="${PROJECT_SOURCE_DIR}/../Application/app_init.c"
    Y_TRACE_CMAKE_SOURCES_PATH="${PROJECT_SOURCE_DIR}/../cmake/sources.cmake"
    Y_TRACE_CMAKE_OPTIONS_PATH="${PROJECT_SOURCE_DIR}/../cmake/options.cmake")
target_compile_options(test_imu_driver_contract PRIVATE -Wall -Wextra)
target_link_libraries(test_imu_driver_contract PRIVATE unity)
add_test(NAME test_imu_driver_contract COMMAND test_imu_driver_contract)
