option(Y_TRACE_ENABLE_LVGL "Build and start the LVGL UI path" ON)

set(Y_TRACE_DEFINES
    _DEBUG
    USE_STDPERIPH_DRIVER
    AT_START_F403A_V1
    AT32F403AVGT7
)

set(Y_TRACE_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/libraries/cmsis/cm4/core_support
    ${CMAKE_CURRENT_SOURCE_DIR}/libraries/cmsis/cm4/device_support
    ${CMAKE_CURRENT_SOURCE_DIR}/libraries/drivers/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Board
    ${CMAKE_CURRENT_SOURCE_DIR}/Board/Drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/Application
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/rt-thread/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/rt-thread/components/finsh
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/rt-thread/components/ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/rt-thread/components/ulog
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/rt-thread/components/utest
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/fatfs/source
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

if(Y_TRACE_ENABLE_LVGL)
    list(APPEND Y_TRACE_DEFINES
        Y_TRACE_ENABLE_LVGL
    )

    list(APPEND Y_TRACE_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/lvgl
    )
endif()

set(Y_TRACE_CPU_FLAGS
    -mthumb
    -mcpu=cortex-m4
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

set(Y_TRACE_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/Board/misc/AT32F403AxG_FLASH.ld)
