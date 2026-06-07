#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_source_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(file, path);

    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_END));
    const long size = ftell(file);
    TEST_ASSERT_GREATER_THAN_INT32(0, size);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_SET));

    char *buffer = (char *)malloc((size_t)size + 1U);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_EQUAL_size_t((size_t)size, fread(buffer, 1U, (size_t)size, file));
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

static char *read_driver_source(void)
{
    return read_source_file(Y_TRACE_WS2812B_DRIVER_PATH);
}

static char *read_app_init_source(void)
{
    return read_source_file(Y_TRACE_APP_INIT_PATH);
}

static char *read_driver_header(void)
{
    return read_source_file(Y_TRACE_WS2812B_HEADER_PATH);
}

static void assert_contains(const char *source, const char *needle)
{
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(source, needle), needle);
}

void setUp(void)
{
}
void tearDown(void)
{
}

void test_ws2812b_keeps_board_pa15_tmr2_mapping_while_using_reference_flow(void)
{
    char *source = read_driver_source();
    char *header = read_driver_header();

    assert_contains(header, "#define WS2812B_PIN        GET_PIN(A, 15)");
    assert_contains(source, "#define WS2812B_TMR              TMR2");
    assert_contains(source, "#define WS2812B_DMA_CHANNEL      DMA1_CHANNEL1");
    assert_contains(source, "#define WS2812B_DMA_FLEX_CHANNEL FLEX_CHANNEL1");
    assert_contains(source, "#define WS2812B_DMA_REQUEST      DMA_FLEXIBLE_TMR2_OVERFLOW");
    assert_contains(source, "#define WS2812B_DMA_DONE         DMA1_FDT1_FLAG");
    assert_contains(source, "gpio_pin_remap_config(SWJTAG_GMUX_010, TRUE)");
    assert_contains(source, "gpio_pin_remap_config(TMR2_MUX_01, TRUE)");
    assert_contains(source, "tmr_channel_dma_select(WS2812B_TMR, TMR_DMA_REQUEST_BY_OVERFLOW)");
    assert_contains(source, "tmr_dma_request_enable(WS2812B_TMR, TMR_OVERFLOW_DMA_REQUEST, TRUE)");
    assert_contains(source, "dma_interrupt_enable(WS2812B_DMA_CHANNEL, DMA_FDT_INT, TRUE)");
    assert_contains(source, "nvic_irq_enable(DMA1_Channel1_IRQn");
    assert_contains(source, "nvic_irq_enable(TMR2_GLOBAL_IRQn");

    TEST_ASSERT_NULL_MESSAGE(strstr(source, "TMR3"), "PA15 board mapping must not switch to TestKit PB4/TMR3 mapping");
    TEST_ASSERT_NULL_MESSAGE(strstr(source, "DMA1_CHANNEL2"), "PA15 board mapping must not switch to TestKit DMA1 channel2 mapping");

    free(source);
    free(header);
}

void test_ws2812b_uses_interrupt_driven_tail_stop_like_reference(void)
{
    char *source = read_driver_source();

    assert_contains(source, "void DMA1_Channel1_IRQHandler(void)");
    assert_contains(source, "void TMR2_GLOBAL_IRQHandler(void)");
    assert_contains(source, "tmr_interrupt_enable(WS2812B_TMR, TMR_OVF_FLAG, TRUE)");
    assert_contains(source, "tmr_interrupt_enable(WS2812B_TMR, TMR_OVF_FLAG, FALSE)");
    assert_contains(source, "tmr_counter_enable(WS2812B_TMR, FALSE)");
    assert_contains(source, "ws2812b_tail_done = 1U");

    free(source);
}

