//
// Created by An on 2026/06/07.
//

#include "drv_ws2812b.h"

#include "at32f403a_407.h"
#include "rthw.h"

#define WS2812B_TMR WS2812B_TMR2
#define WS2812B_TMR2 TMR2
#define WS2812B_TMR_CHANNEL TMR_SELECT_CHANNEL_1

#define WS2812B_DMA DMA1
#define WS2812B_DMA_CHANNEL DMA1_CHANNEL1
#define WS2812B_DMA_FLEX_CHANNEL FLEX_CHANNEL1
#define WS2812B_DMA_REQUEST DMA_FLEXIBLE_TMR2_CH1
#define WS2812B_DMA_DONE DMA1_FDT1_FLAG
#define WS2812B_DMA_ERR DMA1_DTERR1_FLAG
#define WS2812B_DMA_CLR DMA1_GL1_FLAG

/*
 * Board clock config sets TMR2 on APB1 with APB1 prescaler /2.
 * AT32 general timers count at 2x PCLK when the APB prescaler is not 1,
 * so TMR2 runs from the 240 MHz effective timer clock.
 */
#define WS2812B_TMR_COUNTER_HZ 240000000ULL
#define WS2812B_NS_PER_SECOND 1000000000ULL
#define WS2812B_NS_TO_TICKS(ns) \
    ((((uint64_t)(ns) * WS2812B_TMR_COUNTER_HZ) + (WS2812B_NS_PER_SECOND / 2ULL)) / WS2812B_NS_PER_SECOND)

#define WS2812B_RESET_US 80U
#define WS2812B_BIT_TOTAL_NS 1250U
#define WS2812B_T0H_NS 375U
#define WS2812B_T1H_NS 750U
#define WS2812B_PWM_PERIOD ((uint16_t)WS2812B_NS_TO_TICKS(WS2812B_BIT_TOTAL_NS))
#define WS2812B_DUTY_0 ((uint16_t)WS2812B_NS_TO_TICKS(WS2812B_T0H_NS))
#define WS2812B_DUTY_1 ((uint16_t)WS2812B_NS_TO_TICKS(WS2812B_T1H_NS))
#define WS2812B_BITS_PER_PIXEL 24U
#define WS2812B_RESET_SLOTS 64U
#define WS2812B_DMA_TIMEOUT_TICKS (RT_TICK_PER_SECOND / 10U)
#define WS2812B_FRAME_SLOTS(pixel_count) ((pixel_count) * WS2812B_BITS_PER_PIXEL + WS2812B_RESET_SLOTS)

static uint16_t ws2812b_pwm_buffer[WS2812B_FRAME_SLOTS(WS2812B_MAX_PIXELS)];
static uint8_t ws2812b_initialized = 0;

static void ws2812b_timer_init(void)
{
    tmr_output_config_type output_config;

    crm_periph_clock_enable(CRM_TMR2_PERIPH_CLOCK, TRUE);
    tmr_reset(WS2812B_TMR);
    tmr_base_init(WS2812B_TMR, WS2812B_PWM_PERIOD - 1U, 0U);
    tmr_cnt_dir_set(WS2812B_TMR, TMR_COUNT_UP);
    tmr_clock_source_div_set(WS2812B_TMR, TMR_CLOCK_DIV1);
    tmr_internal_clock_set(WS2812B_TMR);

    tmr_output_default_para_init(&output_config);
    output_config.oc_mode = TMR_OUTPUT_CONTROL_PWM_MODE_A;
    output_config.oc_output_state = TRUE;
    output_config.oc_polarity = TMR_OUTPUT_ACTIVE_HIGH;
    output_config.oc_idle_state = FALSE;

    tmr_output_channel_config(WS2812B_TMR, TMR_SELECT_CHANNEL_1, &output_config);
    tmr_output_channel_buffer_enable(WS2812B_TMR, WS2812B_TMR_CHANNEL, TRUE);
    tmr_channel_value_set(WS2812B_TMR, WS2812B_TMR_CHANNEL, 0U);
    tmr_channel_dma_select(WS2812B_TMR, TMR_DMA_REQUEST_BY_OVERFLOW);
    tmr_dma_request_enable(WS2812B_TMR, TMR_OVERFLOW_DMA_REQUEST, TRUE);
    tmr_channel_enable(WS2812B_TMR, WS2812B_TMR_CHANNEL, TRUE);
}

static void ws2812b_dma_init(void)
{
    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);
    dma_flexible_config(WS2812B_DMA, WS2812B_DMA_FLEX_CHANNEL, WS2812B_DMA_REQUEST);
    dma_channel_enable(WS2812B_DMA_CHANNEL, FALSE);
    dma_flag_clear(WS2812B_DMA_CLR);
}

