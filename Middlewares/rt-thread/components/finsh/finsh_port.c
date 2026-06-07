/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 */

#include "Drivers/drv_uart.h"
#include "Drivers/drv_rtt.h"
#include <rthw.h>
#include <rtconfig.h>
// #include <Drivers/drv_uart.h>

#ifndef RT_USING_FINSH
#error Please uncomment the line <#include "finsh_config.h"> in the rtconfig.h 
#endif

#ifdef RT_USING_FINSH

RT_WEAK char rt_hw_console_getchar(void)
{
    /* Note: the initial value of ch must < 0 */
    int ch = -1;
	ch = shell_uart_getc();
    return ch;
}

void rt_hw_console_output(const char *str)
{
    rt_size_t i = 0, size = 0;

#ifdef Y_TRACE_USING_RTT_CONSOLE
    ytrace_rtt_write_string(str);
#endif

    if (!shell_uart_is_initialized())
    {
        return;
    }

    size = rt_strlen(str);
    for (i = 0; i < size; i++)
    {
        if (*(str + i) == '\n')
        {
            shell_uart_putc('\r');
        }
        shell_uart_putc(*(str + i));
    }
}

#endif /* RT_USING_FINSH */