void test_ws2812b_timing_constants_are_formula_based_from_effective_timer_clock(void)
{
    char *source = read_driver_source();

    assert_contains(source, "WS2812B_TMR_COUNTER_HZ");
    assert_contains(source, "WS2812B_NS_TO_TICKS");
    assert_contains(source, "WS2812B_BIT_TOTAL_NS");
    assert_contains(source, "WS2812B_T0H_NS");
    assert_contains(source, "WS2812B_T1H_NS");
    assert_contains(source, "#define WS2812B_T0H_NS                        300U");
    assert_contains(source, "#define WS2812B_T1H_NS                        650U");
    assert_contains(source, "WS2812B_BUILD_ASSERT(WS2812B_PWM_PERIOD == 300U");
    assert_contains(source, "WS2812B_BUILD_ASSERT(WS2812B_DUTY_0 == 72U");
    assert_contains(source, "WS2812B_BUILD_ASSERT(WS2812B_DUTY_1 == 156U");
    TEST_ASSERT_NULL_MESSAGE(strstr(source, "#define WS2812B_PWM_PERIOD 300U"),
                             "period should be derived from documented timing formulas");

    free(source);
}

void test_ws2812b_refreshes_the_full_configured_chain(void)
{
    char *driver_source = read_driver_source();
    char *app_source    = read_app_init_source();

    assert_contains(driver_source, "ws2812b_rgb_t clear_pixels[WS2812B_MAX_PIXELS] = {0};");
    assert_contains(driver_source, "drv_ws2812b_write_rgb(clear_pixels, WS2812B_MAX_PIXELS)");

    assert_contains(app_source, "ws2812b_rgb_t pixels[WS2812B_MAX_PIXELS]");
    assert_contains(app_source, "for (rt_size_t pixel = 0U; pixel < WS2812B_MAX_PIXELS; pixel++)");
    assert_contains(app_source, "pixels[pixel] = ws2812b_color_cycle[index];");
    assert_contains(app_source, "drv_ws2812b_write_rgb(pixels, WS2812B_MAX_PIXELS)");
    TEST_ASSERT_NULL_MESSAGE(strstr(app_source, "drv_ws2812b_write_rgb(&color, 1U)"),
                             "application animation must refresh every LED in the chain, not only the first pixel");

    free(driver_source);
    free(app_source);
}

void test_ws2812b_uses_reference_pixel_buffer_show_style(void)
{
    char *source = read_driver_source();

    assert_contains(source, "typedef union");
    assert_contains(source, "ws2812b_pixel_buffer_t");
    assert_contains(source, "static ws2812b_pixel_buffer_t pixel_buffer");
    assert_contains(source, "static void ws2812b_set_pixel_rgb");
    assert_contains(source, "static int ws2812b_show(void)");
    assert_contains(source, "while (dma_data_number_get(WS2812B_DMA_CHANNEL) != 0U)");
    assert_contains(source, "dma_data_number_set(WS2812B_DMA_CHANNEL, WS2812B_FRAME_SLOTS - 1U)");
    assert_contains(source, "dma_init_struct.memory_base_addr      = (uint32_t)&pixel_buffer");
    TEST_ASSERT_NULL_MESSAGE(strstr(source, "ws2812b_encode_frame"),
                             "reference-style driver should update pixel_buffer and call show(), not rebuild/reinit a frame per write");
    TEST_ASSERT_NULL_MESSAGE(strstr(source, "WS2812B_RESET_SLOTS"),
                             "reference-style driver should latch by stopping low after transfer, not by DMAing extra reset slots");

    free(source);
}

void test_ws2812b_appends_one_byte_of_low_bits_as_stop_tail(void)
{
    char *source = read_driver_source();

    assert_contains(source, "#define WS2812B_STOP_LOW_BITS     8U");
    assert_contains(source, "#define WS2812B_FRAME_SLOTS       (WS2812B_MAX_PIXELS * WS2812B_BITS_PER_PIXEL + WS2812B_STOP_LOW_BITS)");
    assert_contains(source, "static void ws2812b_fill_stop_low_bits(void)");
    assert_contains(source, "pixel_buffer.flat_buffer[WS2812B_MAX_PIXELS * WS2812B_BITS_PER_PIXEL + bit] = WS2812B_DUTY_0;");
    assert_contains(source, "ws2812b_fill_stop_low_bits();");
    TEST_ASSERT_NULL_MESSAGE(strstr(source, "WS2812B_RESET_SLOTS"),
                             "stop tail must be exactly one byte of WS_LOW bits, not a long reset slot tail");

    free(source);
}

