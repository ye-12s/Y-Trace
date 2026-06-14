#ifndef __BOARD_H__
#define __BOARD_H__

#define ROM_START      ((uint32_t)0x08000000)
#define ROM_SIZE       (1024 * 1024)
#define ROM_END        ((uint32_t)(ROM_START + ROM_SIZE))

#define RAM_START      (0x20000000)
#define RAM_SIZE       (224 * 1024)
#define RAM_END        (RAM_START + RAM_SIZE)

#endif
