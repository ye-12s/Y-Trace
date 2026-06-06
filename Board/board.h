#ifndef __BOARD_H__
#define __BOARD_H__

#define ROM_START      ((uint32_t)0x08000000)
#define ROM_SIZE       (1024 * 1024)
#define ROM_END        ((uint32_t)(ROM_START + ROM_SIZE))

#define RAM_START      (0x20000000)
#define RAM_SIZE       (96 * 1024)
#define RAM_END        (RAM_START + RAM_SIZE)

#define RAM_EXT_START  RAM_END
#define RAM_EXT_SIZE   (128 * 1024)
#define RAM_TOTAL_SIZE (RAM_SIZE + RAM_EXT_SIZE)
#define RAM_TOTAL_END  (RAM_START + RAM_TOTAL_SIZE)

#endif