void test_ws2812b_app_cycles_red_yellow_blue_at_1hz(void)
{
    char *app_source = read_app_init_source();

    assert_contains(app_source, "#define WS2812B_COLOR_STEP_MS 1000U");
    assert_contains(app_source, "{255U, 0U, 0U}");
    assert_contains(app_source, "{255U, 255U, 0U}");
    assert_contains(app_source, "{0U, 0U, 255U}");
    assert_contains(app_source, "rt_thread_mdelay(WS2812B_COLOR_STEP_MS)");
    TEST_ASSERT_NULL_MESSAGE(strstr(app_source, "LIULI_LIGHT_SEGMENT_STEPS"),
                             "1Hz red/yellow/blue test mode should not interpolate liuli segments");

    free(app_source);
}

void test_ws2812b_uses_grb_order_with_low_zero_margin(void)
{
    char *source = read_driver_source();

    assert_contains(source, "#define WS2812B_BYTE_ORDER_GRB    1");
    assert_contains(source, "#define WS2812B_T0H_NS                        300U");
    assert_contains(source, "#define WS2812B_T1H_NS                        650U");
    assert_contains(source, "WS2812B_BUILD_ASSERT(WS2812B_DUTY_0 == 72U");
    assert_contains(source, "WS2812B_BUILD_ASSERT(WS2812B_DUTY_1 == 156U");
    assert_contains(source, "return ((uint32_t)green << 16U) | ((uint32_t)red << 8U) | blue;");

    free(source);
}

void test_ws2812b_primes_first_bit_before_dma_to_avoid_frame_head_loss(void)
{
    char *source = read_driver_source();

    assert_contains(source, "tmr_output_channel_buffer_enable(WS2812B_TMR, WS2812B_TMR_CHANNEL, FALSE);");
    assert_contains(source, "tmr_channel_value_set(WS2812B_TMR, WS2812B_TMR_CHANNEL, pixel_buffer.flat_buffer[0]);");
    assert_contains(source, "tmr_output_channel_buffer_enable(WS2812B_TMR, WS2812B_TMR_CHANNEL, TRUE);");
    assert_contains(source, "dma_init_struct.memory_base_addr      = (uint32_t)&pixel_buffer.flat_buffer[1]");
    assert_contains(source, "dma_data_number_set(WS2812B_DMA_CHANNEL, WS2812B_FRAME_SLOTS - 1U)");

    free(source);
}

void test_ws2812b_matches_pcb_chain_length(void)
{
    char *header_source = read_driver_header();

    assert_contains(header_source, "#define WS2812B_MAX_PIXELS 1U");

    free(header_source);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ws2812b_keeps_board_pa15_tmr2_mapping_while_using_reference_flow);
    RUN_TEST(test_ws2812b_uses_interrupt_driven_tail_stop_like_reference);
    RUN_TEST(test_ws2812b_timing_constants_are_formula_based_from_effective_timer_clock);
    RUN_TEST(test_ws2812b_refreshes_the_full_configured_chain);
    RUN_TEST(test_ws2812b_uses_reference_pixel_buffer_show_style);
    RUN_TEST(test_ws2812b_appends_one_byte_of_low_bits_as_stop_tail);
    RUN_TEST(test_ws2812b_app_cycles_red_yellow_blue_at_1hz);
    RUN_TEST(test_ws2812b_uses_grb_order_with_low_zero_margin);
    RUN_TEST(test_ws2812b_primes_first_bit_before_dma_to_avoid_frame_head_loss);
    RUN_TEST(test_ws2812b_matches_pcb_chain_length);
    return UNITY_END();
}
