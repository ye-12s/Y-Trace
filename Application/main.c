/**
  **************************************************************************
  * @file     main.c
  * @version  v2.1.2
  * @date     2022-08-16
  * @brief    main program
  **************************************************************************
  *                       Copyright notice & Disclaimer
  *
  * The software Board Support Package (BSP) that is made available to
  * download from Artery official website is the copyrighted work of Artery.
  * Artery authorizes customers to use, copy, and distribute the BSP
  * software and its related documentation for the purpose of design and
  * development in conjunction with Artery microcontrollers. Use of the
  * software is governed by this copyright notice and the following disclaimer.
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
  * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
  * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
  * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
  * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
  *
  **************************************************************************
  */

/** @addtogroup AT32F403A_periph_template
  * @{
  */

/** @addtogroup 403A_LED_toggle LED_toggle
  * @{
  */

#include "rtthread.h"
#include "sample/sample.h"

// static struct _soft_i2c_bus sensor_i2c_bus;
// const pin_t PIN_SENSOR_I2C_SCL = GET_PIN(A, 8);
// const pin_t PIN_SENSOR_I2C_SDA = GET_PIN(C, 9);

/**
  * @brief  main function.
  * @param  none
  * @retval none
  */
int main(void)
{
    while (1)
    {
        rt_thread_mdelay(1000);
    }
}

/**
  * @}
  */

/**
  * @}
  */
