# cmake_minimum_required(VERSION 3.13)

# project(RTThread_lib C)

file(GLOB_RECURSE RTT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/libcpu/arm/cortex-m4/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/device/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/finsh/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/device/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/ipc/*.c"
   "${CMAKE_CURRENT_SOURCE_DIR}/components/ulog/*.c"
)

function(assign_source_group)
    foreach(_src IN ITEMS ${ARGV})
        get_filename_component(_path "${_src}" PATH)
        file(RELATIVE_PATH _rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_path}")
        string(REPLACE "/" "\\" _grp "${_rel}")
        source_group("${_grp}" FILES "${_src}")
    endforeach()
endfunction()

assign_source_group(${RTT_SOURCES})

add_library(RTThread STATIC ${RTT_SOURCES})

target_include_directories(RTThread PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/components
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libcpu/arm/cortex-m4
    ${CMAKE_CURRENT_SOURCE_DIR}/components/finsh
    ${CMAKE_CURRENT_SOURCE_DIR}/components/device
    ${CMAKE_CURRENT_SOURCE_DIR}/components/ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/components/ulog
)
