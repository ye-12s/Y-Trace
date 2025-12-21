file(GLOB_RECURSE RTT_SOURCES
    lfs.c
    lfs_util.c
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

add_library(LittleFs STATIC ${RTT_SOURCES})

target_include_directories(LittleFs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
