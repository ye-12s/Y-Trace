//
// Created by An on 2026/06/07.
//

#include "drv_ws2812b.h"

#include "at32f403a_407.h"
#include "rthw.h"

/*
 * FIXME: This WS2812B driver is still hardware-broken.
 * Red/yellow/blue 1 Hz diagnostics do not render the requested colors on the
 * PA15 single-LED PCB, so the final byte order/timing/latch protocol remains
 * unresolved. Keep this warning until oscilloscope-backed protocol validation
 * proves stable colors.
 */

#define WS2812B_TMR              TMR2
#define WS2812B_TMR_CHANNEL      TMR_SELECT_CHANNEL_1

#define WS2812B_DMA              DMA1
#define WS2812B_DMA_CHANNEL      DMA1_CHANNEL1
#define WS2812B_DMA_FLEX_CHANNEL FLEX_CHANNEL1
#define WS2812B_DMA_REQUEST      DMA_FLEXIBLE_TMR2_OVERFLOW
#define WS2812B_DMA_DONE         DMA1_FDT1_FLAG
#define WS2812B_DMA_ERR          DMA1_DTERR1_FLAG
#define WS2812B_DMA_CLR          DMA1_GL1_FLAG

/*
 * Board clock config sets general APB1 timers from the 240 MHz effective timer
 * clock, so 800 kHz WS2812B bits use 300 timer ticks.
 */
#define WS2812B_TMR_COUNTER_HZ 240000000ULL
#define WS2812B_NS_PER_SECOND  1000000000ULL
#define WS2812B_NS_TO_TICKS(ns) \
    ((((uint64_t)(ns) * WS2812B_TMR_COUNTER_HZ) + (WS2812B_NS_PER_SECOND / 2ULL)) / WS2812B_NS_PER_SECOND)

#define WS2812B_RESET_US                      80U
#define WS2812B_BIT_TOTAL_NS                  1250U
#define WS2812B_T0H_NS                        300U
#define WS2812B_T1H_NS                        650U
#define WS2812B_PWM_PERIOD                    ((uint16_t)WS2812B_NS_TO_TICKS(WS2812B_BIT_TOTAL_NS))
#define WS2812B_DUTY_0                        ((uint16_t)WS2812B_NS_TO_TICKS(WS2812B_T0H_NS))
#define WS2812B_DUTY_1                        ((uint16_t)WS2812B_NS_TO_TICKS(WS2812B_T1H_NS))
#define WS2812B_BUILD_ASSERT(condition, name) typedef char name[(condition) ? 1 : -1]
WS2812B_BUILD_ASSERT(WS2812B_PWM_PERIOD == 300U, ws2812b_period_ticks_must_match_board_clock);
WS2812B_BUILD_ASSERT(WS2812B_DUTY_0 == 72U, ws2812b_t0h_ticks_must_match_board_clock);
WS2812B_BUILD_ASSERT(WS2812B_DUTY_1 == 156U, ws2812b_t1h_ticks_must_match_board_clock);
#define WS2812B_BITS_PER_PIXEL    24U
#define WS2812B_STOP_LOW_BITS     8U
#define WS2812B_FRAME_SLOTS       (WS2812B_MAX_PIXELS * WS2812B_BITS_PER_PIXEL + WS2812B_STOP_LOW_BITS)
#define WS2812B_DMA_TIMEOUT_TICKS (RT_TICK_PER_SECOND / 10U)
#define WS2812B_BYTE_ORDER_GRB    1

typedef union {
    uint16_t all_buffer[WS2812B_MAX_PIXELS][WS2812B_BITS_PER_PIXEL];
    uint16_t flat_buffer[WS2812B_FRAME_SLOTS];
} ws2812b_pixel_buffer_t;

static ws2812b_pixel_buffer_t pixel_buffer;
static volatile uint8_t ws2812b_tail_done = 0U;
static volatile uint8_t ws2812b_dma_error = 0U;
static uint8_t ws2812b_initialized        = 0U;

static int ws2812b_gpio_config(void)
{
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    gpio_pin_remap_config(SWJTAG_GMUX_010, TRUE);
    gpio_pin_remap_config(TMR2_MUX_01, TRUE);
    return pin_af_init(WS2812B_PIN, PIN_PULL_NONE, PIN_MODE_AF, 0);
}

