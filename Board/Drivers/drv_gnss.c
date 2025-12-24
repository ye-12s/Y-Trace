#include "drv_gnss.h"
#include <rtthread.h>
#include "drv_pin.h"
#include "ringbuffer.h"
#include "rtdef.h"

struct rt_ringbuffer *g_gnss_ringbuffer;
rt_sem_t g_gnss_sem;

int drv_gnss_init(void)
{
    crm_periph_clock_enable(CRM_UART5_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    g_gnss_ringbuffer = rt_ringbuffer_create(1024);
    g_gnss_sem = rt_sem_create("gnss_sem", 1, RT_IPC_FLAG_FIFO);

    gpio_init_type gpio_init_struct;
    gpio_default_para_init(&gpio_init_struct);

    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins = GPIO_PINS_9;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOB, &gpio_init_struct);

    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init_struct.gpio_out_type  = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = GPIO_PINS_8;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOB, &gpio_init_struct);

    gpio_pin_remap_config(UART5_GMUX_0001, TRUE);

    /* configure param */
    usart_init(UART5, 115200, USART_DATA_8BITS, USART_STOP_1_BIT);
    usart_transmitter_enable(UART5, TRUE);
    usart_receiver_enable(UART5, TRUE);
    usart_parity_selection_config(UART5, USART_PARITY_NONE);

    usart_hardware_flow_control_set(UART5, USART_HARDWARE_FLOW_NONE);
    usart_interrupt_enable(UART5, USART_RDBF_INT, TRUE);

    nvic_irq_enable(UART5_IRQn, 5, 0);

    usart_enable(UART5, TRUE);

    return 0;
}

static int _getb(uint32_t timeout)
{
    uint8_t ch;
    rt_tick_t start = rt_tick_get();
    while (1)
    {
        if (rt_ringbuffer_getchar(g_gnss_ringbuffer, &ch))
        {
            return ch;
        }
        rt_sem_take(g_gnss_sem, timeout);
        if (timeout != (rt_tick_get() - start) >= timeout)
        {
            return -1;
        }
    }
}


int drv_gnss_get_line(char *line, int maxlen)
{
    int idx = 0;
    while (idx < maxlen - 1)
    {
        int ch = _getb(1000);
        if (ch >= 0)
        {
            line[idx++] = ch;
            if (ch == '\n')
            {
                break;
            }
        }
    }
    line[idx] = '\0';
    return idx;
}

void drv_gnss_raw_show()
{
    while (1)
    {
        char ch;
        if (rt_ringbuffer_getchar(g_gnss_ringbuffer, &ch))
        {
            rt_kprintf("%c", ch);
        }
        else
        {
            break;
        }
    }

}

void UART5_IRQHandler(void)
{
    __IO uint16_t val_rx;

    if (usart_interrupt_flag_get(UART5, USART_RDBF_FLAG) != RESET)
    {
        val_rx = usart_data_receive(UART5);
        if (g_gnss_ringbuffer)
        {
            rt_ringbuffer_putchar(g_gnss_ringbuffer, val_rx & 0xFF);
        }
    }
}