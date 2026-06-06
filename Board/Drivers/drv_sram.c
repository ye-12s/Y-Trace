#include "drv_sram.h"

#include "at32f403a_407_flash.h"
#include <rtthread.h>

#define SRAM_224K_EOPB0_VALUE 0xFEU
#define SRAM_EOPB0_ERASED     0xFFU

int drv_sram_224k_is_enabled(void)
{
    return ((uint8_t)(USD->eopb0 & 0xFFU)) == SRAM_224K_EOPB0_VALUE ? RT_TRUE : RT_FALSE;
}

int drv_sram_224k_prepare(void)
{
    uint8_t eopb0 = (uint8_t)(USD->eopb0 & 0xFFU);

    if (eopb0 == SRAM_224K_EOPB0_VALUE) {
        return RT_TRUE;
    }

    if (eopb0 != SRAM_EOPB0_ERASED) {
        rt_kprintf("sram: EOPB0=0x%02x, cannot program 224K mode without erasing option bytes\n", eopb0);
        return RT_FALSE;
    }

    flash_status_type status = flash_user_system_data_program((uint32_t)&USD->eopb0, SRAM_224K_EOPB0_VALUE);
    if (status != FLASH_OPERATE_DONE) {
        rt_kprintf("sram: failed to program EOPB0=0x%02x, status=%d\n", SRAM_224K_EOPB0_VALUE, status);
        return RT_FALSE;
    }

    rt_kprintf("sram: programmed EOPB0=0x%02x for 224K SRAM, reset to reload option byte\n", SRAM_224K_EOPB0_VALUE);
    rt_thread_mdelay(20);
    NVIC_SystemReset();

    return RT_FALSE;
}
