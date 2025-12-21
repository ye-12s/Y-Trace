//
// Created by An on 2025/12/12.
//

#ifndef Y_TRACK_DRV_COMMON_H
#define Y_TRACK_DRV_COMMON_H

#include "board.h"

#define AT32_SRAM1_SIZE               RAM_SIZE
#define AT32_SRAM1_START              RAM_START
#define AT32_SRAM1_END                RAM_END

#if defined(__CC_ARM) || defined(__CLANG_ARM)
extern int Image$RW_IRAM1$ZI$Limit;
#define HEAP_BEGIN      ((void *)&Image$RW_IRAM1$ZI$Limit)
#elif __ICCARM__
#pragma section="CSTACK"
#define HEAP_BEGIN      (__segment_end("CSTACK"))
#else
extern int __bss_end;
#define HEAP_BEGIN      ((void *)&__bss_end)
#endif

#define HEAP_END                       RAM_END


#endif //Y_TRACK_DRV_COMMON_H