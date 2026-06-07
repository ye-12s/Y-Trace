#include "Drivers/drv_pin.h"
#include "Drivers/drv_lcd.h"
#include "Drivers/drv_soft_iic.h"
#include "Drivers/drv_ws2812b.h"
#include "rtthread.h"
#ifdef Y_TRACE_ENABLE_LVGL
#include "port/lvgl_port.h"
#else
#include "sample/sample.h"
#endif

#define LOG_TAG "app.init"
#define LOG_LVL LOG_LVL_DBG
#include "ulog.h"

static struct _soft_i2c_bus sensor_i2c_bus;
static const pin_t PIN_SENSOR_I2C_SCL = GET_PIN(A, 8);
static const pin_t PIN_SENSOR_I2C_SDA = GET_PIN(C, 9);

#define LIULI_LIGHT_STEP_MS 28U
#define LIULI_LIGHT_SEGMENT_STEPS 40U

static const ws2812b_rgb_t liuli_palette[] = {
    {28U, 0U, 0U},
    {28U, 10U, 0U},
    {24U, 22U, 0U},
    {0U, 28U, 0U},
    {0U, 24U, 24U},
    {0U, 0U, 32U},
    {16U, 0U, 28U},
    {28U, 0U, 14U},
};

static uint8_t liuli_lerp_u8(uint8_t from, uint8_t to, uint8_t step)
{
    const int16_t delta = (int16_t)to - (int16_t)from;
    return (uint8_t)((int16_t)from + (delta * step) / LIULI_LIGHT_SEGMENT_STEPS);
}

static void app_ws2812b_liuli_thread_entry(void *parameter)
{
    (void)parameter;

    const rt_size_t palette_count = sizeof(liuli_palette) / sizeof(liuli_palette[0]);

    while (1) {
        for (rt_size_t index = 0; index < palette_count; index++) {
            const ws2812b_rgb_t from = liuli_palette[index];
            const ws2812b_rgb_t to = liuli_palette[(index + 1U) % palette_count];

            for (uint8_t step = 0U; step < LIULI_LIGHT_SEGMENT_STEPS; step++) {
                const ws2812b_rgb_t color = {
                    .red = liuli_lerp_u8(from.red, to.red, step),
                    .green = liuli_lerp_u8(from.green, to.green, step),
                    .blue = liuli_lerp_u8(from.blue, to.blue, step),
                };

                (void)drv_ws2812b_write_rgb(&color, 1U);
                rt_thread_mdelay(LIULI_LIGHT_STEP_MS);
            }
        }
    }
}

static int app_ws2812b_liuli_start(void)
{
    rt_thread_t thread = rt_thread_create("liuli",
                                          app_ws2812b_liuli_thread_entry,
                                          RT_NULL,
                                          512,
                                          RT_THREAD_PRIORITY_MAX - 2,
                                          20);
    if (thread == RT_NULL) {
        return -RT_ERROR;
    }

    rt_thread_startup(thread);
    return RT_EOK;
}

static int bsp_init(void)
{
    drv_soft_i2c_init(&sensor_i2c_bus, PIN_SENSOR_I2C_SCL, PIN_SENSOR_I2C_SDA, 100, 1000);
    (void)drv_ws2812b_init();
    return 0;
}
INIT_DEVICE_EXPORT(bsp_init);

static int lcd_init(void)
{
    drv_lcd_init();
    LOG_I("LCD initialized.");
    return 0;
}
INIT_COMPONENT_EXPORT(lcd_init);

static int app_init(void)
{
#ifdef Y_TRACE_ENABLE_LVGL
    int result = app_lvgl_port_init();
    if (result != 0) {
        LOG_E("LVGL port initialization failed: %d", result);
        return result;
    }

    LOG_I("LVGL port initialized.");
#else
    int result = lcd_refresh_sample_init();
    if (result != 0) {
        LOG_E("LCD refresh sample initialization failed: %d", result);
        return result;
    }

    LOG_I("LCD refresh sample initialized.");
#endif
    if (app_ws2812b_liuli_start() != RT_EOK) {
        LOG_E("WS2812B liuli light thread start failed.");
    }

    LOG_I("Application initialization completed.");
    return 0;
}
INIT_APP_EXPORT(app_init);
