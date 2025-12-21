
# For this cpu port we only compile the GCC-compatible sources (avoid IAR/RVDS asm files)
set(_srcs
	${CMAKE_CURRENT_SOURCE_DIR}/context_gcc.S
	${CMAKE_CURRENT_SOURCE_DIR}/cpuport.c
)

add_library(obj_Middlewares_rt_thread_libcpu_arm_cortex_m4 OBJECT ${_srcs})

# Register this object target into the global collector used by top-level
get_property(_t_existing GLOBAL PROPERTY PROJECT_OBJECT_TARGETS)
if(_t_existing)
	set(_t_new "${_t_existing};obj_Middlewares_rt_thread_libcpu_arm_cortex_m4")
else()
	set(_t_new "obj_Middlewares_rt_thread_libcpu_arm_cortex_m4")
endif()
set_property(GLOBAL PROPERTY PROJECT_OBJECT_TARGETS "${_t_new}")
message(STATUS "Registered obj_Middlewares_rt_thread_libcpu_arm_cortex_m4 explicitly with ${_srcs}")
