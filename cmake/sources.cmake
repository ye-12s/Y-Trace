set(Y_TRACE_SOURCES
    Application/app_init.c
    Application/main.c
    Application/port/diskio.c
    Application/sample/gnss_sample.c
    Application/sample/imu_sample.c
    Application/sample/lcd_refresh_sample.c
    Application/sample/mag_sample.c
    Application/utils/minmea/minmea.c

    Board/at32f403a_407_int.c
    Board/board.c
    Board/syscall.c
    Board/Drivers/drv_flash.c
    Board/Drivers/drv_gnss.c
    Board/Drivers/drv_imu.c
    Board/Drivers/drv_lcd.c
    Board/Drivers/drv_lis3mdltr.c
    Board/Drivers/drv_lsm6dsm.c
    Board/Drivers/drv_pin.c
    Board/Drivers/drv_rtt.c
    Board/Drivers/drv_sram.c
    Board/Drivers/drv_sdio.c
    Board/Drivers/drv_soft_iic.c
    Board/Drivers/drv_uart.c
    Board/Drivers/drv_ws2812b.c
    cmake/newlib_heap_bridge.c

    libraries/cmsis/cm4/device_support/system_at32f403a_407.c
    libraries/cmsis/cm4/device_support/startup/gcc/startup_at32f403a_407.s
    libraries/drivers/src/at32f403a_407_acc.c
    libraries/drivers/src/at32f403a_407_adc.c
    libraries/drivers/src/at32f403a_407_bpr.c
    libraries/drivers/src/at32f403a_407_can.c
    libraries/drivers/src/at32f403a_407_crc.c
    libraries/drivers/src/at32f403a_407_crm.c
    libraries/drivers/src/at32f403a_407_dac.c
    libraries/drivers/src/at32f403a_407_debug.c
    libraries/drivers/src/at32f403a_407_dma.c
    libraries/drivers/src/at32f403a_407_emac.c
    libraries/drivers/src/at32f403a_407_exint.c
    libraries/drivers/src/at32f403a_407_flash.c
    libraries/drivers/src/at32f403a_407_gpio.c
    libraries/drivers/src/at32f403a_407_i2c.c
    libraries/drivers/src/at32f403a_407_misc.c
    libraries/drivers/src/at32f403a_407_pwc.c
    libraries/drivers/src/at32f403a_407_rtc.c
    libraries/drivers/src/at32f403a_407_sdio.c
    libraries/drivers/src/at32f403a_407_spi.c
    libraries/drivers/src/at32f403a_407_tmr.c
    libraries/drivers/src/at32f403a_407_usart.c
    libraries/drivers/src/at32f403a_407_usb.c
    libraries/drivers/src/at32f403a_407_wdt.c
    libraries/drivers/src/at32f403a_407_wwdt.c
    libraries/drivers/src/at32f403a_407_xmc.c

    Middlewares/fatfs/source/ff.c
    Middlewares/fatfs/source/ffsystem.c
    Middlewares/fatfs/source/ffunicode.c
    Middlewares/rt-thread/components/finsh/cmd.c
    Middlewares/rt-thread/components/finsh/finsh_port.c
    Middlewares/rt-thread/components/finsh/msh.c
    Middlewares/rt-thread/components/finsh/shell.c
    Middlewares/rt-thread/components/ipc/completion.c
    Middlewares/rt-thread/components/ipc/dataqueue.c
    Middlewares/rt-thread/components/ipc/ringblk_buf.c
    Middlewares/rt-thread/components/ipc/ringbuffer.c
    Middlewares/rt-thread/components/ulog/backend/console_be.c
    Middlewares/rt-thread/components/ulog/syslog/syslog.c
    Middlewares/rt-thread/components/ulog/ulog.c
    Middlewares/rt-thread/components/utest/utest.c
    Middlewares/rt-thread/libcpu/arm/common/backtrace.c
    Middlewares/rt-thread/libcpu/arm/common/div0.c
    Middlewares/rt-thread/libcpu/arm/common/showmem.c
    Middlewares/rt-thread/libcpu/arm/cortex-m4/context_gcc.S
    Middlewares/rt-thread/libcpu/arm/cortex-m4/cpuport.c
    Middlewares/rt-thread/src/clock.c
    Middlewares/rt-thread/src/components.c
    Middlewares/rt-thread/src/cpu.c
    Middlewares/rt-thread/src/idle.c
    Middlewares/rt-thread/src/ipc.c
    Middlewares/rt-thread/src/irq.c
    Middlewares/rt-thread/src/kservice.c
    Middlewares/rt-thread/src/mem.c
    Middlewares/rt-thread/src/memheap.c
    Middlewares/rt-thread/src/mempool.c
    Middlewares/rt-thread/src/object.c
    Middlewares/rt-thread/src/scheduler.c
    Middlewares/rt-thread/src/slab.c
    Middlewares/rt-thread/src/thread.c
    Middlewares/rt-thread/src/timer.c

)

if(Y_TRACE_ENABLE_FATFS_SD_BENCH OR Y_TRACE_FATFS_SD_BENCH_AUTORUN)
    list(APPEND Y_TRACE_SOURCES
        Application/sample/fatfs_sd_bench.c
    )
endif()

if(Y_TRACE_ENABLE_LVGL)
    list(APPEND Y_TRACE_SOURCES
        Application/map/map_benchmark.c
        Application/map/ytrace_map_tiles.c
        Application/port/lvgl_port.c
    )

    file(GLOB_RECURSE Y_TRACE_LVGL_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/lvgl/src/*.c
    )

    list(APPEND Y_TRACE_SOURCES ${Y_TRACE_LVGL_SOURCES})
endif()
