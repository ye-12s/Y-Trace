#include "drv_flash.h"
#include "at32f403a_407.h"
#include "stdlib.h"
#include "string.h"

#include "ulog.h"


int drv_flash_init(void)
{
    //noting to do
    return 0;
}


int drv_flash_read(uint32_t addr, uint8_t *data, uint32_t length)
{
    ASSERT(addr + length <= FLASH_BASE_ADDR + SECTOR_SIZE * SECTOR_NUM);
    memset(data, 0, length);
    memcpy(data, (void *)(addr), length);
    return 0;
}


int drv_flash_write_nocheck(uint32_t addr, uint8_t *data, uint32_t length)
{
    ASSERT(addr + length <= FLASH_BASE_ADDR + SECTOR_SIZE * SECTOR_NUM);
    // osKernelLock();
    __disable_irq();
    flash_unlock();
    int ret = 0;
    for (int i = 0; i < length; i++)
    {
        if (flash_byte_program(addr + i, data[i]) != FLASH_OPERATE_DONE)
        {
            ret = -1;
            goto __exit;
        }
    }
__exit:
    flash_lock();
    // osKernelUnlock();
    __enable_irq();
    return ret;
}

int drv_flash_erase_sector(uint32_t addr)
{
    ASSERT(addr % SECTOR_SIZE == 0);
    __disable_irq();
    flash_unlock();
    flash_sector_erase(addr);
    flash_lock();
    __enable_irq();
    return 0;
}

int drv_flash_write_sector(uint32_t addr, uint8_t *data)
{
    ASSERT(addr % SECTOR_SIZE == 0);
    uint32_t *pdata = (uint32_t *)data;
    __disable_irq();
    flash_unlock();
    flash_sector_erase(addr);
    for (int i = 0; i < SECTOR_SIZE; i += sizeof(uint32_t))
    {
        if (flash_word_program(addr + i, *pdata++) != FLASH_OPERATE_DONE)
        {
            flash_lock();
            return -1;
        }
    }
    flash_lock();
    __enable_irq();
    return 0;
}
