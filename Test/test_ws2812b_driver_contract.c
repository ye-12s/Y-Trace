#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_driver_source(void)
{
    const char *path = Y_TRACE_WS2812B_DRIVER_PATH;
    FILE *file       = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(file, "failed to open drv_ws2812b.c");

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

void test_ws2812b_dma_updates_compare_on_timer_overflow(void)
{
    char *source = read_driver_source();

    assert_contains(source, "DMA_FLEXIBLE_TMR2_OVERFLOW");
    assert_contains(source, "tmr_channel_dma_select(WS2812B_TMR, TMR_DMA_REQUEST_BY_OVERFLOW)");
    assert_contains(source, "tmr_dma_request_enable(WS2812B_TMR, TMR_OVERFLOW_DMA_REQUEST, TRUE)");
    assert_contains(source, "tmr_output_channel_buffer_enable(WS2812B_TMR, WS2812B_TMR_CHANNEL, TRUE)");
    assert_contains(source, "tmr_event_sw_trigger(WS2812B_TMR, TMR_OVERFLOW_SWTRIG)");
    assert_contains(source, "tmr_flag_clear(WS2812B_TMR, TMR_OVF_FLAG)");

    TEST_ASSERT_NULL_MESSAGE(strstr(source, "TMR_DMA_REQUEST_BY_CHANNEL"),
                             "WS2812B DMA must not be triggered by compare events");
    TEST_ASSERT_NULL_MESSAGE(strstr(source, "TMR_C1_DMA_REQUEST"),
                             "WS2812B DMA must not use channel compare DMA requests");
    TEST_ASSERT_NULL_MESSAGE(strstr(source, "DMA_FLEXIBLE_TMR2_CH1"),
                             "WS2812B DMA must listen to TMR2 overflow, not CH1 compare");

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
    assert_contains(source, "WS2812B_BUILD_ASSERT(WS2812B_PWM_PERIOD == 300U");
    assert_contains(source, "WS2812B_BUILD_ASSERT(WS2812B_DUTY_0 == 90U");
    assert_contains(source, "WS2812B_BUILD_ASSERT(WS2812B_DUTY_1 == 180U");
    TEST_ASSERT_NULL_MESSAGE(strstr(source, "#define WS2812B_PWM_PERIOD 300U"),
                             "period should be derived from documented timing formulas");

    free(source);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ws2812b_dma_updates_compare_on_timer_overflow);
    RUN_TEST(test_ws2812b_timing_constants_are_formula_based_from_effective_timer_clock);
    return UNITY_END();
}
