cmake_minimum_required(VERSION 3.20)

project(Y-Trace LANGUAGES C ASM)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/options.cmake)
include(cmake/sources.cmake)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/drivers)
file(GLOB Y_TRACE_DRIVER_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Board/Drivers/*.h)
foreach(driver_header IN LISTS Y_TRACE_DRIVER_HEADERS)
    get_filename_component(driver_header_name ${driver_header} NAME)
    configure_file(${driver_header} ${CMAKE_CURRENT_BINARY_DIR}/include/drivers/${driver_header_name} COPYONLY)
endforeach()

set_source_files_properties(Application/utils/minmea/minmea.c PROPERTIES
    COMPILE_OPTIONS -Wno-error=implicit-function-declaration
)
set_source_files_properties(Board/syscall.c PROPERTIES
    COMPILE_DEFINITIONS "_malloc_r=ytrace_rt_malloc_r;_realloc_r=ytrace_rt_realloc_r;_calloc_r=ytrace_rt_calloc_r;_free_r=ytrace_rt_free_r"
)
set_source_files_properties(
    Application/sample/lcd_refresh_sample.c
    Board/Drivers/drv_lcd.c
    PROPERTIES COMPILE_OPTIONS -O2
)

if(Y_TRACE_ENABLE_LVGL)
    set_source_files_properties(
        ${Y_TRACE_LVGL_SOURCES}
        Application/port/lvgl_port.c
        PROPERTIES COMPILE_OPTIONS -O2
    )
endif()

add_executable(${PROJECT_NAME} ${Y_TRACE_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${Y_TRACE_INCLUDE_DIRS})
target_compile_definitions(${PROJECT_NAME} PRIVATE ${Y_TRACE_DEFINES})

target_compile_features(${PROJECT_NAME} PRIVATE c_std_11)
set_target_properties(${PROJECT_NAME} PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
)

target_compile_options(${PROJECT_NAME} PRIVATE
    ${Y_TRACE_CPU_FLAGS}
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Og>
    $<$<COMPILE_LANGUAGE:C>:-g>
    $<$<COMPILE_LANGUAGE:C>:-ffunction-sections>
    $<$<COMPILE_LANGUAGE:C>:-fdata-sections>
    $<$<COMPILE_LANGUAGE:ASM>:-Wa,-mimplicit-it=always>
)

target_link_options(${PROJECT_NAME} PRIVATE
    ${Y_TRACE_CPU_FLAGS}
    LINKER:-T,${Y_TRACE_LINKER_SCRIPT}
    LINKER:-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    LINKER:--gc-sections
    LINKER:--wrap=_malloc_r
    LINKER:--wrap=_realloc_r
    LINKER:--wrap=_calloc_r
    LINKER:--wrap=_free_r
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    m
    --specs=nano.specs
    --specs=nosys.specs
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    BYPRODUCTS
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
    COMMENT "Generating firmware binary/hex artifacts and size report"
)