static void ws2812b_timer_config(void)
{
    tmr_output_config_type output_config;

    crm_periph_clock_enable(CRM_TMR2_PERIPH_CLOCK, TRUE);
    tmr_reset(WS2812B_TMR);
    tmr_base_init(WS2812B_TMR, WS2812B_PWM_PERIOD - 1U, 0U);
    tmr_cnt_dir_set(WS2812B_TMR, TMR_COUNT_UP);
    tmr_clock_source_div_set(WS2812B_TMR, TMR_CLOCK_DIV1);
    tmr_internal_clock_set(WS2812B_TMR);

    tmr_output_default_para_init(&output_config);
    output_config.oc_mode         = TMR_OUTPUT_CONTROL_PWM_MODE_A;
    output_config.oc_output_state = TRUE;
    output_config.oc_polarity     = TMR_OUTPUT_ACTIVE_HIGH;
    output_config.oc_idle_state   = FALSE;

    tmr_output_channel_config(WS2812B_TMR, WS2812B_TMR_CHANNEL, &output_config);
    tmr_channel_value_set(WS2812B_TMR, WS2812B_TMR_CHANNEL, 0U);
    tmr_output_channel_buffer_enable(WS2812B_TMR, WS2812B_TMR_CHANNEL, TRUE);
    tmr_channel_dma_select(WS2812B_TMR, TMR_DMA_REQUEST_BY_OVERFLOW);
    tmr_dma_request_enable(WS2812B_TMR, TMR_OVERFLOW_DMA_REQUEST, TRUE);
    tmr_interrupt_enable(WS2812B_TMR, TMR_OVF_FLAG, FALSE);
    tmr_channel_enable(WS2812B_TMR, WS2812B_TMR_CHANNEL, TRUE);
    nvic_irq_enable(TMR2_GLOBAL_IRQn, 4, 0);
}

static void ws2812b_dma_config(void)
{
    dma_init_type dma_init_struct;

    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);
    dma_channel_enable(WS2812B_DMA_CHANNEL, FALSE);
    dma_reset(WS2812B_DMA_CHANNEL);
    dma_default_para_init(&dma_init_struct);
    dma_init_struct.peripheral_base_addr  = (uint32_t)&WS2812B_TMR->c1dt;
    dma_init_struct.memory_base_addr      = (uint32_t)&pixel_buffer.flat_buffer[1];
    dma_init_struct.direction             = DMA_DIR_MEMORY_TO_PERIPHERAL;
    dma_init_struct.buffer_size           = 0U;
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.memory_inc_enable     = TRUE;
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_HALFWORD;
    dma_init_struct.memory_data_width     = DMA_MEMORY_DATA_WIDTH_HALFWORD;
    dma_init_struct.loop_mode_enable      = FALSE;
    dma_init_struct.priority              = DMA_PRIORITY_VERY_HIGH;
    dma_init(WS2812B_DMA_CHANNEL, &dma_init_struct);

    dma_flexible_config(WS2812B_DMA, WS2812B_DMA_FLEX_CHANNEL, WS2812B_DMA_REQUEST);
    dma_flag_clear(WS2812B_DMA_CLR);
    dma_interrupt_enable(WS2812B_DMA_CHANNEL, DMA_FDT_INT, TRUE);
    nvic_irq_enable(DMA1_Channel1_IRQn, 4, 0);
}

static uint32_t ws2812b_color(uint8_t red, uint8_t green, uint8_t blue)
{
    return ((uint32_t)green << 16U) | ((uint32_t)red << 8U) | blue;
}

static void ws2812b_set_pixel_color(rt_size_t index, uint32_t grb_color)
{
    if (index >= WS2812B_MAX_PIXELS) {
        return;
    }

    for (uint8_t bit = 0U; bit < WS2812B_BITS_PER_PIXEL; bit++) {
        pixel_buffer.all_buffer[index][bit] = ((grb_color << bit) & 0x800000UL) != 0U ? WS2812B_DUTY_1 : WS2812B_DUTY_0;
    }
}

static void ws2812b_set_pixel_rgb(rt_size_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    ws2812b_set_pixel_color(index, ws2812b_color(red, green, blue));
}

static void ws2812b_fill_stop_low_bits(void)
{
    for (uint8_t bit = 0U; bit < WS2812B_STOP_LOW_BITS; bit++) {
        pixel_buffer.flat_buffer[WS2812B_MAX_PIXELS * WS2812B_BITS_PER_PIXEL + bit] = WS2812B_DUTY_0;
    }
}

static void ws2812b_close_all(void)
{
    for (rt_size_t index = 0U; index < WS2812B_MAX_PIXELS; index++) {
        ws2812b_set_pixel_rgb(index, 0U, 0U, 0U);
    }
}

