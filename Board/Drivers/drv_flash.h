#ifndef __DRV_FLASH_H__
#define __DRV_FLASH_H__

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define FLASH_BASE_ADDR                 (0x08000000)
#define SECTOR_SIZE                     (0x800)
#define SECTOR_NUM                      (512)

#define FLASH_FILESYSTEM_START_ADDR     (FLASH_BASE_ADDR + SECTOR_SIZE * 448 ) // 448 sector for filesystem
#define FLASH_FILESYSTEM_SIZE           (SECTOR_SIZE * 64) // 64 sector for filesystem

int drv_flash_read(uint32_t addr, uint8_t *data, uint32_t length);
int drv_flash_write_nocheck(uint32_t addr, uint8_t *data, uint32_t length);
int drv_flash_erase_sector(uint32_t addr);
int drv_flash_write_sector(uint32_t addr, uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif
