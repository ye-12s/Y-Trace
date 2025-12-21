//
// Created by An on 2025/12/14.
//

#include "drv_uart.h"
#include <stddef.h>

#include "at32f403a_407.h"
#include "ringbuffer.h"

static struct rt_ringbuffer *shell_uart_ringbuffer = NULL;
static rt_sem_t shell_uart_sem =  NULL;

int shell_uart_init(int baudrate)
{
    shell_uart_ringbuffer = rt_ringbuffer_create(512);
    shell_uart_sem = rt_sem_create("shell_uart", 1, RT_IPC_FLAG_FIFO);
    gpio_init_type gpio_init_struct;
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, TRUE);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins = GPIO_PINS_9;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOA, &gpio_init_struct);

    /* configure the RX pin */
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init_struct.gpio_out_type  = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = GPIO_PINS_10;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOA, &gpio_init_struct);

    /* configure param */
    usart_init(USART1, baudrate, USART_DATA_8BITS, USART_STOP_1_BIT);
    usart_transmitter_enable(USART1, TRUE);
    usart_receiver_enable(USART1, TRUE);
    usart_parity_selection_config(USART1, USART_PARITY_NONE);
    usart_hardware_flow_control_set(USART1, USART_HARDWARE_FLOW_NONE);
    nvic_irq_enable(USART1_IRQn, 5, 0);
    usart_interrupt_enable(USART1, USART_RDBF_INT, TRUE);
    usart_enable(USART1, TRUE);
    return 0;
}

void shell_uart_putc(int ch)
{
    while (usart_flag_get(USART1, USART_TDBE_FLAG) == RESET);
    usart_data_transmit(USART1, ch);
}

char shell_uart_getc(void)
{
    if (shell_uart_ringbuffer == NULL)
    {
        return -1;
    }
    rt_uint8_t ch = 0;
    while (1)
    {
        if (rt_ringbuffer_getchar(shell_uart_ringbuffer, &ch) == 0)
        {
            rt_sem_take(shell_uart_sem, RT_WAITING_FOREVER);
        }
        else
        {
            return ch;
        }
    }
}

void USART1_IRQHandler(void)
{
    rt_interrupt_enter();
    if (usart_interrupt_flag_get(USART1, USART_RDBF_FLAG) != RESET)
    {
        char c = usart_data_receive(USART1);
        // todo 存储在ringbuffer中
        rt_ringbuffer_putchar(shell_uart_ringbuffer, c);
        rt_sem_release(shell_uart_sem);
    }
    rt_interrupt_leave();
}