static int ws2812b_show(void)
{
    const rt_tick_t start_tick = rt_tick_get();

    while (dma_data_number_get(WS2812B_DMA_CHANNEL) != 0U) {
        if ((rt_tick_get() - start_tick) > WS2812B_DMA_TIMEOUT_TICKS) {
            return -1;
        }
    }

    ws2812b_tail_done = 0U;
    ws2812b_dma_error = 0U;
    ws2812b_fill_stop_low_bits();

    tmr_counter_enable(WS2812B_TMR, FALSE);
    tmr_counter_value_set(WS2812B_TMR, 0U);
    tmr_output_channel_buffer_enable(WS2812B_TMR, WS2812B_TMR_CHANNEL, FALSE);
    tmr_channel_value_set(WS2812B_TMR, WS2812B_TMR_CHANNEL, pixel_buffer.flat_buffer[0]);
    tmr_output_channel_buffer_enable(WS2812B_TMR, WS2812B_TMR_CHANNEL, TRUE);
    tmr_flag_clear(WS2812B_TMR, TMR_OVF_FLAG);
    tmr_interrupt_enable(WS2812B_TMR, TMR_OVF_FLAG, FALSE);
    dma_channel_enable(WS2812B_DMA_CHANNEL, FALSE);
    dma_flag_clear(WS2812B_DMA_CLR);
    dma_data_number_set(WS2812B_DMA_CHANNEL, WS2812B_FRAME_SLOTS - 1U);
    dma_channel_enable(WS2812B_DMA_CHANNEL, TRUE);
    tmr_counter_enable(WS2812B_TMR, TRUE);

    while (ws2812b_tail_done == 0U) {
        if (ws2812b_dma_error != 0U || dma_flag_get(WS2812B_DMA_ERR) == SET) {
            return -1;
        }

        if ((rt_tick_get() - start_tick) > WS2812B_DMA_TIMEOUT_TICKS) {
            return -1;
        }
    }

    rt_hw_us_delay(WS2812B_RESET_US);
    return 0;
}

int drv_ws2812b_init(void)
{
    if (ws2812b_gpio_config() != 0) {
        return -1;
    }

    ws2812b_dma_config();
    ws2812b_timer_config();
    ws2812b_initialized = 1U;
    drv_ws2812b_clear();
    return 0;
}

int drv_ws2812b_write_rgb(const ws2812b_rgb_t *pixels, rt_size_t count)
{
    if (!ws2812b_initialized || count > WS2812B_MAX_PIXELS || (pixels == RT_NULL && count > 0U)) {
        return -1;
    }

    ws2812b_close_all();
    for (rt_size_t index = 0U; index < count; index++) {
        ws2812b_set_pixel_rgb(index, pixels[index].red, pixels[index].green, pixels[index].blue);
    }

    return ws2812b_show();
}

int drv_ws2812b_set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    const ws2812b_rgb_t pixel = {
        .red   = red,
        .green = green,
        .blue  = blue,
    };

    return drv_ws2812b_write_rgb(&pixel, 1U);
}

void drv_ws2812b_clear(void)
{
    if (!ws2812b_initialized) {
        return;
    }

    ws2812b_rgb_t clear_pixels[WS2812B_MAX_PIXELS] = {0};
    (void)drv_ws2812b_write_rgb(clear_pixels, WS2812B_MAX_PIXELS);
}

void DMA1_Channel1_IRQHandler(void)
{
    rt_interrupt_enter();
    if (dma_flag_get(WS2812B_DMA_DONE) != RESET) {
        dma_flag_clear(WS2812B_DMA_DONE);
        dma_channel_enable(WS2812B_DMA_CHANNEL, FALSE);
        tmr_flag_clear(WS2812B_TMR, TMR_OVF_FLAG);
        tmr_interrupt_enable(WS2812B_TMR, TMR_OVF_FLAG, TRUE);
    }

    if (dma_flag_get(WS2812B_DMA_ERR) != RESET) {
        dma_flag_clear(WS2812B_DMA_ERR);
        ws2812b_dma_error = 1U;
    }
    rt_interrupt_leave();
}

void TMR2_GLOBAL_IRQHandler(void)
{
    rt_interrupt_enter();
    if (tmr_flag_get(WS2812B_TMR, TMR_OVF_FLAG) != RESET) {
        tmr_channel_value_set(WS2812B_TMR, WS2812B_TMR_CHANNEL, 0U);
        tmr_flag_clear(WS2812B_TMR, TMR_OVF_FLAG);
        tmr_counter_enable(WS2812B_TMR, FALSE);
        tmr_interrupt_enable(WS2812B_TMR, TMR_OVF_FLAG, FALSE);
        ws2812b_tail_done = 1U;
    }
    rt_interrupt_leave();
}