static void ws2812b_encode_byte(uint8_t byte, uint16_t *buffer, rt_size_t *offset)
{
    for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U) {
        buffer[*offset] = (byte & mask) != 0U ? WS2812B_DUTY_1 : WS2812B_DUTY_0;
        (*offset)++;
    }
}

static rt_size_t ws2812b_encode_frame(const ws2812b_rgb_t *pixels, rt_size_t count)
{
    rt_size_t offset = 0;

    for (rt_size_t index = 0; index < count; index++) {
        ws2812b_encode_byte(pixels[index].green, ws2812b_pwm_buffer, &offset);
        ws2812b_encode_byte(pixels[index].red, ws2812b_pwm_buffer, &offset);
        ws2812b_encode_byte(pixels[index].blue, ws2812b_pwm_buffer, &offset);
    }

    for (rt_size_t index = 0; index < WS2812B_RESET_SLOTS; index++) {
        ws2812b_pwm_buffer[offset++] = 0U;
    }

    return offset;
}

static void ws2812b_dma_start(rt_size_t slot_count)
{
    dma_init_type dma_init_struct;

    dma_channel_enable(WS2812B_DMA_CHANNEL, FALSE);
    dma_reset(WS2812B_DMA_CHANNEL);
    dma_flag_clear(WS2812B_DMA_CLR);

    dma_default_para_init(&dma_init_struct);
    dma_init_struct.peripheral_base_addr = (uint32_t)&WS2812B_TMR->c1dt;
    dma_init_struct.memory_base_addr = (uint32_t)&ws2812b_pwm_buffer[1];
    dma_init_struct.direction = DMA_DIR_MEMORY_TO_PERIPHERAL;
    dma_init_struct.buffer_size = (uint16_t)(slot_count - 1U);
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.memory_inc_enable = TRUE;
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_HALFWORD;
    dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_HALFWORD;
    dma_init_struct.loop_mode_enable = FALSE;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_init(WS2812B_DMA_CHANNEL, &dma_init_struct);

    tmr_counter_enable(WS2812B_TMR, FALSE);
    tmr_counter_value_set(WS2812B_TMR, 0U);
    tmr_channel_value_set(WS2812B_TMR, WS2812B_TMR_CHANNEL, ws2812b_pwm_buffer[0]);
    dma_channel_enable(WS2812B_DMA_CHANNEL, TRUE);
    tmr_counter_enable(WS2812B_TMR, TRUE);
}

static void ws2812b_dma_stop(void)
{
    tmr_counter_enable(WS2812B_TMR, FALSE);
    dma_channel_enable(WS2812B_DMA_CHANNEL, FALSE);
    tmr_channel_value_set(WS2812B_TMR, WS2812B_TMR_CHANNEL, 0U);
    dma_flag_clear(WS2812B_DMA_CLR);
}

int drv_ws2812b_init(void)
{
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    gpio_pin_remap_config(SWJTAG_GMUX_010, TRUE);
    gpio_pin_remap_config(TMR2_MUX_01, TRUE);

    if (pin_af_init(WS2812B_PIN, PIN_PULL_NONE, PIN_MODE_AF, 0) != 0) {
        return -1;
    }

    ws2812b_timer_init();
    ws2812b_dma_init();
    ws2812b_initialized = 1;
    drv_ws2812b_clear();
    return 0;
}

int drv_ws2812b_write_rgb(const ws2812b_rgb_t *pixels, rt_size_t count)
{
    if (!ws2812b_initialized || count > WS2812B_MAX_PIXELS || (pixels == RT_NULL && count > 0U)) {
        return -1;
    }

    const rt_size_t slot_count = ws2812b_encode_frame(pixels, count);
    const rt_tick_t start_tick = rt_tick_get();

    ws2812b_dma_start(slot_count);
    while (dma_flag_get(WS2812B_DMA_DONE) == RESET) {
        if (dma_flag_get(WS2812B_DMA_ERR) == SET) {
            ws2812b_dma_stop();
            return -1;
        }

        if ((rt_tick_get() - start_tick) > WS2812B_DMA_TIMEOUT_TICKS) {
            ws2812b_dma_stop();
            return -1;
        }
    }

    ws2812b_dma_stop();
    rt_hw_us_delay(WS2812B_RESET_US);
    return 0;
}

int drv_ws2812b_set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    const ws2812b_rgb_t pixel = {
        .red = red,
        .green = green,
        .blue = blue,
    };

    return drv_ws2812b_write_rgb(&pixel, 1U);
}

void drv_ws2812b_clear(void)
{
    if (!ws2812b_initialized) {
        return;
    }

    (void)drv_ws2812b_set_rgb(0U, 0U, 0U);
}
