set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(ARM_NONE_EABI_GCC arm-none-eabi-gcc)
find_program(ARM_NONE_EABI_OBJCOPY arm-none-eabi-objcopy)
find_program(ARM_NONE_EABI_SIZE arm-none-eabi-size)

foreach(tool ARM_NONE_EABI_GCC ARM_NONE_EABI_OBJCOPY ARM_NONE_EABI_SIZE)
    if(NOT ${tool})
        message(FATAL_ERROR "${tool} executable not found. Install the ARM GCC toolchain and ensure arm-none-eabi-* tools are on PATH.")
    endif()
endforeach()

set(CMAKE_C_COMPILER ${ARM_NONE_EABI_GCC})
set(CMAKE_ASM_COMPILER ${ARM_NONE_EABI_GCC})
set(CMAKE_OBJCOPY ${ARM_NONE_EABI_OBJCOPY} CACHE FILEPATH "ARM objcopy")
set(CMAKE_SIZE ${ARM_NONE_EABI_SIZE} CACHE FILEPATH "ARM size")
